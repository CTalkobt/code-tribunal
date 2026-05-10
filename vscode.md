# VS Code Extension: Council IDE

## Overview

A VS Code extension that integrates the council code review system, allowing developers to:
1. Select files/folders and invoke council with a task description
2. Watch the debate unfold in real-time with full transcript and decision tree
3. Review the arbiter's corrections alongside the original code (diffs)
4. Optionally stage changes directly to git or apply them to disk
5. Configure analyst roles, rounds, and spawning/pruning behavior without leaving the editor

## Features

### MVP (14 hours)
- Right-click context menu: "Review with Council"
- Quick input dialog for task description
- Run council subprocess, parse output
- Side-by-side diff viewer for each changed file
- "Apply all" button to write files and back up originals
- Progress notifications (running, success, failure)

### Full Scope (44 hours total)

#### Real-time debate UI (6–8 hrs)
- Webview panel showing live debate transcript
- Decision tree (what's been agreed, contested, conceded) updated after each round
- Analyst positions per round (which analyst said what)
- Election outcomes and voting justifications
- Arbiter synthesis in final round

#### Config UI (3–4 hrs)
- Settings panel in webview: analyst models, rounds, election schedule, spawn/prune toggles
- Disabled during active debate (user must "Stop" to reconfigure)
- Saves preferences to workspace settings

#### Real-time streaming (2–3 hrs)
- Council exposes HTTP API (`--http-port` flag) with JSON endpoints
- Extension polls or subscribes to updates (transcript, decision tree, status)
- Progress bar showing round N of M

#### Git integration (2–3 hrs)
- Option to stage changes instead of direct write
- Show diffs via `git diff` (uses git's diff renderer in VS Code)
- Rollback to original via `git checkout` if needed

#### Testing, polish, error handling (3–4 hrs)
- Handle council errors (ollama offline, syntax errors in input files)
- Graceful shutdown
- Config validation before run

---

## Architecture

### Extension Structure
```
vscode-council/
  src/
    extension.ts       # Main entry point, command registration
    council.ts        # Subprocess management, HTTP polling
    parser.ts         # Parse COUNCIL_FILE blocks from consensus
    git.ts            # Git operations (stage, diff, checkout)
    webview.ts        # Webview creation and message passing
  webview/
    src/
      index.tsx       # Debate UI, config panel, diff viewer
      App.tsx
    package.json
    tsconfig.json
  package.json
  tsconfig.json
```

### Council HTTP API

Council must expose a simple HTTP API when run with `--http-port <port>`:

```
GET /api/status
  → { round: int, total_rounds: int, phase: "debate|election|prune|spawn|arbiter|done", running: bool }

GET /api/transcript
  → { rounds: [ { round: int, analyst: string, role: string, response: string, error: bool }, ... ] }

GET /api/decision_tree
  → { agreed: string[], contested: string[], conceded: string[] }

GET /api/elections
  → [ { round: int, arbiter: string, votes: {...}, reasons: {...} }, ... ]

GET /api/pool_events
  → [ { round: int, type: "spawn|prune", role: string, passed: bool, rationale: string }, ... ]

GET /api/consensus
  → { text: string, parsed_files: [ { path: string, content: string }, ... ] }
```

The extension polls `/api/status` every 1–2 seconds, fetches `/api/transcript` and `/api/decision_tree` when round changes.

### Message Flow

1. **User triggers**: Right-click → "Review with Council"
2. **Extension**: Shows quick input for task, validates files
3. **Extension**: Spawns council subprocess with `--http-port 9001`
4. **Webview opens**: Shows progress, debate UI locked
5. **Polling loop**: Extension polls `/api/status`, pushes updates to webview
6. **Webview renders**: Transcript, decision tree, round progress
7. **User can**:
   - Stop the debate early (kills subprocess)
   - Wait for completion → applies/reviews changes
   - Configure and restart (only when stopped)

---

## UI Layout

### Primary Webview (fullscreen panel)

```
┌─────────────────────────────────────────────────────────────┐
│ Council Review | [⚙ Config] [⏹ Stop] [↻ New Task]            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Round 2 of 4  ███████░░░░░░                              │
│                                                              │
│  📋 TRANSCRIPT                │ 🎯 DECISION TREE            │
│  ──────────────────────────────┼────────────────────────────│
│  [Security]                    │ ✓ Agreed:                  │
│  "Buffer overflow in line 42"  │   - Add bounds check       │
│  "Fix: use memcpy_s instead"   │   - Validate input         │
│                                │                            │
│  [Performance]                 │ ✗ Contested:               │
│  "Loop unroll could help"      │   - Loop optimization      │
│  "Disagree, memory limited"    │   - Use stack or heap?     │
│                                │                            │
│  [Correctness]                 │ ✓ Conceded:                │
│  "Agrees with bounds check"    │   - Performance analyst    │
│                                │     accepted security view │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ ✓ Election held after round 2: [Correctness] elected arbiter │
└──────────────────────────────────────────────────────────────┘
```

### Config Panel (tab in webview, disabled during debate)

```
┌──────────────────────────────────────┐
│ Analyst Models                       │
│ [Security]    llama3.2         [▼]   │
│ [Performance] deepseek-coder   [▼]   │
│ [Correctness] codellama        [▼]   │
│ [Style]       mistral          [▼]   │
│                                      │
│ Debate                               │
│ Rounds:              4       [+] [-] │
│ Election start:      1       [+] [-] │
│ Election backoff:    2       [+] [-] │
│                                      │
│ Spawning    ☑ Enabled               │
│ Pruning     ☑ Enabled               │
│ Min analysts: 2     [+] [-]          │
│                                      │
│ [Save] [Reset to Defaults]           │
└──────────────────────────────────────┘
```

### Diff Panel (after completion)

```
┌──────────────────────────────────────┐
│ Changes: 3 files                     │
├──────────────────────────────────────┤
│ ✓ src/foo.c        [Open] [Discard]  │
│ ✓ src/bar.c        [Open] [Discard]  │
│ ✗ src/skip.c       (no changes)      │
│                                      │
│ [Stage All] [Apply All] [Review All] │
└──────────────────────────────────────┘
```

---

## Implementation Notes

### Config Locking During Debate
- When debate starts, disable config panel
- Show "Stop debate to reconfigure"
- User clicks "Stop" → kills council subprocess
- Config becomes editable
- User clicks "Start" or "New Task" → re-enables debate, clears results

### Parsing Council Output
- Council writes `/* COUNCIL_FILE: <path> */` markers in consensus
- Extension regex parses these blocks to extract corrected code
- Each file in the output is compared against disk to generate diffs

### Git Integration
- `git.ts` provides `stage()`, `diff()`, `checkout()` wrappers
- If user chooses "Stage", extension runs `git add <file>` after write
- VS Code's built-in diff viewer can show staged vs. unstaged

### Error Handling
- Ollama offline → graceful message, suggest `ollama serve`
- Syntax errors in input files → show council stderr
- Council timeout (> 30 min) → offer to kill and retry
- Config validation → warn if model not available in ollama

### Testing Strategy
- Unit tests: parser (COUNCIL_FILE extraction), git operations, HTTP mocking
- Integration test: mock HTTP API, verify webview renders correctly
- Manual: test with real council runs, different file counts, election outcomes

---

## Known Trade-offs

1. **No real-time diff updates** — Diffs only shown after debate completes. Could stream them mid-debate but would add complexity.
2. **Config saved locally** — No cross-machine sync of preferred analysts/rounds. Could add later via settings sync.
3. **Single concurrent debate** — Can't run multiple councils in parallel. Extension enforces one active debate per workspace.
4. **HTTP polling** — Not as efficient as websockets, but simpler to implement. Fine for human-speed debate updates (rounds take minutes).

---

## Effort Breakdown

| Task | Hours | Notes |
|---|---|---|
| Extension scaffolding + commands | 3 | yo code, package setup |
| MVP: file selection, run council, apply | 11 | Subprocess, parsing, file I/O |
| Webview setup (Vite build, messages) | 4 | One-time scaffolding |
| Debate UI (transcript, decision tree) | 6 | React components, styling |
| Config UI | 3 | Form inputs, validation, persistence |
| HTTP polling + state sync | 2 | Simple fetch loop |
| Git integration | 2 | Wrapper around git CLI |
| Error handling + testing | 4 | Edge cases, UX polish |
| **Total** | **~44** | 2 weeks full-time |

---

## Next Steps

1. Design and implement council HTTP API (`--http-port` flag)
2. Set up VS Code extension scaffold + webview boilerplate
3. Implement MVP (context menu, run, apply)
4. Add debate UI (transcript, decision tree)
5. Add config panel with locking
6. Add git integration
7. Test with real council runs

---

## Assumptions

- Council is built and available on PATH (`council` command)
- Ollama is running locally on `http://localhost:11434`
- VS Code 1.85+ (for modern webview + message API)
- Node.js 18+ for extension
