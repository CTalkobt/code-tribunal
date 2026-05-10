# llm-council

A self-organising multi-LLM code review council. Multiple local models run in
parallel, each with a specialist role. They debate across configurable rounds,
elect an arbiter from among themselves, spawn new specialist roles when they
identify a gap, and prune underperforming analysts by peer vote.

```
┌─────────────────────────────────────────────────────────────────┐
│  Static pool (configured)                                        │
│  security · performance · correctness · style                    │
│                     +                                            │
│  Dynamic pool (self-assembled)                                   │
│  e.g. concurrency · API-contracts · portability · ...           │
└────────────────────────┬────────────────────────────────────────┘
                         │  parallel per round
          ┌──────────────▼──────────────┐
          │  Round N  (all active)       │
          │  · Each sees codebase        │
          │  · Round >1: sees peers'     │
          │    prior positions +         │
          │    decision tree             │
          └──────────────┬──────────────┘
                         │ after each round
          ┌──────────────▼──────────────────────────────┐
          │  Post-round hooks (in order)                 │
          │  1. Score contributions                      │
          │  2. Update decision tree                     │
          │  3. Election vote  (if scheduled)            │
          │  4. Prune vote     (idle analysts)           │
          │  5. Spawn vote     (gap detection)           │
          └──────────────┬──────────────────────────────┘
                         │ after all rounds
          ┌──────────────▼──────────────┐
          │  Elected arbiter (or        │
          │  fallback judge_model)      │
          │  · Full debate transcript   │
          │  · Decision tree            │
          │  · Election history         │
          │  · Contribution scores      │
          │  → per-file corrected code  │
          └──────────────┬──────────────┘
                         │
          ┌──────────────▼──────────────┐
          │  Apply + backup originals   │
          │  Store lesson in SQLite     │
          └─────────────────────────────┘
```

---

## Requirements

- [Ollama](https://ollama.ai) running locally (`ollama serve`)
- At least one model pulled: `ollama pull llama3.2`
- `libcurl4-openssl-dev` and `libsqlite3-dev`

```bash
# Debian/Ubuntu
apt install libcurl4-openssl-dev libsqlite3-dev

# Fedora/RHEL
dnf install libcurl-devel sqlite-devel

# macOS
brew install curl sqlite
```

## Build

```bash
make
make install   # optional: installs to /usr/local/bin/council
```

## Usage

```
council [options] -t <task> <path> [path ...]

  <path> ...     One or more source files or directories
  -t <task>      Task description (required)
  -r <rounds>    Debate rounds 1-16 (overrides config)
  -c <config>    Config file (default: config/council.conf)
  -y             Auto-apply without prompting
  -h             Help
```

### Examples

```bash
# Single file, defaults
./council -t "fix memory leaks and buffer overflows" examples/example.c

# Multiple files, two rounds
./council -r 2 -t "harden error handling" examples/example.c examples/utils.c

# Directory, four rounds, auto-apply
./council -y -r 4 -t "improve correctness and security" src/

# Custom config with mixed models
./council -c myproject.conf -t "optimise hot path" src/render.c src/math.c
```

---

## Configuration (`config/council.conf`)

```ini
# Debate
rounds=4

# Election: first vote after round N, then interval *= backoff, capped at max
election_start=1
election_backoff=2
election_max=8

# Fallback model (used before first election, or on tie)
judge_model=llama3.2

# Dynamic role spawning
spawn_enabled=1        # 1=on, 0=off
spawn_check_round=1    # round after which first spawn vote is held

# Pruning
prune_enabled=1        # 1=on, 0=off
prune_interval=3       # rounds idle before removal vote
prune_grace=2          # immunity rounds for newly spawned analysts
min_analysts=2         # floor: never prune below this
prune_respawn=1        # 1=trigger spawn vote after a prune

# Analyst models (in role order: security, performance, correctness, style)
# Using different models makes elections and spawn/prune votes more meaningful
model=llama3.2
model=deepseek-coder
model=codellama
model=mistral
```

---

## Features

### Multi-round debate with decision tree

Each analyst sees the full codebase and task. From round 2 onward they also
see all peers' previous positions and a compact decision tree (maintained by
the judge_model after each round) summarising what has been agreed, contested,
and conceded. This prevents re-arguing settled points.

### Arbiter election with configurable backoff

After `election_start` rounds, all active analysts vote for a peer (not
themselves) to serve as arbiter, with a one-sentence justification. Majority
wins; ties prefer the correctness analyst; persistent ties fall back to
`judge_model`. After each election the interval doubles (up to `election_max`),
so elections are frequent early and sparse later.

The elected arbiter receives the full debate transcript, decision tree, and
election history, plus a prompt noting they were chosen by peers — giving the
synthesis legitimacy.

### Dynamic role spawning

After `spawn_check_round` and after each election, all active analysts vote on
whether the council has a gap (a class of issue no current role addresses).
Majority YES triggers a role generation call to `judge_model`, which produces:

```
ROLE_NAME: <name>
SYSTEM_PROMPT:
<250-word prompt defining the role, what to look for, how to challenge peers>
END_PROMPT
```

The new analyst is added to the pool from the next round, using `judge_model`
as its model (configurable). Duplicate role names are rejected. A hard cap of
`MAX_DYNAMIC_ROLES` (4) prevents unbounded growth.

Generated prompts are constrained to:
- Cover a class of issue not in the existing four roles
- List 4-6 specific things to examine
- Include peer-challenge instructions
- Require output in the standard `COUNCIL_FILE` format

### Contribution scoring and pruning

Every round, each analyst's contributions are tracked:

| Event | Score |
|---|---|
| Code proposal adopted by arbiter | +2 |
| Round with a code-bearing response | resets idle counter |
| Round with empty or error response | increments idle counter |

When `rounds_since_contrib >= prune_interval` and the analyst is not in its
grace period, a removal vote is held among peers. Majority required to remove;
ties keep the analyst. The floor `min_analysts` prevents the pool from becoming
too small. If `prune_respawn=1`, a successful prune triggers a spawn vote to
fill the gap.

### Multi-file output

The arbiter emits one block per changed file:

```
/* COUNCIL_FILE: src/foo.c */
```c
...complete corrected file...
```
```

Each original is backed up as `<file>.council` before being overwritten.
Files the arbiter determines need no changes are omitted.

### Lesson store

Every run appends to `lessons/council.db` (SQLite). Stored fields include task
hash, file count, round count, analyst pool composition, election outcomes, and
contribution scores. Prior lessons for similar tasks are injected into each
analyst's round-1 context.

---

## Output files

| File | Description |
|---|---|
| `<original>.council` | Backup before each change |
| `council_consensus.txt` | Raw arbiter output if no `COUNCIL_FILE` blocks parsed |
| `lessons/council.db`  | SQLite lesson history |

---

## Architecture

```
src/
  council.h   — all types: Analyst, AnalystStats, ElectionResult,
                PoolEvent, Council, constants
  council.c   — core logic: loading, debate loop, decision tree,
                election, spawning, pruning, scoring, apply
  ollama.c    — libcurl HTTP client, streaming JSON parser
  db.c        — SQLite lesson store
  main.c      — CLI: -t -r -c -y flags, multi-path loading
config/
  council.conf — all tunable parameters
examples/
  example.c   — intentionally buggy C file (demo target)
  utils.c     — second buggy file (multi-file demo)
lessons/      — SQLite DB created at runtime (gitignored)
```

### Analyst lifecycle

```
council_init()     → static analysts created (security/perf/correct/style)
council_run()      → per round:
  analyst_thread() → each active analyst queries Ollama in parallel
  score_contributions() → update idle counters
  update_decision_tree() → summarise round
  run_election()   → if scheduled: vote for arbiter
  run_prune_check() → vote to remove idle analysts
  run_spawn_vote() → vote to add new specialist
arbiter call       → elected or fallback model synthesises
score_adopted()    → check which proposals appear in consensus
council_apply_changes() → write files, backup originals
db_store_lesson()  → persist run summary
```

---

## Limitations (PoC scope)

- Context window: 512KB total across all files. Large projects need chunking or RAG.
- Dynamic analyst model: always uses `judge_model`. A future improvement would
  let the spawn vote also select the model.
- Contribution scoring uses fingerprint substring matching, not semantic similarity.
- No diff output: files are written wholesale. Use `diff <file>.council <file>` to review.
- Lesson retrieval uses FNV task hash; semantic similarity would be more useful at scale.
