# Phase 1 Frontend Implementation Guide

## Dashboard Enhancement Strategy

### New Tab Structure
```html
<!-- Tab Navigation (update existing tab-btn group) -->
<button class="tab-btn active" onclick="switchTab('query')">Query Interface</button>
<button class="tab-btn" onclick="switchTab('debate-progress')">Debate Progress</button>
<button class="tab-btn" onclick="switchTab('history')">Query History</button>
<button class="tab-btn" onclick="switchTab('metrics')">Metrics</button>
```

### Tab 1: Query Debate Progress (Real-time Flow)

**HTML Structure:**
```html
<div id="debate-progress" class="tab-content">
  <div class="debate-container">
    <div class="progress-header">
      <h2>Live Debate Progress</h2>
      <div class="progress-info">
        <span id="round-counter">Round 0/4</span>
        <span id="analyst-count">Analysts: 4</span>
      </div>
    </div>
    
    <div class="progress-bar-container">
      <div class="progress-bar">
        <div id="progress-fill" class="progress-fill"></div>
      </div>
      <div id="progress-percent">0%</div>
    </div>
    
    <div class="debate-content">
      <div id="argument-display" class="argument-display">
        <p>Select a running debate to see real-time progress...</p>
      </div>
    </div>
  </div>
</div>
```

**CSS:**
```css
.debate-container {
  display: flex;
  flex-direction: column;
  gap: 15px;
  height: 100%;
}

.progress-bar-container {
  display: flex;
  gap: 10px;
  align-items: center;
}

.progress-bar {
  flex: 1;
  height: 20px;
  background: #2a2a2a;
  border-radius: 10px;
  border: 1px solid #444;
  overflow: hidden;
}

.progress-fill {
  height: 100%;
  background: linear-gradient(90deg, #0066cc, #00b4ff);
  width: 0%;
  transition: width 0.3s ease;
}

.argument-display {
  background: #0a0a0a;
  border: 1px solid #333;
  padding: 15px;
  border-radius: 3px;
  font-size: 13px;
  line-height: 1.6;
  max-height: 400px;
  overflow-y: auto;
}
```

**JavaScript:**
```javascript
let debatePollingInterval = null;

function switchTab(tabName) {
  // Stop debate polling if switching away
  if (debatePollingInterval) {
    clearInterval(debatePollingInterval);
    debatePollingInterval = null;
  }
  
  document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  
  document.getElementById(tabName).classList.add('active');
  event.target.classList.add('active');
  
  // Start polling if switching to debate progress
  if (tabName === 'debate-progress' && selectedJobId) {
    startDebatePolling(selectedJobId);
  }
}

function startDebatePolling(jobId) {
  // Poll every 500ms for real-time updates
  debatePollingInterval = setInterval(() => {
    fetch(`/api/debate/${jobId}/progress`)
      .then(r => r.json())
      .then(data => {
        updateDebateDisplay(data);
      })
      .catch(() => {
        // Stop polling if job not found
        clearInterval(debatePollingInterval);
        debatePollingInterval = null;
      });
  }, 500);
  
  // Fetch immediately
  fetch(`/api/debate/${jobId}/progress`)
    .then(r => r.json())
    .then(data => updateDebateDisplay(data));
}

function updateDebateDisplay(data) {
  // Update round counter
  document.getElementById('round-counter').textContent = 
    `Round ${data.current_round}/${data.total_rounds}`;
  
  // Update analyst count
  document.getElementById('analyst-count').textContent = 
    `Analysts: ${data.analyst_count}`;
  
  // Update progress bar
  const percent = data.progress_percent || 0;
  document.getElementById('progress-fill').style.width = percent + '%';
  document.getElementById('progress-percent').textContent = percent + '%';
  
  // Update argument display
  let argText = `Debate Progress\n`;
  argText += `Query: ${data.query}\n`;
  argText += `Provider: ${data.provider}\n`;
  argText += `Duration: ${data.duration_ms}ms\n`;
  argText += `Status: ${['Running', 'Complete', 'Failed'][data.status]}\n`;
  document.getElementById('argument-display').textContent = argText;
  
  // Stop polling if complete or failed
  if (data.status !== 0) {
    if (debatePollingInterval) {
      clearInterval(debatePollingInterval);
      debatePollingInterval = null;
    }
  }
}
```

### Tab 2: Query History & Search

**HTML Structure:**
```html
<div id="history" class="tab-content">
  <div class="history-container">
    <div class="history-header">
      <h2>Query History & Search</h2>
      <input type="text" id="search-input" placeholder="Search queries..." 
             onkeyup="searchHistory(this.value)">
    </div>
    
    <div class="history-controls">
      <button onclick="sortHistoryBy('timestamp')">Sort by Date</button>
      <button onclick="sortHistoryBy('duration_ms')">Sort by Duration</button>
      <button onclick="sortHistoryBy('provider')">Sort by Provider</button>
    </div>
    
    <div class="history-table-container">
      <table class="history-table">
        <thead>
          <tr>
            <th>ID</th>
            <th>Query</th>
            <th>Provider</th>
            <th>Duration (ms)</th>
            <th>Status</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody id="history-tbody">
          <!-- Populated by JavaScript -->
        </tbody>
      </table>
    </div>
  </div>
</div>
```

**CSS:**
```css
.history-container {
  display: flex;
  flex-direction: column;
  gap: 15px;
  height: 100%;
}

.history-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 10px;
}

#search-input {
  padding: 8px 12px;
  background: #2a2a2a;
  border: 1px solid #444;
  color: #e0e0e0;
  border-radius: 4px;
  width: 200px;
  font-size: 12px;
}

.history-table-container {
  overflow-y: auto;
  flex: 1;
}

.history-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
}

.history-table th, .history-table td {
  padding: 8px;
  text-align: left;
  border-bottom: 1px solid #333;
}

.history-table th {
  background: #1e1e1e;
  color: #0066cc;
  font-weight: 500;
  position: sticky;
  top: 0;
}

.history-table tr:hover {
  background: #2a2a2a;
  cursor: pointer;
}

.history-table .status-success {
  color: #4caf50;
}

.history-table .status-failed {
  color: #f44336;
}

.history-table button {
  padding: 4px 8px;
  font-size: 11px;
  background: #0066cc;
}
```

**JavaScript:**
```javascript
let allHistory = [];
let filteredHistory = [];

function loadQueryHistory() {
  fetch('/api/history')
    .then(r => r.json())
    .then(data => {
      allHistory = data.history || [];
      filteredHistory = [...allHistory];
      displayHistory();
    });
}

function searchHistory(searchText) {
  if (!searchText.trim()) {
    filteredHistory = [...allHistory];
  } else {
    filteredHistory = allHistory.filter(job => 
      job.query.toLowerCase().includes(searchText.toLowerCase())
    );
  }
  displayHistory();
}

function sortHistoryBy(field) {
  filteredHistory.sort((a, b) => {
    if (typeof a[field] === 'string') {
      return a[field].localeCompare(b[field]);
    }
    return a[field] - b[field];
  });
  displayHistory();
}

function displayHistory() {
  const tbody = document.getElementById('history-tbody');
  tbody.innerHTML = filteredHistory.map(job => `
    <tr>
      <td>${job.job_id}</td>
      <td>${job.query.substring(0, 40)}...</td>
      <td>${job.provider}</td>
      <td>${job.duration_ms}</td>
      <td class="status-${job.success ? 'success' : 'failed'}">
        ${job.success ? 'Success' : 'Failed'}
      </td>
      <td>
        <button onclick="rerundebate('${job.query}')">Re-run</button>
      </td>
    </tr>
  `).join('');
}

function rerunDebate(query) {
  document.getElementById('query-input').value = query;
  switchTab('query');
  submitQuery();
}

// Load history on page load
loadQueryHistory();
// Refresh history every 5 seconds
setInterval(loadQueryHistory, 5000);
```

## Integration Steps

1. **Update tab navigation** in dashboard HTML with new tabs
2. **Add new CSS** for debate progress and history styling
3. **Add new JavaScript** functions for tab switching, polling, search
4. **Maintain existing functionality** for Query Interface and Metrics tabs
5. **Test end-to-end** with live API endpoints

## Key Features Implemented

✅ Real-time debate progress tracking with 500ms polling  
✅ Query history display with session persistence  
✅ Full-text search of past queries  
✅ Sort by date, duration, provider  
✅ Re-run past queries with one click  
✅ Progress bar visualization  
✅ Live round counter and analyst tracking  

## Performance Optimization

- Polling only when Debate tab is active
- Search runs client-side (minimal server load)
- Debounced search input (optional)
- Limited history to current session

## Future Enhancements

- Persistent database storage of query history
- Export history as CSV/JSON
- Debate replay with full argument history
- Provider comparison view
- Advanced filtering by date range
