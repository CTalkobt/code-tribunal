# Code-Tribunal UI Enhancement Plan

## Overview
Comprehensive dashboard redesign with 5 major feature additions for production-grade debate visualization and analysis.

## Architecture

### Tab Structure
```
Dashboard Root (/)
├── Query Interface (existing, enhanced)
├── Real-time Debate (NEW)
├── Debate History & Search (NEW)
├── Advanced Metrics (NEW)
└── Configuration (NEW)
```

### New API Endpoints

#### Debate Progress Tracking
```
GET /api/debate/{job_id}/progress
  Returns: {
    rounds_completed: int,
    current_round: int,
    total_rounds: int,
    analyst_count: int,
    analyst_states: [{id, status, argument, confidence}],
    timeline: [{round, action, timestamp}]
  }
```

#### Query History
```
GET /api/history?limit=50&offset=0
  Returns: [{job_id, query, timestamp, provider, status, duration_ms}]

GET /api/history/search?q=query_text
  Returns: [matching queries]
```

#### Configuration Access
```
GET /api/config
  Returns: current configuration

POST /api/config/preview
  Test configuration without applying

POST /api/config/apply
  Apply new configuration (with validation)
```

## Features

### 1. Real-time Debate Flow
**Purpose:** Watch debates unfold with live updates

**Components:**
- Analyst timeline (visual list)
- Round progress indicator
- Live argument display
- Voting visualization
- Winner announcement

**Technical Implementation:**
- SSE (Server-Sent Events) or WebSocket for live updates
- Debate state tracking in ExecutionJob
- New HTTP endpoint for progress polling/streaming

**Database Schema Addition:**
```sql
CREATE TABLE debate_rounds (
  job_id INTEGER,
  round_number INTEGER,
  analyst_id TEXT,
  argument TEXT,
  confidence FLOAT,
  timestamp DATETIME
);
```

### 2. Debate Visualization
**Purpose:** Visual representation of debate structure

**Components:**
- Analyst cards (avatar, name, model)
- Argument strength indicators
- Pruning/elimination animation
- Round-by-round flow diagram
- Decision tree view

**Chart.js Elements:**
- Analyst participation timeline
- Confidence score progression
- Vote distribution pie chart
- Pruning/survival rates

### 3. Query History & Search
**Purpose:** Browse and replay past debates

**Components:**
- Sortable history table (date, query, provider, duration, status)
- Full-text search of query text
- Filters (provider, date range, success/failure)
- Quick re-run button
- Result preview on hover

**Database Schema:**
```sql
CREATE TABLE query_history (
  job_id INTEGER PRIMARY KEY,
  query TEXT,
  provider TEXT,
  model TEXT,
  timestamp DATETIME,
  duration_ms INTEGER,
  success BOOLEAN,
  result TEXT,
  FOREIGN KEY(job_id) REFERENCES debates(job_id)
);

CREATE INDEX idx_query_history_timestamp ON query_history(timestamp);
CREATE FULLTEXT INDEX idx_query_history_text ON query_history(query);
```

### 4. Advanced Metrics Dashboard
**Purpose:** Deep performance analysis

**Charts:**
- Provider latency trends (over time, p50/p95/p99)
- Success rate by provider (line chart)
- Token usage distribution (histogram)
- Query complexity vs latency (scatter)
- Model performance comparison (grouped bar)
- Time-series metrics (throughput, errors)

**Metrics Stored:**
- Per-debate: latency, token count, success flag
- Aggregated: by provider, model, query type
- Time windows: last hour, 24h, 7 days, all time

### 5. Configuration Panel
**Purpose:** Adjust system behavior from UI

**Controls:**
- Provider selection (dropdown)
- Rounds slider (1-20)
- Model multi-select
- Timeout adjustment
- Retry policy preset (aggressive/moderate/conservative)
- Election/spawn/prune toggles
- Ollama endpoint management

**Workflow:**
1. User adjusts settings
2. Preview button tests config (calls `/api/config/preview`)
3. Apply button persists (calls `/api/config/apply`)
4. Error handling for invalid configs (red highlights)
5. Revert to default button

## Implementation Order

### Phase 1: Foundation (Highest ROI)
1. Query History & Search (low effort, high value)
   - Database schema
   - History table UI
   - Search functionality
   
2. Real-time Debate Flow (medium effort)
   - Debate progress tracking
   - Live update mechanism
   - Analyst timeline UI

### Phase 2: Visualization
3. Debate Visualization (medium effort)
   - Analyst cards
   - Round progress
   - Argument display

4. Advanced Metrics (medium effort)
   - Time-series data collection
   - Chart generation
   - Trend analysis

### Phase 3: Control
5. Configuration Panel (medium effort)
   - Config form
   - Validation UI
   - Preview/apply workflow

## Database Changes

### New Tables
- `query_history` - track all queries
- `debate_rounds` - round-by-round data (optional, for replay)
- `analyst_arguments` - argument text and metadata (optional)

### Migrations
- Add columns to existing tables if needed
- Create indexes for search performance
- Populate history from existing jobs

## Frontend Technologies

- **Charts:** Chart.js (already loaded)
- **Live Updates:** Fetch polling or Server-Sent Events
- **Search:** Client-side filtering + server fulltext search
- **Forms:** HTML5 form validation + custom validation

## Security Considerations

- Configuration changes require validation
- No direct file access from config panel
- Sanitize search inputs
- Rate-limit history queries
- Audit config changes

## Performance

- Paginate history results (50 per page)
- Cache metrics calculations
- Lazy-load chart data
- Debounce search input
- Use indexes for fulltext search

## Success Metrics

- ✓ Query history searchable and sortable
- ✓ Real-time debate updates visible within 500ms
- ✓ Metrics dashboard loads within 2 seconds
- ✓ Configuration changes apply without restart
- ✓ Visualization renders smoothly with 50+ queries
