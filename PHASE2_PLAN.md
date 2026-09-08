# Phase 2: Debate Visualization & Advanced Metrics

## Overview
Phase 2 adds visual analytics and deep performance insights. Two major features:
1. **Debate Visualization** - Real-time visual representation of debate structure
2. **Advanced Metrics Dashboard** - Time-series performance analysis

## Feature 1: Debate Visualization

### Purpose
Visual representation of debate progression with analyst cards, argument strength, and decision flow.

### Components

**Analyst Cards**
- Avatar/icon for each analyst
- Model name and provider
- Real-time status (thinking, arguing, idle)
- Confidence score display
- Pruning/elimination state
- Vote count indicator

**Argument Visualization**
- Argument strength bars
- Confidence progressions (line chart)
- Pruning timeline (when analysts eliminated)
- Vote distribution (pie chart)
- Winner announcement with highlight

**Round Flow Diagram**
- Sequential round display
- Analyst participation per round
- Argument quality indicator
- Transition animation

**Technical Details**

Backend API Endpoints:
```
GET /api/debate/{job_id}/visualization
  Returns: {
    analysts: [{id, name, model, status, confidence, votes, eliminated_round}],
    rounds: [{number, arguments: [{analyst_id, text, strength, confidence}], votes: {analyst_id: count}}],
    timeline: [{round, action, timestamp}],
    winner: {analyst_id, confidence}
  }
```

Database Schema:
```sql
CREATE TABLE debate_analysts (
  job_id INTEGER,
  analyst_id TEXT,
  model_name TEXT,
  provider TEXT,
  eliminated_round INTEGER,
  final_votes INTEGER,
  final_confidence FLOAT,
  PRIMARY KEY(job_id, analyst_id)
);

CREATE TABLE debate_arguments (
  job_id INTEGER,
  round_number INTEGER,
  analyst_id TEXT,
  argument TEXT,
  confidence FLOAT,
  argument_strength FLOAT,
  timestamp DATETIME,
  PRIMARY KEY(job_id, round_number, analyst_id)
);
```

Frontend Implementation:
- Analyst cards container with dynamic rendering
- Chart.js line chart for confidence progression
- Chart.js pie chart for final votes
- CSS animations for pruning/elimination
- Responsive grid layout for analyst cards

### Implementation Tasks
1. Add `/api/debate/{job_id}/visualization` endpoint
2. Extract analyst data from ExecutionJob
3. Create AnalystVisualization struct with all metadata
4. Implement JSON serialization for visualization data
5. Create debate-visualization tab in dashboard
6. Add analyst cards HTML/CSS
7. Add confidence progression chart
8. Add vote distribution pie chart
9. Add animation for pruning events
10. Test with live debate execution

### Success Metrics
- ✓ Analyst cards render in <100ms
- ✓ Charts update smoothly with new data
- ✓ Pruning animations play correctly
- ✓ All 4+ analysts visible in responsive layout

---

## Feature 2: Advanced Metrics Dashboard

### Purpose
Deep performance analysis with time-series data, trend visualization, and comparative metrics.

### Components

**Provider Latency Trends**
- Line chart: latency over time (x-axis: time, y-axis: ms)
- Multi-series: p50, p95, p99 percentiles
- Provider-colored lines
- 24-hour moving window

**Success Rate by Provider**
- Grouped bar chart: success % per provider
- Colored bars (green/red)
- Time-range selector (last hour, 24h, 7d, all time)

**Token Usage Distribution**
- Histogram: token counts by debate
- Bin width: 500 tokens
- Color-coded by provider
- Mean/median indicators

**Query Complexity vs Latency**
- Scatter plot: complexity (x-axis) vs latency (y-axis)
- Bubble size: token count
- Color: provider
- Trend line

**Model Performance Comparison**
- Grouped bar chart: latency by model
- Bars grouped by provider
- Legend with model names

**Time-Series Metrics**
- Multi-line chart with:
  - Throughput (queries/minute)
  - Error rate (%)
  - Avg latency (ms)
- Synchronized x-axis (time)
- Dual y-axis (count left, latency/% right)

### Technical Details

Backend API Endpoints:
```
GET /api/metrics/trends?hours=24
  Returns: {
    timestamps: [unix_ms, ...],
    providers: {
      ollama: {latencies: [ms, ...], success_rates: [%, ...]},
      claude: {latencies: [ms, ...], success_rates: [%, ...]},
      google: {latencies: [ms, ...], success_rates: [%, ...]}
    }
  }

GET /api/metrics/percentiles?provider=ollama&hours=24
  Returns: {
    p50: [ms, ...],
    p95: [ms, ...],
    p99: [ms, ...]
  }

GET /api/metrics/tokens?hours=24
  Returns: {
    histogram: {bins: [count, ...], labels: ["0-500", "500-1000", ...]},
    by_provider: {ollama: [counts], claude: [counts], ...}
  }

GET /api/metrics/models?hours=24
  Returns: {
    models: [
      {name, provider, avg_latency, success_rate, count},
      ...
    ]
  }

GET /api/metrics/timeseries?hours=24
  Returns: {
    timestamps: [unix_ms, ...],
    throughput: [queries/min, ...],
    error_rate: [%, ...],
    avg_latency: [ms, ...]
  }
```

Data Collection:
- Store metrics in memory (aggregated by provider, model, hour)
- Maintain 7-day rolling window
- Calculate percentiles on-demand from stored samples
- Update timestamps for trend detection

Database Schema (Optional - for persistence):
```sql
CREATE TABLE metrics_hourly (
  timestamp DATETIME,
  provider TEXT,
  model TEXT,
  latency_p50 FLOAT,
  latency_p95 FLOAT,
  latency_p99 FLOAT,
  success_rate FLOAT,
  query_count INTEGER,
  avg_tokens INTEGER,
  PRIMARY KEY(timestamp, provider, model)
);

CREATE INDEX idx_metrics_timestamp ON metrics_hourly(timestamp);
```

Frontend Implementation:
- 6 chart.js instances (one per metric type)
- Time-range selector (buttons or dropdown)
- Real-time updates via polling /api/metrics/timeseries every 5 seconds
- Responsive grid layout (2 columns on desktop, 1 on mobile)
- Legend and tooltip customization

### Implementation Tasks
1. Create metrics aggregation struct in Metrics.h
2. Add time-series data collection to DebateMetrics
3. Implement percentile calculation (p50, p95, p99)
4. Add `/api/metrics/trends` endpoint
5. Add `/api/metrics/percentiles` endpoint
6. Add `/api/metrics/tokens` endpoint
7. Add `/api/metrics/models` endpoint
8. Add `/api/metrics/timeseries` endpoint
9. Create advanced-metrics tab in dashboard
10. Implement 6 Chart.js instances
11. Add time-range selector UI
12. Add polling/refresh logic
13. Test with 50+ debates across multiple providers
14. Verify percentile calculations

### Success Metrics
- ✓ Charts load within 2 seconds
- ✓ Percentiles calculated correctly
- ✓ Real-time updates every 5 seconds
- ✓ 7-day data retention
- ✓ Responsive on mobile (1 column)
- ✓ All providers visible in comparisons
- ✓ No UI lag with 100+ data points

---

## Database Schema Summary

**New Tables:**
- `debate_analysts` - per-analyst metadata
- `debate_arguments` - per-round arguments
- `metrics_hourly` - hourly aggregated metrics (optional)

**Query Performance:**
- Index on `metrics_hourly(timestamp)` for time-range queries
- Primary keys for fast lookups
- Aggregation on read (no pre-aggregation needed)

---

## Implementation Order

### Step 1: Debate Visualization Backend (Days 1-2)
1. Extend ExecutionJob with analyst tracking
2. Implement visualization data extraction
3. Add `/api/debate/{job_id}/visualization` endpoint
4. Create JSON serialization

### Step 2: Debate Visualization Frontend (Days 2-3)
1. Add debate-visualization tab
2. Create analyst cards component
3. Add confidence/vote charts
4. Implement pruning animations
5. Test end-to-end

### Step 3: Metrics Backend (Days 3-4)
1. Enhance Metrics.h with time-series
2. Implement percentile calculation
3. Add 5 metrics endpoints
4. Create aggregation logic

### Step 4: Metrics Frontend (Days 4-5)
1. Add advanced-metrics tab
2. Create 6 Chart.js instances
3. Add time-range selector
4. Implement polling logic
5. Test with live data

---

## Dependencies
- Phase 1 complete (history, progress tracking)
- Chart.js (already loaded)
- ExecutionJob struct with analyst tracking
- Metrics.h infrastructure (Prometheus metrics)

## Testing Strategy
- Unit tests for percentile calculations
- Integration tests for visualization endpoint
- E2E tests with 50+ debates
- Responsive design tests (mobile, tablet, desktop)
- Performance benchmarks (chart render time <500ms)

## Success Criteria
- All 6 metrics charts render correctly
- Analyst cards responsive and animated
- Time-series data accurate and up-to-date
- No memory leaks with 7-day retention
- All provider comparisons working
- Mobile responsive design
