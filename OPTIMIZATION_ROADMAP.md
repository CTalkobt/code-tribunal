# Code-Tribunal Optimization Roadmap

**Date:** September 2, 2026  
**Status:** Detailed Implementation Plan  
**Codebase:** 4,304 lines of C code, 42+ functions, 5 core modules

---

## Executive Summary

Code-Tribunal is a parallel LLM-based code analysis framework that runs multiple analyst AI agents in concurrent debate rounds to identify security, performance, correctness, and style issues. Current implementation achieves good functionality but faces scalability and maintainability challenges:

- **High Complexity:** 3 functions with CC > 100, averaging 3x industry standards
- **No File-Level Caching:** Re-analyzes unchanged files on every run (90% potential speedup for incremental runs)
- **Limited Parallelization:** Analysts run in parallel, but file loading is sequential
- **Monolithic Backend:** Tightly coupled to Ollama HTTP client; switching providers requires code changes
- **Dead Code:** ~62 unused code paths accumulating technical debt
- **Memory Pressure:** 512KB hard limit on codebase context; no chunking strategy for large projects

This roadmap prioritizes **5 interconnected optimizations** that unlock 8-15x speedup potential while improving code maintainability from CC 100+ down to industry standard (CC < 15 per function).

---

## Current Architecture Analysis

### Module Structure
```
council.c (4,304 lines)
├── Codebase loading (20%)        - council_load_codebase(), collect_files()
├── Debate orchestration (30%)    - council_run(), analyst_thread()
├── Analysis & metrics (25%)      - analyze_code_metrics(), analyze_dead_code(), analyze_code_style()
├── Elections & pruning (15%)     - run_election(), run_prune_check(), run_spawn_vote()
└── Change synthesis (10%)        - council_apply_changes(), parse_structured_changes()

ollama.c (429 lines)
├── Connection pooling            - pool_get_connection(), pool_return_connection()
└── HTTP client                   - ollama_chat()

db.c (308 lines) / main.c (134 lines)
└── Database & configuration
```

### Key Performance Constraints
1. **HTTP Round-Trip Latency:** 0.5–2s per LLM call (network + model startup)
2. **Model Inference:** 10–60s per analyst per round (Ollama, quantized models)
3. **Sequential File Loading:** O(n) disk I/O blocks startup
4. **No Response Memoization:** Identical prompts re-queried every run
5. **Context Window Exhaustion:** Large projects hit 512KB limit; truncation causes missed issues

### Cyclomatic Complexity Baseline
| Function | Lines | CC | Severity |
|----------|-------|----|----|
| `council_run()` | 617 | 144 | CRITICAL |
| `generate_file_summary()` | 164 | 146 | CRITICAL |
| `analyze_code_metrics()` | 272 | 105 | HIGH |
| `council_apply_changes()` | 172 | 37 | MODERATE |
| **Industry Standard** | — | **6-15** | — |

---

## Optimization 1: Caching Layer (File-Level Memoization)

### Overview
Add persistent file-level caching indexed by SHA256 hash to skip re-analyzing unchanged files. For typical incremental runs (10% of files changed), **90% speedup**.

### Current State
- ✓ Response cache exists in `council.h:25` (64-entry in-memory cache)
- ✗ No file-level hash tracking
- ✗ No persistent cache backend (SQLite available but unused for this)
- ✗ Task re-runs entire 5-layer analysis even if codebase unchanged

### Implementation Design

#### Phase 1: File Hash Indexing (Weeks 1-2)
**Files to modify:** `council.h`, `council.c`

**Tasks:**
1. **Add file-hash structure to Council**
   ```c
   // In council.h, add to Council struct:
   typedef struct {
       char filepath[MAX_PATH_LEN];
       char file_hash[65];        // SHA256 hex
       char task_hash[16];        // FNV of task + role
       char summary[384];         // Cached file summary
       int metrics_cc;            // Peak cyclomatic complexity
       int metrics_loc;           // Total lines of code
       // Future: DeadCodeAnalysis, StyleAnalysis, ToolFindings
   } FileCacheEntry;
   
   // In Council struct:
   FileCacheEntry file_cache[MAX_FILES];
   int file_cache_count;
   ```

2. **Implement SHA256 hashing for file content**
   - Compute SHA256 of file content on load (use OpenSSL or small embedded SHA256)
   - Store in cache alongside file metadata
   - Cost: ~1ms per file (negligible vs. analysis time)

3. **Build file-cache lookup function**
   ```c
   int cache_lookup_file(Council *c, const char *filepath, 
                        const char *file_hash,
                        FileCacheEntry *out);  // Returns 1 if found
   ```

4. **Modify `load_file_entry()` to check cache before analyzing**
   - Skip metrics/dead-code/style analysis if file_hash matches cache
   - Reuse cached metrics in analyst prompts

**Estimated Effort:** 200-300 lines of code  
**Risk:** Low (isolated cache logic; no upstream dependencies)

#### Phase 2: Persistent Cache Backend (Weeks 2-3)
**Files to modify:** `council.c`, `db.c`

**Tasks:**
1. **Extend SQLite schema**
   ```sql
   CREATE TABLE file_cache (
       id INTEGER PRIMARY KEY,
       filepath TEXT,
       file_hash TEXT UNIQUE,
       task_hash TEXT,
       mtime INTEGER,                -- File modification time
       analysis_json TEXT,           -- Serialized metrics + findings
       created_at TIMESTAMP,
       accessed_at TIMESTAMP
   );
   CREATE INDEX idx_file_hash ON file_cache(file_hash);
   ```

2. **Implement cache persistence**
   - Write cache to DB after each file analysis
   - Query DB on startup to hydrate in-memory cache
   - Invalidate entry if mtime changed

3. **Add cache eviction policy**
   - LRU: Remove least-recently-accessed entries when DB grows >100MB
   - TTL: Expire entries older than 30 days

**Estimated Effort:** 300-400 lines  
**Risk:** Low (SQLite already used in project)

### Impact Analysis
| Scenario | Benefit | Speedup |
|----------|---------|---------|
| **Incremental run** (90% files unchanged) | Skip 90% of analysis | 9-10x |
| **CI/CD pipeline** (repeated runs) | Amortize over 100 runs | 8-9x |
| **Large project** (1000+ files) | Load cached metrics from DB | 5-6x |
| **Single-file task** | No benefit | 1x (baseline) |

**Cumulative Speedup:** For projects with >20 files, **6-9x average improvement** across multiple runs.

### Success Metrics
- [ ] Cache hit rate > 80% on incremental runs (measure via log output)
- [ ] File analysis time < 5ms for cached entries (verify via audit log)
- [ ] DB size stable at <100MB with LRU eviction
- [ ] Correctness: Cached + fresh analyses produce identical results (regression test)

---

## Optimization 2: Parallel Processing (Multi-File Concurrency)

### Overview
Enable concurrent analysis of multiple files using thread pool. Each thread gets independent analysis state (metrics, findings, blame). For batch jobs with 10+ files, **4-8x speedup**.

### Current State
- ✓ Analysts run in parallel per round (pthreads already used)
- ✓ Connection pooling for HTTP reuse
- ✗ File loading is sequential (single thread)
- ✗ No file-analysis parallelization (metrics, dead-code, style all sequential)
- ✗ No thread pool abstraction (manual thread creation/joining)

### Implementation Design

#### Phase 1: File I/O Parallelization (Weeks 1-2)
**Files to modify:** `council.c`

**Tasks:**
1. **Create thread-pool abstraction**
   ```c
   // In council.h:
   #define MAX_LOAD_THREADS 8  // CPU cores - 2
   
   typedef struct {
       int thread_id;
       void (*work_fn)(void *);
       void *arg;
       int done;
   } ThreadPoolJob;
   
   typedef struct {
       pthread_t threads[MAX_LOAD_THREADS];
       ThreadPoolJob job_queue[MAX_FILES];
       int queue_len;
       pthread_mutex_t lock;
       pthread_cond_t cond;
   } ThreadPool;
   ```

2. **Parallelize `council_load_codebase()`**
   ```c
   // Current: for(i=0; i<file_count; i++) load_file_entry(c, files[i])
   //
   // New: Spawn N worker threads, distribute files
   int collect_files_parallel(Council *c, const char *dir_path, 
                             FileList *list, int max_threads);
   
   // Worker thread function:
   static void* file_load_worker(void *arg) {
       FileLoadTask *task = arg;
       load_file_entry(task->council, task->filepath, task->pos);
       return NULL;
   }
   ```

3. **Thread-safe codebase buffer**
   - Use atomic offset counter for `codebase_pos` instead of mutex
   - Each thread appends to disjoint offset range
   - No lock contention

**Estimated Effort:** 250-350 lines  
**Risk:** Medium (concurrency bugs possible; needs thorough testing)

#### Phase 2: Analysis Parallelization (Weeks 3-4)
**Files to modify:** `council.c`

**Tasks:**
1. **Parallel metrics analysis**
   ```c
   // Create thread per file instead of analyzing all at once
   typedef struct {
       const char *file_content;
       size_t file_len;
       CodeMetrics *result;
   } MetricsTask;
   
   // Spawn N threads: each analyzes one file's metrics
   CodeMetrics** analyze_code_metrics_parallel(Council *c, 
                                             int max_threads);
   ```

2. **Parallel dead-code detection**
   - Similar pattern: one thread per file
   - Aggregate results into per-file DeadCodeAnalysis array

3. **Parallel style analysis**
   - Same pattern for StyleAnalysis

4. **Synchronization**
   - Use pthread_join() to wait for all workers
   - Merge per-file results into single arrays for analyst injection

**Estimated Effort:** 400-500 lines  
**Risk:** Medium (state synchronization; thread-pool reuse across phases)

### Tuning Knobs
```c
// In council.h, add:
int max_load_threads;     // CPU core count - 2 (default: 4)
int max_analysis_threads; // Can differ from load threads
```

**Command-line flag:**
```
./council -t "task" --parallel=8 src/
```

### Impact Analysis
| Scenario | Benefit | Speedup |
|----------|---------|---------|
| **10 files, parallel load** | Avoid N sequential fopen() calls | 4-6x |
| **Metrics + dead-code analysis** | N files analyzed concurrently | 3-4x |
| **Large project (100+ files)** | Network I/O overlaps with disk I/O | 5-8x |
| **Single file** | No benefit (overhead > savings) | 0.9x (slight overhead) |

**Cumulative Speedup:** For codebases with >5 files, **3-6x average improvement** in load/analysis phase.

### Success Metrics
- [ ] File load time for 20-file project < 2s (vs. 8-10s sequential)
- [ ] Thread pool CPU utilization > 70% on multi-core machines
- [ ] Memory peak < 2x baseline (no explosion of analysis objects)
- [ ] Correctness: Parallel + sequential analyses produce identical results
- [ ] No deadlock/race conditions under stress test (1000+ files)

---

## Optimization 3: Break Up High-Complexity Functions

### Overview
Refactor 3 high-complexity functions (CC > 100) into focused helpers. Reduces cognitive load, improves testability, and enables targeted optimizations.

### Current State
- `council_run()`: CC=144, 617 lines (debate orchestration + validation + synthesis)
- `generate_file_summary()`: CC=146, 164 lines (type detection + parsing)
- `analyze_code_metrics()`: CC=105, 272 lines (function detection + parsing)

### Implementation Design

#### 3.1: Refactor `council_run()` (Weeks 2-3)
**Current:** Single monolithic function handling:
1. Codebase loading
2. Tool analysis injection
3. Debate rounds
4. Convergence checking
5. Elections/spawning/pruning
6. Consensus synthesis
7. Validation & self-correction

**New structure:**
```c
int council_run(Council *c) {
    // Phase 1: Grounding (extract tools, findings, metrics)
    AnalysisState *state = council_prepare_grounding(c);
    if (!state) return -1;
    
    // Phase 2: Debate rounds
    if (council_run_debate_rounds(c, state) != 0) goto cleanup;
    
    // Phase 3: Consensus synthesis
    if (council_synthesize_consensus(c, state) != 0) goto cleanup;
    
    // Phase 4: Validation & refinement
    if (council_validate_and_refine(c, state) != 0) goto cleanup;
    
cleanup:
    analysis_state_free(state);
    return 0;
}

// New helper functions (CC < 30 each):
static AnalysisState* council_prepare_grounding(Council *c);
static int council_run_debate_rounds(Council *c, AnalysisState *state);
static int council_synthesize_consensus(Council *c, AnalysisState *state);
static int council_validate_and_refine(Council *c, AnalysisState *state);
```

**Extracted structure:**
```c
typedef struct {
    CodeMetrics *metrics;
    DeadCodeAnalysis *dead_code;
    StyleAnalysis *style;
    ToolFindings *findings;
    BlameCache *blame;
    // ... other grounding data
} AnalysisState;
```

**Effort:** 300-400 lines (mostly copy-paste + refactoring)  
**Risk:** Medium (large refactor; needs comprehensive testing)

#### 3.2: Refactor `generate_file_summary()` (Week 2)
**Current:** Monolithic parser for C and Python files

**New structure:**
```c
static void generate_file_summary(const char *path, const char *content,
                                  char *summary, size_t sum_size) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        generate_summary_unknown(content, summary, sum_size);
        return;
    }
    
    if (is_c_file(ext))
        generate_c_summary(content, summary, sum_size);
    else if (is_py_file(ext))
        generate_py_summary(content, summary, sum_size);
    else
        generate_summary_generic(content, summary, sum_size);
}

// Helper functions (CC < 20 each):
static void generate_c_summary(const char *content, char *out, size_t size);
static void generate_py_summary(const char *content, char *out, size_t size);
static void extract_c_includes(const char *content, char **out, int *count);
static void extract_c_functions(const char *content, char **out, int *count);
// ... etc.
```

**Effort:** 200-250 lines  
**Risk:** Low (isolated changes; natural function grouping)

#### 3.3: Refactor `analyze_code_metrics()` (Weeks 2-3)
**Current:** Single function parsing C code and computing metrics

**New structure:**
```c
CodeMetrics* analyze_code_metrics(const char *source_code, size_t source_len) {
    CodeMetrics *metrics = calloc(1, sizeof(CodeMetrics));
    if (!metrics) return NULL;
    
    int func_index = 0;
    FunctionParser parser = parser_init(source_code, source_len);
    
    while (parser_has_next(&parser) && func_index < MAX_FUNCTIONS) {
        FunctionInfo func = parser_next_function(&parser);
        if (!func.valid) continue;
        
        int cc = calculate_function_complexity(&func);
        int loc = calculate_lines_of_code(&func);
        int params = count_parameters(&func);
        
        store_metrics(&metrics->functions[func_index++], &func, cc, loc, params);
    }
    
    metrics->count = func_index;
    parser_free(&parser);
    return metrics;
}

// New helpers (CC < 15 each):
typedef struct { /* parser state */ } FunctionParser;
static FunctionParser parser_init(const char *code, size_t len);
static int parser_has_next(FunctionParser *p);
static FunctionInfo parser_next_function(FunctionParser *p);
static int calculate_function_complexity(FunctionInfo *func);
static int calculate_lines_of_code(FunctionInfo *func);
static int count_parameters(FunctionInfo *func);
```

**Effort:** 400-500 lines (significant restructuring)  
**Risk:** Medium-High (core parsing logic; requires thorough validation)

### Impact Analysis
| Metric | Before | After | Benefit |
|--------|--------|-------|---------|
| **Max CC** | 146 | 30 | 5x improvement |
| **Avg CC** | 98 | 18 | 5x improvement |
| **Test Coverage** | 40% | 80% | Better bug detection |
| **Maintainability** | Low | High | Faster feature dev |

**Cumulative Speedup:** Indirect; enables targeted optimizations in later phases (parallel analysis, caching). No direct runtime improvement.

### Success Metrics
- [ ] All functions have CC < 30 (industry standard)
- [ ] New helper functions are reusable (used in 2+ places)
- [ ] Unit test coverage > 85% for new functions
- [ ] Regression tests pass (output identical to pre-refactor)
- [ ] Code review approval from team lead

---

## Optimization 4: Dead Code Removal

### Overview
Identify and remove 62+ unused code paths to reduce binary size, improve maintainability, and simplify analysis flow.

### Current State
- Partial dead-code detection implemented in `analyze_dead_code()` for user code analysis
- No systematic cleanup of Council's own dead code
- Suspected orphaned code:
  - Unused role types (enums not referenced)
  - Conditional features (spawning/pruning disabled in some configs)
  - Legacy analysts (old prompt templates)
  - Test/debug functions

### Implementation Design

#### Phase 1: Dead Code Audit (Week 1)
**Tools:** `clang-tools`, static analysis

**Tasks:**
1. **Run static analysis on council.c**
   ```bash
   clang-tidy -checks='*' council.c > dead_code_report.txt
   # Also check for:
   # - Unused function declarations
   # - Unreachable code blocks
   # - Dead assignments
   # - Unused #defines
   ```

2. **Manual code review**
   - Trace each function to find callers
   - Identify ifdef'd code (feature flags)
   - Check for obsolete role/model definitions

3. **Generate removal list**
   - Document each dead code item with reason
   - Verify no external dependencies
   - Create before/after snapshots

**Estimated Effort:** 40-60 hours (careful manual work)

#### Phase 2: Safe Removal (Weeks 2-3)
**Tasks:**
1. **Cluster related dead code**
   - Unused roles → remove enum value, prompt, default
   - Deprecated functions → remove definition + all call sites
   - Legacy features → remove config fields, initialization, handling

2. **Create removal branches** (one per cluster)
   - Small, reviewable commits
   - Each branch has specific test case

3. **Remove with fallback** (if uncertain)
   - Mark with `#if 0 ... #endif` instead of deleting
   - Add comment: "Candidate for removal; reason: X"
   - Revisit in future cleanup pass

### Inventory of Suspected Dead Code
| Item | Type | Reason | Est. LOC |
|------|------|--------|---------|
| Unused role types | enum | Never instantiated | 50 |
| Legacy prompt templates | static | Replaced by new version | 80 |
| Disabled spawning code | func | Feature flag off | 120 |
| Old diff parsing logic | func | Replaced by structured changes | 90 |
| Debug print statements | code | Commented out | 60 |
| **Total** | | | **~400 LOC (10% of council.c)** |

### Impact Analysis
| Benefit | Value |
|---------|-------|
| **Binary size reduction** | ~15-20KB (2-3%) |
| **Compilation time** | ~2-3% faster |
| **Cognitive load** | 10% fewer lines to understand |
| **Maintenance burden** | Fewer obsolete features to maintain |

**Runtime Speedup:** Negligible (<1%)  
**Development Velocity:** +5-10% (faster code review + onboarding)

### Success Metrics
- [ ] 60+ LOC removed (verified by git diff)
- [ ] Zero performance regression (benchmark comparison)
- [ ] All tests pass post-cleanup
- [ ] Code coverage unchanged or improved
- [ ] Binary size reduced by 15KB+

---

## Optimization 5: Backend Abstraction (Multi-Provider Support)

### Overview
Decouple LLM backend from Ollama HTTP client to support Claude API, vLLM, OpenAI, and other providers. Enables cost/performance tradeoffs without code changes.

### Current State
- ✓ Partial support: `api_type`, `api_url`, `api_key` fields in config
- ✓ Some conditional logic for "claude" vs. "ollama" in `council_init()`
- ✗ **Tightly coupled:** `ollama_chat()` hardcoded in analyst_thread, votes, elections
- ✗ **No provider abstraction:** Each provider requires code changes
- ✗ **No batch API support:** Sequential calls for elections/pruning (could batch in OpenAI/Claude)

### Implementation Design

#### Phase 1: Provider Interface (Weeks 1-2)
**Files to create:** `backend.h`, `backend.c`

**Tasks:**
1. **Define provider abstraction**
   ```c
   // In backend.h:
   typedef struct {
       char name[32];         // "ollama", "claude", "openai", "vllm"
       int supports_streaming;
       int supports_batch;    // Can send multiple prompts in 1 call
       int supports_stop_sequences;
   } ProviderCapabilities;
   
   typedef struct {
       const char *provider;     // Determined by api_type
       void *handle;             // Provider-specific state
       ProviderCapabilities caps;
   } LLMBackend;
   
   // Function pointers for provider-specific operations:
   typedef int (*llm_chat_fn)(
       LLMBackend *backend,
       const char *model,
       const char *system_prompt,
       const char *user_msg,
       char *response, size_t resp_size
   );
   
   typedef int (*llm_batch_chat_fn)(
       LLMBackend *backend,
       const char *model,
       int request_count,
       BatchRequest *requests,
       char **responses
   );
   
   struct {
       llm_chat_fn chat;
       llm_batch_chat_fn batch_chat;
       void (*cleanup)(LLMBackend *);
   } backend_ops;
   ```

2. **Create provider factory**
   ```c
   // In backend.c:
   LLMBackend* backend_init(const char *api_type, const char *api_url,
                           const char *api_key);
   int backend_chat(LLMBackend *b, const char *model,
                   const char *system, const char *user,
                   char *response, size_t resp_size);
   int backend_batch_chat(LLMBackend *b, const char *model, ...);
   void backend_free(LLMBackend *b);
   ```

3. **Implement Ollama provider** (refactor existing code)
   ```c
   static int ollama_backend_chat(LLMBackend *b, const char *model,
                                 const char *system, const char *user,
                                 char *response, size_t resp_size) {
       // Move existing ollama_chat() logic here
       // Use curl from backend->handle (connection pool)
   }
   ```

**Estimated Effort:** 400-500 lines  
**Risk:** Low-Medium (new module; careful isolation)

#### Phase 2: Claude API Provider (Weeks 2-3)
**Files to modify:** `backend.c`

**Tasks:**
1. **Implement Claude API provider**
   ```c
   static int claude_backend_chat(LLMBackend *b, const char *model,
                                 const char *system, const char *user,
                                 char *response, size_t resp_size) {
       // Use Claude API with proper headers, request format
       // Support model: "claude-3-opus", "claude-3-sonnet", "claude-3-haiku"
   }
   
   // New dependency: Link against libcurl (already used by Ollama)
   ```

2. **Implement batch support** (if API supports it)
   - Claude API doesn't have batch yet, but plan for future
   - OpenAI Batch API can parallelize election votes

3. **Add cost tracking** (optional)
   ```c
   struct {
       int tokens_in, tokens_out;
       float cost;  // Estimated cost for this run
   } backend_stats;
   ```

**Estimated Effort:** 250-350 lines  
**Risk:** Medium (Claude API errors/timeouts need handling)

#### Phase 3: vLLM Provider (Week 3+, future)
**Rationale:** vLLM has 4x throughput vs. Ollama for concurrent requests (relevant for election voting).

**Implementation sketch:**
```c
static int vllm_backend_chat(...) {
    // vLLM OpenAI-compatible API
    // Same format as OpenAI but different URL
}
```

#### Phase 4: Update Core Code (Weeks 3-4)
**Files to modify:** `council.c`, `council.h`

**Tasks:**
1. **Replace all `ollama_chat()` calls with backend abstraction**
   ```c
   // Before:
   ollama_chat(c->api_type, c->api_url, c->api_key, model,
              system_prompt, user_msg, response, resp_size);
   
   // After:
   backend_chat(c->backend, model, system_prompt, user_msg,
               response, resp_size);
   ```

2. **Add backend instance to Council struct**
   ```c
   struct Council {
       // ...
       LLMBackend *backend;
   };
   ```

3. **Update election voting to use batch API** (if available)
   ```c
   // If provider supports batching, send all votes in one call
   if (c->backend->caps.supports_batch) {
       backend_batch_chat(c->backend, ...);
   } else {
       // Fallback: sequential calls
       for (int i = 0; i < voter_count; i++) {
           backend_chat(c->backend, ...);
       }
   }
   ```

**Estimated Effort:** 300-400 lines (mostly call site replacements)  
**Risk:** Medium (impacts critical code paths; thorough testing required)

### Configuration
**config/council.conf:**
```ini
[api]
type = claude              # or "ollama", "openai", "vllm"
url = https://api.anthropic.com/v1/messages
key = ${CLAUDE_API_KEY}

[models]
judge_model = claude-3-opus
fast_model = claude-3-haiku
standard_models = claude-3-sonnet
```

**CLI override:**
```bash
./council -t "task" --api=claude --model=claude-3-opus src/
```

### Impact Analysis
| Scenario | Benefit | Speedup |
|----------|---------|---------|
| **Switch to Claude API** | Better quality; cost/token reduction | 5-15% more tokens (quality tradeoff) |
| **Use vLLM for elections** | Batch voting (10 voters in 1 call) | 5-8x faster voting phase |
| **Cost optimization** | Use cheaper models for voting | 50-80% cost reduction |
| **Fallback flexibility** | Automatic provider failover | Improved reliability |

**Cumulative Speedup:** Indirect; enables strategic model selection (fast models for voting, better models for arbiter). Plus batching can yield 5-8x on voting rounds.

### Success Metrics
- [ ] Code compiles with/without libcurl
- [ ] Backend factory correctly instantiates providers
- [ ] All provider implementations pass same test suite
- [ ] No performance regression on Ollama path
- [ ] Cost tracking logs correctly (for future analysis)
- [ ] Failover works (Ollama unavailable → falls back to Claude API)

---

## Integration & Staging Strategy

### Phase 0: Foundation (Week 1)
- **Parallel 1:** Dead code audit (low-risk, can proceed independently)
- **Parallel 2:** Backend abstraction skeleton (design/interface only)
- **Parallel 3:** Cyclomatic complexity refactoring plan (detailed breakdown)

### Phase 1: Core Optimizations (Weeks 1-3)
```
Week 1:
  ✓ Dead code audit + removal (safe cleanup)
  ✓ File hash indexing (caching layer foundation)
  ✓ Backend interface design (no implementation yet)

Week 2:
  ✓ SQLite cache persistence
  ✓ Parallel file loading (thread pool)
  ✓ Refactor generate_file_summary() (low-complexity function)

Week 3:
  ✓ Parallel metrics/dead-code/style analysis
  ✓ Refactor analyze_code_metrics() (medium-complexity)
  ✓ Ollama provider wrapper (backend abstraction implementation)
```

### Phase 2: Major Refactoring (Weeks 3-4)
```
Week 3-4:
  ✓ Refactor council_run() (high-complexity, high-impact)
  ✓ Claude API provider implementation
  ✓ Batch voting support (if provider supports it)
  ✓ vLLM provider (future, after Claude is stable)
```

### Phase 3: Testing & Validation (Weeks 4-5)
```
Week 4-5:
  ✓ Regression testing (output diff against baseline)
  ✓ Performance benchmarking (measure speedups claimed)
  ✓ Integration testing (all optimizations together)
  ✓ Large codebase stress testing (1000+ files)
  ✓ Cross-provider validation (Ollama vs. Claude vs. vLLM)
```

### Dependency Graph
```
Dead Code Removal (Phase 0)
    ↓
File Hash Indexing (Phase 1, Week 1)
    ├→ SQLite Persistence (Week 2)
    ├→ Parallel File Loading (Week 2) ← Depends on: Thread pool setup
    └→ Parallel Analysis (Week 3) ← Depends on: Thread pool, cache

Backend Abstraction (Phase 1-2)
    ├→ Ollama Wrapper (Week 3)
    ├→ Claude Provider (Week 3-4)
    └→ Batch Voting (Week 4) ← Depends on: Claude provider stable

Complexity Refactoring (Phase 1-2)
    ├→ generate_file_summary() (Week 2) ← Can start independently
    ├→ analyze_code_metrics() (Week 2-3)
    └→ council_run() (Week 3-4) ← Depends on: Foundation solid
```

---

## Risk Management

### High-Risk Items

#### 1. Refactoring `council_run()` (Week 3-4)
**Risk:** Large refactor of critical path; risk of logic bugs, subtle behavioral changes

**Mitigation:**
- Extract each phase incrementally; test each separately
- Run full regression suite after each extraction
- Diff output against baseline (must be identical)
- Add detailed comments to explain control flow
- Code review by 2+ team members

#### 2. Parallel File Analysis (Week 3)
**Risk:** Race conditions, deadlocks, data corruption from concurrent access

**Mitigation:**
- Use thread-safe data structures (atomic counters for offsets)
- No shared state between worker threads (each analyzes disjoint file)
- Stress test with 1000+ files; run under ThreadSanitizer
- Fallback: disable parallelism with `--parallel=1` flag

#### 3. Backend Abstraction (Week 3-4)
**Risk:** Incomplete provider implementations; missing error handling

**Mitigation:**
- Start with Ollama wrapper (proven code path)
- Add verbose logging for provider/API calls
- Implement timeout + retry logic for all providers
- Provider selection: require explicit `--api=claude` (don't auto-switch)
- Keep Ollama as default; others are opt-in

### Medium-Risk Items
- SQLite persistence: Use in-memory DB as fallback if persistence fails
- Cyclomatic complexity refactoring: Extensive unit testing of extracted functions
- Batch voting API: Fall back to sequential if provider doesn't support batching

### Low-Risk Items
- Dead code removal (no runtime impact)
- File hash indexing (read-only; cache miss → recompute)
- Parallel file loading (can disable with `--parallel=1`)

---

## Rollout Strategy

### Stage 1: Early Adopters (Week 1-2)
- Deploy dead code removal + file hash indexing to internal CI/CD
- Measure speedup on real workloads (existing projects)
- Collect feedback on false positives (cache hits that shouldn't occur)

### Stage 2: Stabilization (Week 3)
- Deploy parallel file loading + analysis
- Run stress tests on large codebases (1000+ files)
- Benchmark all optimizations together
- Update documentation with new CLI flags

### Stage 3: Backend Flexibility (Week 4)
- Deploy Claude API provider (beta, feature-flagged)
- Parallel Ollama + Claude runs (cost/quality comparison)
- Gradual migration to Claude if benchmarks favor it

### Stage 4: Production (Week 5)
- Default to combined optimization stack
- Ollama still supported (for CPU-only environments)
- vLLM support ready (not default yet)

### Stability Gates
**Each stage requires:**
1. ✓ Regression test pass rate > 99%
2. ✓ Performance benchmark shows expected speedup (±10%)
3. ✓ No memory leaks (valgrind clean)
4. ✓ Documentation updated
5. ✓ Team sign-off (code review + QA)

---

## Expected Outcomes

### Performance Targets
| Optimization | Isolated Speedup | Cumulative |
|--------------|------------------|-----------|
| Caching layer | 6-9x (incremental) | **6-9x** |
| + Parallel processing | 3-6x (batch) | **12-20x** |
| + Backend abstraction | 2-3x (elections batch) | **20-30x** |
| + Complexity refactoring | +10% (future optimizations enabled) | **22-33x** |

**Conservative Estimate:** 8-15x speedup for typical multi-file projects  
**Best Case:** 30x+ speedup for large, repetitive workloads (CI/CD)  
**Worst Case (single file, fresh run):** 0.95x (slight overhead from caching)

### Code Quality Improvements
| Metric | Before | After |
|--------|--------|-------|
| **Max Cyclomatic Complexity** | 146 | 30 |
| **Avg CC per function** | 18 | 8 |
| **Dead code lines** | 400 | 0 |
| **Test coverage** | 40% | 85% |
| **Maintainability Index** | 65 | 85 |

### Feature Enablement
- ✓ Support for Claude, OpenAI, vLLM backends
- ✓ Batch voting (5-8x faster elections on some providers)
- ✓ Incremental caching (90% speedup on repeated runs)
- ✓ Parallel analysis (4-8x for large projects)
- ✓ Pluggable provider architecture (easy to add new LLM services)

---

## Success Metrics & Validation

### Metrics to Track
1. **End-to-end runtime** (main metric)
   - Measure on 10-file, 50-file, 100-file codebases
   - Both fresh runs and incremental (90% unchanged)
   - Different backends (Ollama, Claude, vLLM)

2. **Phase breakdown**
   - Load time (file I/O + parsing)
   - Analysis time (metrics, dead code, style)
   - Debate time (rounds)
   - Election time (voting)
   - Synthesis time (arbiter)

3. **Resource usage**
   - Memory peak
   - CPU utilization
   - Network bandwidth
   - API cost (if using paid backends)

4. **Quality metrics**
   - Cache hit rate (% files skipped)
   - Thread pool efficiency (actual vs. expected speedup)
   - Correctness (output diff vs. baseline)

### Regression Test Suite
```c
// tests/test_caching.c
void test_cache_hit_identical_file() { /* verify same output */ }
void test_cache_miss_modified_file() { /* verify reanalyzed */ }
void test_parallel_vs_sequential() { /* verify same output */ }

// tests/test_backends.c
void test_ollama_backend() { /* basic chat */ }
void test_claude_backend() { /* API call */ }
void test_backend_fallback() { /* Ollama down → Claude */ }

// tests/test_complexity.c
void test_generate_file_summary() { /* isolated CC test */ }
void test_analyze_code_metrics() { /* isolated CC test */ }
void test_council_run_phases() { /* each phase independently */ }
```

### Acceptance Criteria
- [ ] All regression tests pass
- [ ] Speedup within 5% of projected targets
- [ ] No memory leaks (valgrind clean)
- [ ] Code review approved (2+ reviewers)
- [ ] Documentation complete & accurate
- [ ] Team confident in production deployment

---

## Timeline & Resource Allocation

### Effort Breakdown
| Component | Weeks | Person-Days | Notes |
|-----------|-------|-------------|-------|
| **Dead code audit** | 1 | 5 | Careful manual review |
| **Caching layer (Phase 1)** | 2 | 5 | File hashing + lookup |
| **Caching layer (Phase 2)** | 1 | 4 | SQLite persistence |
| **Parallel processing (Phase 1)** | 2 | 6 | Thread pool setup |
| **Parallel processing (Phase 2)** | 2 | 7 | Concurrent analysis |
| **Complexity refactoring** | 3 | 10 | Large refactors |
| **Backend abstraction (Phase 1)** | 2 | 6 | Interface design |
| **Backend abstraction (Phase 2-3)** | 2 | 8 | Provider implementations |
| **Testing & validation** | 2 | 8 | Regression + stress tests |
| **Documentation** | 1 | 3 | API docs + user guide |
| **Contingency (15%)** | — | 11 | Unexpected issues |
| **Total** | **18 weeks** | **73 person-days** | ~18 weeks / 1 engineer |

**Parallel tracks possible:**
- Dead code removal (1 person, independent)
- Backend abstraction design (1 person, parallel to other work)
- Complexity refactoring plan (can overlap)

**Critical path:** Caching → Parallel → Refactoring → Backend → Testing

---

## Phase 6: C++ Migration & Architecture Modernization

### Overview
After core optimizations stabilize (end of Week 8), migrate Code-Tribunal from C to modern C++17. This phase refactors the monolithic architecture into focused classes with single responsibility, leveraging RAII for resource safety and STL containers for flexibility.

**Timeline:** Weeks 9-16 (can overlap with optimization final testing)  
**Duration:** 6-8 weeks  
**Effort:** ~50-60 person-days  
**Prerequisite:** Optimizations 1-5 complete and tested

### Rationale
- **Current State:** 50+ files, monolithic council.c (5000+ LOC), manual memory management, tight coupling
- **Pain Points:** Hard to unit test (monolithic logic), difficult to extend (tight coupling to Ollama), high cognitive load (5000-LOC functions), memory safety issues (manual malloc/free)
- **Goal:** Clean architecture with single responsibility per class, RAII-based safety, better testability

### Key Deliverables
1. **Phase 0 (Build):** Parallel C/C++ infrastructure (~3-4 days)
2. **Phase 1 (Foundation):** Types, storage abstraction, utilities (~1-2 weeks)
3. **Phase 2 (LLM):** Pluggable client interfaces (~1-2 weeks)
4. **Phase 3 (Core):** Council, Analyst, Pool, Election classes (~2-3 weeks)
5. **Phase 4 (UI):** Web server, TUI, query classifier (~1-2 weeks)
6. **Phase 5 (Integration):** Full testing, deprecate C version (~1-2 weeks)

### Target Architecture
```
src/
  core/
    ├── Council.h/cpp       (debate orchestration, facade)
    ├── Analyst.h/cpp       (analyst state machine)
    ├── Election.h/cpp      (voting logic)
    ├── Pool.h/cpp          (analyst collection management)
    └── types.h             (shared enums, constants)

  llm/
    ├── LLMClient.h/cpp     (abstract base class)
    ├── OllamaClient.h/cpp  (Ollama HTTP implementation)
    ├── ClaudeClient.h/cpp  (Claude API implementation)
    ├── ResponseParser.h/cpp (JSON parsing)
    └── Cache.h/cpp         (response memoization with TTL)

  storage/
    ├── Database.h/cpp      (SQLite RAII wrapper)
    ├── LessonStore.h/cpp   (lesson persistence)
    ├── FileManager.h/cpp   (file I/O with RAII)
    └── Config.h/cpp        (INI parser)

  ui/
    ├── WebServer.h/cpp     (HTTP server abstraction)
    ├── TUIManager.h/cpp    (terminal UI)
    ├── QueryClassifier.h/cpp (intent detection)
    └── UIHelpers.h/cpp     (shared UI utilities)

  util/
    ├── Logging.h/cpp       (structured logging, thread-safe)
    ├── Concurrent.h/cpp    (thread pool wrapper)
    ├── Hash.h/cpp          (SHA256, FNV hashing)
    └── String.h/cpp        (string utilities)

  main.cpp                   (CLI entry point)
```

### Benefits Over Current C Implementation
| Aspect | C (Current) | C++ (Proposed) |
|--------|-----------|----------------|
| **Type Safety** | Weak (`#define`, NULL checks) | Strong (`enum class`, `std::optional<T>`) |
| **Memory Management** | Manual (malloc/free) | Automatic (RAII, unique_ptr, lock_guard) |
| **Collections** | Fixed arrays (MAX_MODELS=16) | Dynamic (std::vector<T>, std::map) |
| **Abstraction** | None (monolithic council.c) | Proper (classes, polymorphism) |
| **Code Reuse** | Low (duplicate patterns) | High (templates, inheritance) |
| **Testability** | Difficult (global state) | Easy (mockable interfaces) |
| **Cognitive Load** | High (5000-LOC functions) | Low (avg 300-LOC classes) |

### Integration with Optimizations 1-5
Each optimization translates cleanly to C++:
- **Optimization 1 (Caching):** `std::map<std::string, CacheEntry>` with `std::chrono::duration` for TTL
- **Optimization 2 (Parallelization):** `std::thread`, `std::mutex`, `std::lock_guard` (cleaner than pthreads)
- **Optimization 3 (Complexity):** Split into focused classes (single responsibility by design)
- **Optimization 4 (Dead Code):** Auto-accomplished through better encapsulation (private members)
- **Optimization 5 (Backend):** Virtual base class `LLMClient` with concrete implementations

### Build Strategy: Parallel Binaries
During transition:
- `council` — C version (default Phases 0-5)
- `council_cpp` — C++ version (beta, testable)

Week 14: All tests pass on C++; switch Makefile default to `council_cpp`  
Week 16: Archive C version to `main-c` branch; delete C files from main

### Testing Requirements (per CLAUDE.md)
All new C++ code requires comprehensive testing:

**Unit Tests (>90% coverage):**
- `test_analyst.cpp`: State transitions, contribution scoring, serialization
- `test_pool.cpp`: Add/remove analysts, parallel execution, role queries
- `test_voting.cpp`: Election logic, spawn votes, prune votes, tiebreakers
- `test_llm_clients.cpp`: Ollama, Claude, mock clients, cache hit/miss, TTL

**Integration Tests:**
- `test_integration_cpp.cpp`: Full debate loop, output regression vs. C version, concurrency

**Memory & Concurrency Tests:**
- ThreadSanitizer: Compile with `-fsanitize=thread`; detect race conditions
- Valgrind + ASan: Detect memory leaks and buffer overflows

### Success Criteria
- [ ] C++ version compiles with `-Wall -Wextra -Werror`
- [ ] All existing tests pass on C++ binary
- [ ] Code coverage >85%
- [ ] Performance within 5% of C version
- [ ] No memory leaks (Valgrind + ASan clean)
- [ ] Debate output bit-for-bit identical to C version
- [ ] Single responsibility: no class >1000 LOC; avg ~300 LOC
- [ ] Documentation updated (C++ focus; C archived)

### Risk Mitigation
| Risk | Mitigation |
|------|-----------|
| C++17 compiler unavailable | Specify GCC 7+, Clang 5+; CI enforces |
| Performance regression | Profile both versions; optimize hot paths |
| Concurrent access bugs | ThreadSanitizer + stress tests (1000+ files) |
| Subtle logic bugs | Regression tests must match C output exactly |

### Rollback Plan
- Keep C version in git history (`main-c` branch)
- Parallel binaries allow A/B comparison
- If critical bug: revert to C, fix, re-attempt
- Expected rollback effort: ~1 day

### Timeline
```
Week 9 (Phase 0): Build infrastructure (Makefile, C↔C++ interop)
Week 10-11 (Phase 1): Foundation (types, storage, utilities)
Week 11-12 (Phase 2): LLM clients (abstract interface, implementations, cache)
Week 12-14 (Phase 3): Core logic (Analyst, Pool, Election, Council)
Week 14-15 (Phase 4): UI layer (WebServer, TUI, QueryClassifier)
Week 15-16 (Phase 5): Integration testing, C version deprecation
```

---

## Conclusion

This optimization roadmap delivers **8-15x speedup** (conservative) to **30x+ speedup** (best case) through six interconnected phases:

1. **Caching Layer:** Skip re-analyzing unchanged files (6-9x incremental speedup)
2. **Parallel Processing:** Concurrent file I/O + analysis (3-6x batch speedup)
3. **Complexity Reduction:** Refactor high-CC functions (improves maintainability; enables future opts)
4. **Dead Code Removal:** Clean up unused code (reduces cognitive load; simpler testing)
5. **Backend Abstraction:** Support multiple LLM providers (enables strategic model selection; batch voting)
6. **C++ Migration:** Modernize architecture with RAII, single responsibility, proper encapsulation (enables future growth; RAII safety)

**Key principles:**
- **Incremental delivery:** Each optimization works independently; cumulative gains
- **Backward compatible:** Fallback modes for all features (disable parallelism, caching, etc.)
- **Rigorously tested:** Regression suite ensures output correctness
- **Well-documented:** Clear interfaces for future extensions (new providers, new analysis types)

**Next steps:**
1. Prioritize dead code removal (low risk, immediate value)
2. Implement caching + parallelization in parallel (high impact)
3. Refactor complexity hotspots (enables future optimizations)
4. Introduce backend abstraction (flexibility + future-proofing)

**Estimated impact timeline:**
- **End of Week 2:** 2-3x speedup (caching + basic parallelization)
- **End of Week 4:** 8-12x speedup (all core optimizations)
- **End of Week 5:** 12-15x speedup (testing + refinement)

This roadmap positions code-tribunal as a **scalable, maintainable, and extensible** code analysis platform capable of handling large multi-file projects with multiple backend options and sub-minute analysis turnaround.
