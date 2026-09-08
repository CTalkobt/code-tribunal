# Enhanced Dashboard Implementation

## Phase 1: Query History & Search (Foundation)

### Backend Changes
1. Add query history tracking to ExecutionJob
2. Implement `/api/history` endpoint
3. Implement `/api/history/search` endpoint
4. Add simple in-memory history storage (session-based)

### Frontend Changes
1. New "Query History" tab
2. History table with columns: timestamp, query, provider, duration, status
3. Search input with live filtering
4. Sort controls
5. Re-run button for past queries

## Phase 2: Real-time Debate Flow

### Backend Changes
1. Track debate progress in ExecutionJob
2. Add debate state updates (rounds, analysts, votes)
3. Implement `/api/debate/{job_id}/progress` endpoint
4. Optional: WebSocket support for live updates

### Frontend Changes
1. New "Debate Progress" tab (shown during running debate)
2. Round counter and progress bar
3. Analyst status list
4. Argument display area
5. Live update polling (500ms intervals)

## Phase 3: Debate Visualization

### Frontend Changes
1. SVG or Canvas-based visualization
2. Analyst nodes/cards
3. Argument flow diagram
4. Pruning/elimination highlights
5. Winner animation

## Phase 4: Advanced Metrics

### Frontend Changes
1. Enhanced metrics tab
2. Time-series charts (latency trends, success rate)
3. Provider comparison charts
4. Percentile calculations (p50, p95, p99)
5. Date range selector

## Phase 5: Configuration Panel

### Backend Changes
1. Implement `/api/config` GET endpoint
2. Implement `/api/config/preview` POST endpoint
3. Implement `/api/config/apply` POST endpoint

### Frontend Changes
1. New "Settings" tab
2. Form controls for all config options
3. Preview section showing validated config
4. Apply/Revert buttons
5. Error highlighting for invalid values
