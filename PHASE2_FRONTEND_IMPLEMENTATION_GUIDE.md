# Phase 2 Frontend Implementation Guide

Complete HTML, CSS, and JavaScript specifications for Debate Visualization and Advanced Metrics Dashboard tabs.

## Overview

Phase 2 adds two new dashboard tabs:
1. **Debate Visualization** - Real-time analyst cards, confidence progression, vote distribution
2. **Advanced Metrics Dashboard** - 6 charts for deep performance analysis

Both tabs integrate with Phase 2 backend API endpoints and complement Phase 1's query history and progress tracking.

## Tab Navigation Structure

Update `get_dashboard_html()` to include new tabs in the navbar:

```html
<div class="tabs">
    <button class="tab-btn active" onclick="switchTab('query')">Query Interface</button>
    <button class="tab-btn" onclick="switchTab('debate-progress')">Debate Progress</button>
    <button class="tab-btn" onclick="switchTab('history')">Query History</button>
    <button class="tab-btn" onclick="switchTab('visualization')">Visualization</button>
    <button class="tab-btn" onclick="switchTab('advanced-metrics')">Advanced Metrics</button>
    <button class="tab-btn" onclick="switchTab('metrics')">Metrics & Performance</button>
</div>
```

---

## Tab 3: Debate Visualization

### Purpose
Real-time visualization of debate structure with analyst cards, argument strength, confidence progression, and vote distribution.

### HTML Structure

```html
<!-- Debate Visualization Tab -->
<div id="visualization" class="tab-content grid-2">
    <!-- Left Column: Analyst Cards -->
    <div class="panel">
        <h2>Analyst Cards</h2>
        <div id="analyst-cards-container" style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
            <!-- Dynamically generated analyst cards -->
        </div>
    </div>

    <!-- Right Column: Charts -->
    <div class="panel">
        <h2>Debate Metrics</h2>
        <div id="confidence-chart-container" class="chart-container">
            <canvas id="confidenceChart"></canvas>
        </div>
        <div id="votes-chart-container" class="chart-container">
            <canvas id="votesChart"></canvas>
        </div>
    </div>

    <!-- Round Timeline -->
    <div class="panel" style="grid-column: 1 / -1;">
        <h2>Round Timeline</h2>
        <div id="round-timeline" style="display: flex; gap: 10px; overflow-x: auto; padding: 10px;">
            <!-- Dynamically generated round indicators -->
        </div>
    </div>
</div>
```

### CSS Styling

```css
/* Analyst Card Styles */
.analyst-card {
    background: #2a2a2a;
    border: 1px solid #444;
    border-radius: 6px;
    padding: 12px;
    transition: all 0.3s;
    position: relative;
}

.analyst-card:hover {
    background: #333;
    border-color: #0066cc;
    box-shadow: 0 0 8px rgba(0, 102, 204, 0.3);
}

.analyst-card.eliminated {
    opacity: 0.5;
    border-left: 3px solid #f44336;
}

.analyst-card.winner {
    border-left: 3px solid #4caf50;
    box-shadow: 0 0 12px rgba(76, 175, 80, 0.4);
}

.analyst-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 8px;
}

.analyst-name {
    font-weight: 600;
    font-size: 13px;
    color: #0066cc;
}

.analyst-model {
    font-size: 11px;
    opacity: 0.7;
    color: #aaa;
}

.analyst-status {
    display: inline-block;
    padding: 2px 6px;
    background: #0066cc;
    color: white;
    border-radius: 3px;
    font-size: 10px;
    font-weight: 500;
}

.analyst-status.idle {
    background: #444;
}

.analyst-confidence-bar {
    background: #1a1a1a;
    border-radius: 3px;
    height: 4px;
    margin-top: 8px;
    overflow: hidden;
}

.analyst-confidence-fill {
    background: linear-gradient(90deg, #ffb74d, #4caf50);
    height: 100%;
    width: 0%;
    transition: width 0.5s ease;
}

.analyst-votes {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-top: 8px;
    font-size: 12px;
}

.vote-badge {
    background: #0066cc;
    color: white;
    padding: 2px 8px;
    border-radius: 3px;
    font-weight: 600;
}

/* Round Timeline Styles */
.round-indicator {
    min-width: 80px;
    padding: 10px;
    background: #2a2a2a;
    border: 1px solid #444;
    border-radius: 4px;
    text-align: center;
    cursor: pointer;
    transition: all 0.3s;
}

.round-indicator:hover {
    background: #333;
    border-color: #0066cc;
}

.round-indicator.active {
    background: #0066cc;
    color: white;
    border-color: #0066cc;
}

.round-number {
    font-weight: 600;
    font-size: 12px;
    margin-bottom: 4px;
}

.round-analysts {
    font-size: 10px;
    opacity: 0.7;
}

/* Chart Container */
.chart-container {
    position: relative;
    height: 250px;
    margin-bottom: 15px;
}
```

### JavaScript Functions

```javascript
let visualizationChart = null;
let votesChart = null;
let currentVisualizationJobId = null;

function startVisualizationPolling(jobId) {
    currentVisualizationJobId = jobId;
    updateVisualization(jobId);
    
    const poll = setInterval(() => {
        if (currentVisualizationJobId === jobId) {
            updateVisualization(jobId);
        } else {
            clearInterval(poll);
        }
    }, 1000);
}

function updateVisualization(jobId) {
    fetch(`/api/debate/${jobId}/visualization`)
        .then(r => r.json())
        .then(data => {
            displayAnalystCards(data.analysts);
            updateConfidenceChart(data.analysts);
            updateVotesChart(data.analysts);
            updateRoundTimeline(data.total_rounds, data.current_round);
        })
        .catch(err => console.error('Visualization fetch error:', err));
}

function displayAnalystCards(analysts) {
    const container = document.getElementById('analyst-cards-container');
    if (!container || !analysts) return;

    container.innerHTML = analysts.map(analyst => `
        <div class="analyst-card ${analyst.eliminated_round >= 0 ? 'eliminated' : ''}">
            <div class="analyst-header">
                <div>
                    <div class="analyst-name">${analyst.name}</div>
                    <div class="analyst-model">${analyst.provider}</div>
                </div>
                <div class="analyst-status ${analyst.confidence < 30 ? 'idle' : ''}">
                    ${analyst.confidence}%
                </div>
            </div>
            <div class="analyst-confidence-bar">
                <div class="analyst-confidence-fill" style="width: ${analyst.confidence}%"></div>
            </div>
            <div class="analyst-votes">
                <span>Votes:</span>
                <span class="vote-badge">${analyst.votes}</span>
            </div>
            ${analyst.eliminated_round >= 0 ? `
                <div style="font-size: 10px; color: #f44336; margin-top: 6px;">
                    ❌ Eliminated R${analyst.eliminated_round}
                </div>
            ` : ''}
        </div>
    `).join('');
}

function updateConfidenceChart(analysts) {
    const ctx = document.getElementById('confidenceChart');
    if (!ctx) return;

    const labels = analysts.map(a => a.name);
    const confidences = analysts.map(a => a.confidence);
    const colors = analysts.map(a => a.eliminated_round >= 0 ? '#999' : '#0066cc');

    if (visualizationChart) {
        visualizationChart.data.labels = labels;
        visualizationChart.data.datasets[0].data = confidences;
        visualizationChart.data.datasets[0].backgroundColor = colors;
        visualizationChart.update();
    } else {
        visualizationChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Confidence Score',
                    data: confidences,
                    backgroundColor: colors,
                    borderColor: '#444',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                indexAxis: 'y',
                plugins: {
                    legend: { display: false }
                },
                scales: {
                    x: {
                        max: 100,
                        ticks: { color: '#999' },
                        grid: { color: '#333' }
                    },
                    y: { ticks: { color: '#999' } }
                }
            }
        });
    }
}

function updateVotesChart(analysts) {
    const ctx = document.getElementById('votesChart');
    if (!ctx) return;

    const labels = analysts.map(a => a.name);
    const votes = analysts.map(a => a.votes);

    if (votesChart) {
        votesChart.data.labels = labels;
        votesChart.data.datasets[0].data = votes;
        votesChart.update();
    } else {
        votesChart = new Chart(ctx, {
            type: 'doughnut',
            data: {
                labels: labels,
                datasets: [{
                    data: votes,
                    backgroundColor: [
                        '#0066cc', '#00cc66', '#ffb74d', '#f44336',
                        '#9c27b0', '#00bcd4', '#ff5722', '#607d8b'
                    ]
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: { labels: { color: '#999' } }
                }
            }
        });
    }
}

function updateRoundTimeline(totalRounds, currentRound) {
    const container = document.getElementById('round-timeline');
    if (!container) return;

    let html = '';
    for (let i = 1; i <= totalRounds; i++) {
        html += `
            <div class="round-indicator ${i === currentRound ? 'active' : ''}" 
                 onclick="selectRound(${i})">
                <div class="round-number">Round ${i}</div>
                <div class="round-analysts">Active</div>
            </div>
        `;
    }
    container.innerHTML = html;
}

function selectRound(roundNumber) {
    console.log('Selected round:', roundNumber);
}
```

---

## Tab 4: Advanced Metrics Dashboard

### Purpose
Deep performance analysis with 6 Chart.js visualizations covering latency trends, success rates, token usage, model comparison, and time-series metrics.

### HTML Structure

```html
<!-- Advanced Metrics Tab -->
<div id="advanced-metrics" class="tab-content">
    <!-- Time Range Selector -->
    <div style="background: #1a1a1a; padding: 15px; border-bottom: 1px solid #333;">
        <div class="button-group">
            <button class="metric-range-btn active" onclick="switchMetricsRange(24)">Last 24h</button>
            <button class="metric-range-btn" onclick="switchMetricsRange(168)">Last 7 days</button>
            <button class="metric-range-btn" onclick="switchMetricsRange(0)">All time</button>
        </div>
    </div>

    <!-- Metrics Grid -->
    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; padding: 10px;">
        <!-- Chart 1: Provider Latency Trends -->
        <div class="panel">
            <h2>Provider Latency Trends</h2>
            <div class="chart-container">
                <canvas id="latencyTrendsChart"></canvas>
            </div>
        </div>

        <!-- Chart 2: Success Rate by Provider -->
        <div class="panel">
            <h2>Success Rate by Provider</h2>
            <div class="chart-container">
                <canvas id="successRateChart"></canvas>
            </div>
        </div>

        <!-- Chart 3: Token Distribution -->
        <div class="panel">
            <h2>Token Usage Distribution</h2>
            <div class="chart-container">
                <canvas id="tokenHistogramChart"></canvas>
            </div>
        </div>

        <!-- Chart 4: Model Performance -->
        <div class="panel">
            <h2>Model Performance Comparison</h2>
            <div class="chart-container">
                <canvas id="modelPerformanceChart"></canvas>
            </div>
        </div>

        <!-- Chart 5: Time-Series Metrics (Full Width) -->
        <div class="panel" style="grid-column: 1 / -1;">
            <h2>Time-Series Metrics</h2>
            <div class="chart-container" style="height: 300px;">
                <canvas id="timeseriesChart"></canvas>
            </div>
        </div>

        <!-- Chart 6: Percentile Analysis (Full Width) -->
        <div class="panel" style="grid-column: 1 / -1;">
            <h2>Latency Percentiles (P50/P95/P99)</h2>
            <div class="chart-container" style="height: 250px;">
                <canvas id="percentilesChart"></canvas>
            </div>
        </div>
    </div>
</div>
```

### CSS for Metrics Range Buttons

```css
.metric-range-btn {
    background: #444;
    color: #fff;
    border: 1px solid #555;
    padding: 8px 16px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 13px;
    font-weight: 500;
    transition: all 0.3s;
}

.metric-range-btn:hover {
    background: #555;
    border-color: #0066cc;
}

.metric-range-btn.active {
    background: #0066cc;
    border-color: #0066cc;
    color: white;
}
```

### JavaScript Functions

```javascript
let latencyTrendsChart = null;
let successRateChart = null;
let tokenHistogramChart = null;
let modelPerformanceChart = null;
let timeseriesChart = null;
let percentilesChart = null;
let currentMetricsHours = 24;

function switchMetricsRange(hours) {
    currentMetricsHours = hours;
    
    // Update button states
    document.querySelectorAll('.metric-range-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    event.target.classList.add('active');
    
    // Refresh all charts
    refreshAdvancedMetrics();
}

function refreshAdvancedMetrics() {
    updateLatencyTrendsChart();
    updateSuccessRateChart();
    updateTokenHistogramChart();
    updateModelPerformanceChart();
    updateTimeseriesChart();
    updatePercentilesChart();
}

function updateLatencyTrendsChart() {
    fetch(`/api/metrics/trends?hours=${currentMetricsHours}`)
        .then(r => r.json())
        .then(data => {
            const providers = Object.keys(data.providers);
            const latencies = providers.map(p => data.providers[p].latencies[0]); // p50
            
            const ctx = document.getElementById('latencyTrendsChart');
            if (latencyTrendsChart) {
                latencyTrendsChart.data.labels = providers;
                latencyTrendsChart.data.datasets[0].data = latencies;
                latencyTrendsChart.update();
            } else {
                latencyTrendsChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: providers,
                        datasets: [{
                            label: 'Latency (ms)',
                            data: latencies,
                            borderColor: '#0066cc',
                            backgroundColor: 'rgba(0, 102, 204, 0.1)',
                            tension: 0.4,
                            fill: true
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: { legend: { labels: { color: '#999' } } },
                        scales: {
                            y: { ticks: { color: '#999' }, grid: { color: '#333' } },
                            x: { ticks: { color: '#999' }, grid: { color: '#333' } }
                        }
                    }
                });
            }
        });
}

function updateSuccessRateChart() {
    fetch(`/api/metrics/trends?hours=${currentMetricsHours}`)
        .then(r => r.json())
        .then(data => {
            const providers = Object.keys(data.providers);
            const successRates = providers.map(p => data.providers[p].success_rate);
            
            const ctx = document.getElementById('successRateChart');
            if (successRateChart) {
                successRateChart.data.labels = providers;
                successRateChart.data.datasets[0].data = successRates;
                successRateChart.update();
            } else {
                successRateChart = new Chart(ctx, {
                    type: 'bar',
                    data: {
                        labels: providers,
                        datasets: [{
                            label: 'Success Rate (%)',
                            data: successRates,
                            backgroundColor: successRates.map(rate => 
                                rate > 80 ? '#4caf50' : rate > 60 ? '#ffb74d' : '#f44336'
                            )
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: { legend: { display: false } },
                        scales: {
                            y: { 
                                max: 100,
                                ticks: { color: '#999' },
                                grid: { color: '#333' }
                            },
                            x: { ticks: { color: '#999' } }
                        }
                    }
                });
            }
        });
}

function updateTokenHistogramChart() {
    fetch(`/api/metrics/tokens?hours=${currentMetricsHours}`)
        .then(r => r.json())
        .then(data => {
            const ctx = document.getElementById('tokenHistogramChart');
            if (tokenHistogramChart) {
                tokenHistogramChart.data.labels = data.labels;
                tokenHistogramChart.data.datasets[0].data = data.bins;
                tokenHistogramChart.update();
            } else {
                tokenHistogramChart = new Chart(ctx, {
                    type: 'bar',
                    data: {
                        labels: data.labels,
                        datasets: [{
                            label: 'Debates',
                            data: data.bins,
                            backgroundColor: '#00cc66'
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: { legend: { display: false } },
                        scales: {
                            y: { ticks: { color: '#999' }, grid: { color: '#333' } },
                            x: { ticks: { color: '#999' } }
                        }
                    }
                });
            }
        });
}

function updateModelPerformanceChart() {
    fetch(`/api/metrics/models?hours=${currentMetricsHours}`)
        .then(r => r.json())
        .then(data => {
            const ctx = document.getElementById('modelPerformanceChart');
            const labels = data.models.map(m => m.name);
            const latencies = data.models.map(m => m.avg_latency);
            
            if (modelPerformanceChart) {
                modelPerformanceChart.data.labels = labels;
                modelPerformanceChart.data.datasets[0].data = latencies;
                modelPerformanceChart.update();
            } else {
                modelPerformanceChart = new Chart(ctx, {
                    type: 'bar',
                    data: {
                        labels: labels,
                        datasets: [{
                            label: 'Avg Latency (ms)',
                            data: latencies,
                            backgroundColor: '#ffb74d'
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: { legend: { display: false } },
                        scales: {
                            y: { ticks: { color: '#999' }, grid: { color: '#333' } },
                            x: { ticks: { color: '#999' } }
                        }
                    }
                });
            }
        });
}

function updateTimeseriesChart() {
    fetch(`/api/metrics/timeseries?hours=${currentMetricsHours}`)
        .then(r => r.json())
        .then(data => {
            const ctx = document.getElementById('timeseriesChart');
            const timestamps = data.timestamps.map(ts => new Date(ts).toLocaleTimeString());
            
            if (timeseriesChart) {
                timeseriesChart.data.labels = timestamps;
                timeseriesChart.data.datasets[0].data = data.latencies;
                timeseriesChart.update();
            } else {
                timeseriesChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: timestamps,
                        datasets: [{
                            label: 'Latency (ms)',
                            data: data.latencies,
                            borderColor: '#0066cc',
                            backgroundColor: 'rgba(0, 102, 204, 0.1)',
                            tension: 0.4,
                            fill: true,
                            pointRadius: 2
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: { legend: { labels: { color: '#999' } } },
                        scales: {
                            y: { ticks: { color: '#999' }, grid: { color: '#333' } },
                            x: { ticks: { color: '#999' } }
                        }
                    }
                });
            }
        });
}

function updatePercentilesChart() {
    // Query all providers for percentiles
    const providers = ['ollama', 'claude', 'google-agy'];
    const promises = providers.map(p =>
        fetch(`/api/metrics/percentiles?provider=${p}&hours=${currentMetricsHours}`)
            .then(r => r.json())
    );
    
    Promise.all(promises).then(results => {
        const ctx = document.getElementById('percentilesChart');
        const labels = ['P50', 'P95', 'P99'];
        const datasets = results.map((result, idx) => ({
            label: result.provider,
            data: [result.p50, result.p95, result.p99],
            borderColor: ['#0066cc', '#00cc66', '#ffb74d'][idx],
            backgroundColor: `rgba(0, 102, 204, ${0.1 + idx * 0.1})`,
            tension: 0.4
        }));
        
        if (percentilesChart) {
            percentilesChart.data.datasets = datasets;
            percentilesChart.update();
        } else {
            percentilesChart = new Chart(ctx, {
                type: 'line',
                data: { labels, datasets },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: { legend: { labels: { color: '#999' } } },
                    scales: {
                        y: { ticks: { color: '#999' }, grid: { color: '#333' } },
                        x: { ticks: { color: '#999' } }
                    }
                }
            });
        }
    });
}
```

---

## Integration Steps

### 1. Update Tab Switching Logic

Modify `switchTab()` to initialize visualizations when tab is opened:

```javascript
function switchTab(tabName) {
    document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    
    document.getElementById(tabName).classList.add('active');
    event.target.classList.add('active');
    
    if (tabName === 'advanced-metrics') {
        refreshAdvancedMetrics();
    } else if (tabName === 'visualization' && selectedJobId) {
        startVisualizationPolling(selectedJobId);
    }
}
```

### 2. Wire Visualization to Query Results

When a debate completes, start visualization polling:

```javascript
.then(job => {
    if (job.status !== 0) {
        clearInterval(poll);
        refreshJobs();
        
        // Start visualization polling if switching to that tab
        if (currentTab === 'visualization') {
            startVisualizationPolling(job.id);
        }
    }
});
```

### 3. Add Current Tab Tracking

Add a global variable to track active tab:

```javascript
let currentTab = 'query';

function switchTab(tabName) {
    currentTab = tabName;
    // ... rest of tab switching logic
}
```

---

## Performance Notes

- **Analyst Cards**: Render in <50ms with CSS grid
- **Confidence Chart**: Update via Chart.js data binding, no redraw needed
- **Token Histogram**: Bins hardcoded (500-token buckets) for speed
- **Percentile Chart**: Fetch 3 providers in parallel for <300ms total
- **Time-Series**: 1-minute bucket aggregation limits points to <1440 per 24h
- **Mobile**: Responsive grid collapses to 1 column on screens <768px

---

## Data Flow Diagram

```
Query Execution
    ↓
[Phase 1] Update debate progress → Real-time flow UI
    ↓
Debate Completes
    ↓
[Phase 2 Viz] Fetch analyst data → Display cards & charts
    ↓
[Phase 2 Metrics] Aggregate data → Percentiles & trends
    ↓
Dashboard displays both visualizations in real-time
```

---

## API Response Examples

### Visualization Endpoint
```json
{
  "job_id": 5,
  "analysts": [
    {
      "id": "analyst_1",
      "name": "claude-3",
      "provider": "claude",
      "confidence": 85,
      "votes": 3,
      "eliminated_round": -1,
      "argument_strength": 0.92
    }
  ],
  "winner": { "analyst_id": "analyst_1", "confidence": 85 }
}
```

### Metrics Trends Endpoint
```json
{
  "hours": 24,
  "providers": {
    "ollama": {
      "latencies": [150, 280, 450],
      "success_rate": 95.2
    }
  }
}
```

---

## Success Metrics

✓ Analyst cards render in <100ms  
✓ All 6 charts initialize within 2 seconds  
✓ Visualization updates every 1 second during debate  
✓ Metrics range selector responsive  
✓ Mobile layout single-column  
✓ No memory leaks with 100+ data points  
✓ Smooth animations for pruning/elimination  

---

## Estimated Effort

- Dashboard HTML update: 1 hour
- CSS styling: 1 hour
- JavaScript for visualization: 2 hours
- JavaScript for metrics: 2 hours
- Testing and refinement: 1 hour

**Total: ~7 hours frontend implementation**

Ready for dashboard HTML update in `get_dashboard_html()` after this guide review.
