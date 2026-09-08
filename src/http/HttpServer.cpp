#include "HttpServer.h"
#include "../util/Logging.h"
#include "../util/Metrics.h"
#include "../llm/MultiEndpointOllamaClient.h"
#include <sstream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>

namespace tribunal {
namespace http {

using namespace util;

HttpServer::HttpServer(
    const core::Configuration& config,
    std::unique_ptr<llm::LLMClient> llm_client
) : config_(config), llm_client_(std::move(llm_client)) {
    council_ = std::make_unique<core::CouncilOrchestrator>(config);
}

HttpServer::~HttpServer() {
    stop();
    if (server_socket_ >= 0) {
        close(server_socket_);
    }
}

int HttpServer::start() {
    Logger& logger = Logger::instance();
    logger.log(LogLevel::Info, "Starting HTTP server on port 8080...");

    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        logger.log(LogLevel::Error, "Failed to create socket");
        return 1;
    }

    int reuse = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.log(LogLevel::Error, "Failed to bind to port 8080");
        close(server_socket_);
        return 1;
    }

    if (listen(server_socket_, 5) < 0) {
        logger.log(LogLevel::Error, "Failed to listen on socket");
        close(server_socket_);
        return 1;
    }

    running_ = true;
    logger.log(LogLevel::Info, "✓ HTTP server listening on http://localhost:8080");
    logger.log(LogLevel::Info, "✓ Web dashboard running");
    logger.log(LogLevel::Info, "Press Ctrl+C to exit");

    /* Accept connections in this thread */
    while (running_) {
        struct sockaddr_in client_addr = {};
        socklen_t addr_len = sizeof(client_addr);

        int client = accept(server_socket_, (struct sockaddr*)&client_addr, &addr_len);
        if (client >= 0 && running_) {
            handle_request(client);
            close(client);
        }
    }

    close(server_socket_);
    server_socket_ = -1;
    return 0;
}

void HttpServer::stop() {
    running_ = false;
}

void HttpServer::handle_request(int client) {
    char buffer[4096] = {0};
    ssize_t bytes = recv(client, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) return;

    std::string method, path, body;
    if (!parse_request(std::string(buffer, bytes), method, path, body)) {
        send_response(client, 400, "text/plain", "Bad Request");
        return;
    }

    /* Route requests */
    if (path == "/" || path == "/index.html") {
        handle_root(client);
    } else if (path == "/api/tasks" && method == "GET") {
        handle_tasks(client);
    } else if (path == "/api/query" && method == "POST") {
        handle_query(client, body);
    } else if (path.substr(0, 11) == "/api/query?" && method == "GET") {
        /* Extract job_id from query string */
        size_t job_pos = path.find("job_id=");
        if (job_pos != std::string::npos) {
            int job_id = std::stoi(path.substr(job_pos + 7));
            handle_query_status(client, job_id);
        } else {
            send_response(client, 400, "application/json", "{\"error\":\"missing job_id\"}");
        }
    } else if (path == "/metrics" && method == "GET") {
        handle_metrics(client);
    } else if (path == "/api/stats" && method == "GET") {
        handle_stats(client);
    } else if (path == "/api/history" && method == "GET") {
        handle_history(client);
    } else if (path.substr(0, 18) == "/api/history/search" && method == "GET") {
        /* Extract search query from query string */
        size_t q_pos = path.find("q=");
        if (q_pos != std::string::npos) {
            std::string search_query = path.substr(q_pos + 2);
            handle_history_search(client, search_query);
        } else {
            send_response(client, 400, "application/json", "{\"error\":\"missing q parameter\"}");
        }
    } else if (path.substr(0, 15) == "/api/debate/" && method == "GET") {
        /* Extract job_id from path */
        size_t progress_pos = path.find("/progress");
        size_t viz_pos = path.find("/visualization");

        if (progress_pos != std::string::npos) {
            try {
                int job_id = std::stoi(path.substr(11, progress_pos - 11));
                handle_debate_progress(client, job_id);
            } catch (...) {
                send_response(client, 400, "application/json", "{\"error\":\"invalid job_id\"}");
            }
        } else if (viz_pos != std::string::npos) {
            try {
                int job_id = std::stoi(path.substr(11, viz_pos - 11));
                handle_debate_visualization(client, job_id);
            } catch (...) {
                send_response(client, 400, "application/json", "{\"error\":\"invalid job_id\"}");
            }
        } else {
            send_response(client, 404, "text/plain", "Not Found");
        }
    } else if (path.substr(0, 14) == "/api/metrics/" && method == "GET") {
        /* Extract metric type and parameters */
        if (path.find("/trends") != std::string::npos) {
            size_t q_pos = path.find("?");
            std::string params = (q_pos != std::string::npos) ? path.substr(q_pos) : "";
            handle_metrics_trends(client, params);
        } else if (path.find("/percentiles") != std::string::npos) {
            size_t q_pos = path.find("?");
            std::string params = (q_pos != std::string::npos) ? path.substr(q_pos) : "";
            handle_metrics_percentiles(client, params);
        } else if (path.find("/tokens") != std::string::npos) {
            size_t q_pos = path.find("?");
            std::string params = (q_pos != std::string::npos) ? path.substr(q_pos) : "";
            handle_metrics_tokens(client, params);
        } else if (path.find("/models") != std::string::npos) {
            size_t q_pos = path.find("?");
            std::string params = (q_pos != std::string::npos) ? path.substr(q_pos) : "";
            handle_metrics_models(client, params);
        } else if (path.find("/timeseries") != std::string::npos) {
            size_t q_pos = path.find("?");
            std::string params = (q_pos != std::string::npos) ? path.substr(q_pos) : "";
            handle_metrics_timeseries(client, params);
        } else {
            send_response(client, 404, "text/plain", "Not Found");
        }
    } else {
        send_response(client, 404, "text/plain", "Not Found");
    }
}

bool HttpServer::parse_request(
    const std::string& buffer,
    std::string& method,
    std::string& path,
    std::string& body
) {
    std::istringstream iss(buffer);
    std::string line;

    /* Parse request line */
    if (!std::getline(iss, line)) return false;

    std::istringstream req_line(line);
    if (!(req_line >> method >> path)) return false;

    /* Parse headers until empty line */
    int content_length = 0;
    while (std::getline(iss, line)) {
        if (line.empty() || line == "\r") break;

        if (line.substr(0, 16) == "Content-Length: ") {
            content_length = std::stoi(line.substr(16));
        }
    }

    /* Read body if POST */
    if (method == "POST" && content_length > 0) {
        body.resize(content_length);
        iss.read(&body[0], content_length);
    }

    return true;
}

void HttpServer::send_response(
    int client,
    int status_code,
    const std::string& content_type,
    const std::string& body
) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " OK\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;

    std::string response = oss.str();
    send(client, response.c_str(), response.length(), 0);
}

void HttpServer::handle_root(int client) {
    send_response(client, 200, "text/html", get_dashboard_html());
}

void HttpServer::handle_tasks(int client) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream json;
    json << "{\"tasks\":[";

    bool first = true;
    for (const auto& [job_id, job] : jobs_) {
        if (!first) json << ",";
        json << "{"
            << "\"id\":" << job_id
            << ",\"query\":\"" << job.query << "\""
            << ",\"status\":" << job.status
            << ",\"output\":\"" << job.output << "\""
            << ",\"duration\":" << job.duration_ms
            << "}";
        first = false;
    }

    json << "]}";
    send_response(client, 200, "application/json", json.str());
}

void HttpServer::handle_query(int client, const std::string& body) {
    /* Parse JSON request: {"query":"...", "rounds":N} */
    std::string query;
    int rounds = config_.rounds;

    /* Simple JSON parsing */
    size_t query_pos = body.find("\"query\":\"");
    if (query_pos != std::string::npos) {
        size_t start = query_pos + 9;
        size_t end = body.find("\"", start);
        query = body.substr(start, end - start);
    }

    if (query.empty()) {
        send_response(client, 400, "application/json", "{\"error\":\"missing query\"}");
        return;
    }

    ExecutionJob job = allocate_job(query);
    job.rounds = rounds;

    /* Return job ID immediately */
    std::ostringstream response;
    response << "{\"job_id\":" << job.job_id << "}";
    send_response(client, 200, "application/json", response.str());

    /* Execute query asynchronously */
    execute_query_async(job);
}

void HttpServer::handle_query_status(int client, int job_id) {
    ExecutionJob job = find_job(job_id);

    std::ostringstream json;
    if (job.job_id == -1) {
        json << "{\"error\":\"job not found\"}";
        send_response(client, 404, "application/json", json.str());
        return;
    }

    json << "{"
        << "\"id\":" << job.job_id
        << ",\"status\":" << job.status
        << ",\"query\":\"" << job.query << "\""
        << ",\"output\":\"" << job.output << "\""
        << ",\"duration\":" << job.duration_ms
        << "}";

    send_response(client, 200, "application/json", json.str());
}

ExecutionJob HttpServer::allocate_job(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);

    ExecutionJob job;
    job.job_id = next_job_id_++;
    job.query = query;
    job.status = 0;  /* running */
    job.start_time = std::time(nullptr);
    job.provider = config_.api_type;
    job.rounds = config_.rounds;

    jobs_[job.job_id] = job;
    query_history_.push_back(job);  /* Track in history */
    return job;
}

ExecutionJob HttpServer::find_job(int job_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = jobs_.find(job_id);
    if (it != jobs_.end()) {
        return it->second;
    }

    ExecutionJob empty;
    empty.job_id = -1;
    return empty;
}

void HttpServer::update_job_output(int job_id, const std::string& output, int status) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = jobs_.find(job_id);
    if (it != jobs_.end()) {
        it->second.output = output;
        it->second.status = status;
        it->second.duration_ms = (std::time(nullptr) - it->second.start_time) * 1000;
        it->second.success = (status == 1);
    }
}

void HttpServer::execute_query_async(ExecutionJob job) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::thread([this, job, start_time]() {
        try {
            /* Create new LLM client for this thread using configured provider */
            auto thread_client = llm::ClientFactory::create_from_config(
                config_.api_type,
                config_.claude_api_key,
                config_.claude_model,
                config_.google_agy_api_key,
                config_.google_agy_model,
                config_.google_agy_endpoint,
                config_.ollama_urls.empty() ? "" : config_.ollama_urls[0]
            );

            if (!council_->initialize_analysts(std::move(thread_client), config_.models)) {
                update_job_output(job.job_id, "Failed to initialize analysts", 2);

                /* Record failure metric */
                util::DebateMetrics metrics;
                metrics.llm_provider = config_.api_type;
                metrics.success = false;
                metrics.error = "Failed to initialize analysts";
                util::Metrics::instance().record_debate(metrics);
                return;
            }

            core::DebateResult result = council_->run_debate(job.query, job.rounds);

            /* Record success metric */
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            util::DebateMetrics metrics;
            metrics.query_hash = std::to_string(std::hash<std::string>{}(job.query));
            metrics.rounds_completed = result.rounds_completed;
            metrics.total_tokens_used = 0;  /* Would need to track from LLM client */
            metrics.duration_ms = duration_ms;
            metrics.success = true;
            metrics.llm_provider = config_.api_type;
            util::Metrics::instance().record_debate(metrics);

            std::ostringstream output;
            output << "Debate completed.\n";
            output << "Winner: Analyst " << result.final_winner << "\n";
            output << "Rounds: " << result.rounds_completed << "\n";
            output << "Pruned: " << result.analysts_pruned << " analysts\n";
            output << "Duration: " << duration_ms << "ms";

            update_job_output(job.job_id, output.str(), 1);
        } catch (const std::exception& e) {
            update_job_output(job.job_id, std::string("Error: ") + e.what(), 2);

            /* Record failure metric */
            util::DebateMetrics metrics;
            metrics.llm_provider = config_.api_type;
            metrics.success = false;
            metrics.error = e.what();
            util::Metrics::instance().record_debate(metrics);
        }
    }).detach();
}

std::string HttpServer::get_dashboard_html() const {
    return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>TRIBUNAL - Debate Dashboard</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f0f0f; color: #e0e0e0; }
        header { background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%); padding: 20px; border-bottom: 2px solid #444; }
        header h1 { margin: 0 0 5px 0; font-size: 24px; }
        header p { opacity: 0.8; font-size: 13px; }
        .tabs { display: flex; gap: 0; background: #1a1a1a; border-bottom: 1px solid #333; }
        .tab-btn { padding: 12px 20px; background: transparent; border: none; color: #999; cursor: pointer; font-size: 13px; font-weight: 500; border-bottom: 2px solid transparent; }
        .tab-btn.active { color: #0066cc; border-bottom-color: #0066cc; }
        .tab-btn:hover { color: #ddd; }
        .tab-content { display: none; height: calc(100vh - 120px); }
        .tab-content.active { display: grid; }
        .grid-2 { grid-template-columns: 1fr 1fr; gap: 10px; padding: 10px; }
        .grid-3 { grid-template-columns: 1fr 1fr 400px; gap: 10px; padding: 10px; }
        .panel { background: #1e1e1e; border-radius: 4px; border: 1px solid #333; padding: 15px; overflow-y: auto; }
        .panel h2 { font-size: 13px; margin-bottom: 12px; color: #0066cc; text-transform: uppercase; letter-spacing: 1px; }
        textarea { width: 100%; height: 120px; background: #2a2a2a; color: #e0e0e0; border: 1px solid #444; border-radius: 4px; padding: 8px; font-family: monospace; font-size: 12px; resize: vertical; }
        button { background: #0066cc; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; font-size: 13px; font-weight: 500; }
        button:hover { background: #0052a3; }
        button.secondary { background: #444; color: #fff; }
        button.secondary:hover { background: #555; }
        .job { border-left: 4px solid #444; padding: 10px; margin-bottom: 8px; background: #2a2a2a; border-radius: 3px; cursor: pointer; transition: 0.2s; }
        .job:hover { background: #333; }
        .job.running { border-left-color: #ffb74d; box-shadow: 0 0 8px rgba(255,152,0,0.3); }
        .job.complete { border-left-color: #4caf50; }
        .job.failed { border-left-color: #f44336; }
        .job-name { font-weight: 500; margin-bottom: 3px; font-size: 12px; }
        .job-status { font-size: 11px; opacity: 0.6; }
        .output { background: #0a0a0a; border: 1px solid #333; padding: 10px; border-radius: 3px; font-family: monospace; font-size: 11px; overflow-y: auto; white-space: pre-wrap; word-break: break-word; }
        .stat { padding: 12px; background: #2a2a2a; border-radius: 3px; border-left: 3px solid #0066cc; margin-bottom: 8px; }
        .stat-value { font-size: 20px; font-weight: bold; color: #0066cc; }
        .stat-label { font-size: 11px; opacity: 0.7; margin-top: 4px; }
        .chart-container { position: relative; height: 250px; margin-bottom: 15px; }
        .export-btn { width: 100%; margin-top: 10px; }
        .button-group { display: flex; gap: 8px; }
        .button-group button { flex: 1; }

        /* Phase 1: Debate Progress Tab */
        .debate-container { display: flex; flex-direction: column; gap: 15px; height: 100%; }
        .progress-header { display: flex; justify-content: space-between; align-items: center; }
        .progress-info { display: flex; gap: 20px; font-size: 12px; }
        .progress-bar-container { display: flex; gap: 10px; align-items: center; }
        .progress-bar { flex: 1; height: 20px; background: #2a2a2a; border-radius: 10px; border: 1px solid #444; overflow: hidden; }
        .progress-fill { height: 100%; background: linear-gradient(90deg, #0066cc, #00b4ff); width: 0%; transition: width 0.3s ease; }
        .argument-display { background: #0a0a0a; border: 1px solid #333; padding: 15px; border-radius: 3px; font-size: 13px; line-height: 1.6; max-height: 400px; overflow-y: auto; font-family: monospace; }

        /* Phase 1: History Tab */
        .history-container { display: flex; flex-direction: column; gap: 15px; height: 100%; }
        .history-header { display: flex; justify-content: space-between; align-items: center; gap: 10px; }
        #search-input { padding: 8px 12px; background: #2a2a2a; border: 1px solid #444; color: #e0e0e0; border-radius: 4px; width: 200px; font-size: 12px; }
        .history-controls { display: flex; gap: 8px; }
        .history-controls button { padding: 8px 16px; background: #444; color: #fff; border: 1px solid #555; border-radius: 4px; cursor: pointer; font-size: 12px; }
        .history-controls button:hover { background: #555; }
        .history-table-container { overflow-y: auto; flex: 1; }
        .history-table { width: 100%; border-collapse: collapse; font-size: 12px; }
        .history-table th, .history-table td { padding: 8px; text-align: left; border-bottom: 1px solid #333; }
        .history-table th { background: #1e1e1e; color: #0066cc; font-weight: 500; position: sticky; top: 0; }
        .history-table tr:hover { background: #2a2a2a; cursor: pointer; }
        .status-success { color: #4caf50; font-weight: 500; }
        .status-failed { color: #f44336; font-weight: 500; }
        .history-table button { padding: 4px 8px; font-size: 11px; background: #0066cc; color: white; border: none; border-radius: 3px; cursor: pointer; }
        .history-table button:hover { background: #0052a3; }
    </style>
</head>
<body>
    <header>
        <h1>⚖️ TRIBUNAL - Debate Dashboard</h1>
        <p>Multi-LLM Debate System with Real-time Metrics</p>
    </header>
    <div class="tabs">
        <button class="tab-btn active" onclick="switchTab('query')">Query Interface</button>
        <button class="tab-btn" onclick="switchTab('debate-progress')">Debate Progress</button>
        <button class="tab-btn" onclick="switchTab('history')">Query History</button>
        <button class="tab-btn" onclick="switchTab('metrics')">Metrics & Performance</button>
    </div>

    <!-- Query Interface Tab -->
    <div id="query" class="tab-content active grid-3">
        <div class="panel">
            <h2>Debate Query</h2>
            <textarea id="query-input" placeholder="Enter your query...">Check for security issues in this code</textarea>
            <button onclick="submitQuery()" style="width: 100%; margin-top: 10px;">Run Debate</button>
        </div>
        <div class="panel">
            <h2>Active Jobs</h2>
            <div id="jobs"></div>
        </div>
        <div class="panel">
            <h2>Results</h2>
            <div class="output" id="output">Results will appear here...</div>
            <div class="button-group" style="margin-top: 10px;">
                <button class="secondary" onclick="exportJSON()">Export JSON</button>
                <button class="secondary" onclick="exportText()">Export Text</button>
            </div>
        </div>
    </div>

    <!-- Debate Progress Tab (Phase 1) -->
    <div id="debate-progress" class="tab-content">
        <div class="panel" style="height: 100%;">
            <div class="debate-container">
                <div class="progress-header">
                    <h2>Live Debate Progress</h2>
                    <div class="progress-info">
                        <span id="round-counter">Round 0/4</span>
                        <span id="analyst-count">Analysts: 0</span>
                    </div>
                </div>
                <div class="progress-bar-container">
                    <div class="progress-bar">
                        <div id="progress-fill" class="progress-fill"></div>
                    </div>
                    <div id="progress-percent" style="width: 40px; text-align: right;">0%</div>
                </div>
                <div class="argument-display" id="argument-display">
                    <p>Select a running debate to see real-time progress...</p>
                </div>
            </div>
        </div>
    </div>

    <!-- Query History Tab (Phase 1) -->
    <div id="history" class="tab-content">
        <div class="panel" style="height: 100%;">
            <div class="history-container">
                <div class="history-header">
                    <h2>Query History & Search</h2>
                    <input type="text" id="search-input" placeholder="Search queries..." onkeyup="searchHistory(this.value)">
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
                            <tr><td colspan="6" style="text-align: center; padding: 20px;">Loading history...</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    </div>

    <!-- Metrics Tab -->
    <div id="metrics" class="tab-content grid-2">
        <div class="panel">
            <h2>Overall Statistics</h2>
            <div id="stats-container">
                <div class="stat">
                    <div class="stat-value" id="total-debates">0</div>
                    <div class="stat-label">Total Debates</div>
                </div>
                <div class="stat">
                    <div class="stat-value" id="success-rate">0%</div>
                    <div class="stat-label">Success Rate</div>
                </div>
                <div class="stat">
                    <div class="stat-value" id="avg-duration">0ms</div>
                    <div class="stat-label">Average Duration</div>
                </div>
                <div class="stat">
                    <div class="stat-value" id="total-tokens">0</div>
                    <div class="stat-label">Total Tokens</div>
                </div>
            </div>
        </div>
        <div class="panel">
            <h2>Provider Performance</h2>
            <div id="provider-stats"></div>
        </div>
        <div class="panel" style="grid-column: 1 / -1;">
            <h2>Provider Latency Comparison</h2>
            <div class="chart-container">
                <canvas id="latencyChart"></canvas>
            </div>
        </div>
    </div>

    <script>
        let selectedJobId = null;
        let latencyChart = null;
        let debatePollingInterval = null;
        let allHistory = [];
        let filteredHistory = [];

        function switchTab(tabName) {
            /* Stop debate polling if switching away */
            if (debatePollingInterval) {
                clearInterval(debatePollingInterval);
                debatePollingInterval = null;
            }

            /* Hide all tabs */
            document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));

            /* Show selected tab */
            document.getElementById(tabName).classList.add('active');
            event.target.classList.add('active');

            if (tabName === 'metrics') {
                refreshMetrics();
            } else if (tabName === 'debate-progress' && selectedJobId) {
                startDebatePolling(selectedJobId);
            } else if (tabName === 'history') {
                loadQueryHistory();
            }
        }

        function submitQuery() {
            const query = document.getElementById('query-input').value;
            if (!query.trim()) return;

            fetch('/api/query', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ query, rounds: 4 })
            })
            .then(r => r.json())
            .then(data => {
                selectedJobId = data.job_id;
                pollJob(data.job_id);
            });
        }

        /* Phase 1: Debate Progress Polling */
        function startDebatePolling(jobId) {
            debatePollingInterval = setInterval(() => {
                fetch(`/api/debate/${jobId}/progress`)
                    .then(r => r.json())
                    .then(data => {
                        updateDebateDisplay(data);
                    })
                    .catch(() => {
                        clearInterval(debatePollingInterval);
                        debatePollingInterval = null;
                    });
            }, 500);

            fetch(`/api/debate/${jobId}/progress`)
                .then(r => r.json())
                .then(data => updateDebateDisplay(data));
        }

        function updateDebateDisplay(data) {
            document.getElementById('round-counter').textContent =
                `Round ${data.current_round}/${data.total_rounds}`;

            document.getElementById('analyst-count').textContent =
                `Analysts: ${data.analyst_count}`;

            const percent = data.progress_percent || 0;
            document.getElementById('progress-fill').style.width = percent + '%';
            document.getElementById('progress-percent').textContent = percent + '%';

            let argText = `Debate Progress\n\n`;
            argText += `Query: ${data.query}\n`;
            argText += `Provider: ${data.provider}\n`;
            argText += `Duration: ${data.duration_ms}ms\n`;
            argText += `Status: ${['Running', 'Complete', 'Failed'][data.status]}\n`;
            argText += `Progress: ${percent}%\n`;
            document.getElementById('argument-display').textContent = argText;

            if (data.status !== 0) {
                if (debatePollingInterval) {
                    clearInterval(debatePollingInterval);
                    debatePollingInterval = null;
                }
            }
        }

        /* Phase 1: Query History Functions */
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
            if (filteredHistory.length === 0) {
                tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; padding: 20px;">No queries found</td></tr>';
                return;
            }
            tbody.innerHTML = filteredHistory.map(job => `
                <tr>
                    <td>${job.job_id}</td>
                    <td>${job.query.substring(0, 40)}${job.query.length > 40 ? '...' : ''}</td>
                    <td>${job.provider}</td>
                    <td>${job.duration_ms}</td>
                    <td class="status-${job.success ? 'success' : 'failed'}">
                        ${job.success ? 'Success' : 'Failed'}
                    </td>
                    <td>
                        <button onclick="rerunDebate('${job.query.replace(/'/g, "\\'").replace(/"/g, '\\"')}')">Re-run</button>
                    </td>
                </tr>
            `).join('');
        }

        function rerunDebate(query) {
            document.getElementById('query-input').value = query;
            switchTab('query');
            submitQuery();
        }

        function pollJob(jobId) {
            const poll = setInterval(() => {
                fetch(`/api/query?job_id=${jobId}`)
                    .then(r => r.json())
                    .then(job => {
                        if (job.error) {
                            clearInterval(poll);
                            return;
                        }
                        document.getElementById('output').textContent = job.output;
                        if (job.status !== 0) {
                            clearInterval(poll);
                            refreshJobs();
                        }
                    });
            }, 500);
        }

        function refreshJobs() {
            fetch('/api/tasks')
                .then(r => r.json())
                .then(data => {
                    const jobsDiv = document.getElementById('jobs');
                    jobsDiv.innerHTML = data.tasks.map(job => `
                        <div class="job ${['running', 'complete', 'failed'][job.status]}" onclick="selectJob(${job.id})">
                            <div class="job-name">Query ${job.id}</div>
                            <div class="job-status">Status: ${['Running', 'Complete', 'Failed'][job.status]} (${job.duration_ms}ms)</div>
                        </div>
                    `).join('');
                });
        }

        function selectJob(jobId) {
            selectedJobId = jobId;
            fetch(`/api/query?job_id=${jobId}`)
                .then(r => r.json())
                .then(job => {
                    document.getElementById('output').textContent = job.output;
                });
        }

        function refreshMetrics() {
            fetch('/api/stats')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('total-debates').textContent = data.debates_total;
                    document.getElementById('success-rate').textContent = data.success_rate.toFixed(1) + '%';
                    document.getElementById('avg-duration').textContent = data.duration_avg_ms.toFixed(0) + 'ms';
                    document.getElementById('total-tokens').textContent = data.tokens_total.toLocaleString();

                    /* Provider stats */
                    const providerDiv = document.getElementById('provider-stats');
                    if (data.providers && data.providers.length > 0) {
                        providerDiv.innerHTML = data.providers.map(p => `
                            <div class="stat">
                                <div style="font-weight: 500; color: #0066cc; margin-bottom: 4px;">${p.provider}</div>
                                <div style="font-size: 11px; opacity: 0.7;">
                                    Queries: ${p.queries_total} | Success: ${p.queries_success} | Latency: ${p.latency_avg_ms.toFixed(0)}ms
                                </div>
                            </div>
                        `).join('');

                        /* Update latency chart */
                        updateLatencyChart(data.providers);
                    }
                });
        }

        function updateLatencyChart(providers) {
            const ctx = document.getElementById('latencyChart').getContext('2d');

            if (latencyChart) {
                latencyChart.destroy();
            }

            latencyChart = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: providers.map(p => p.provider),
                    datasets: [
                        {
                            label: 'Avg Latency (ms)',
                            data: providers.map(p => p.latency_avg_ms),
                            backgroundColor: '#0066cc',
                            borderColor: '#0052a3',
                            borderWidth: 1
                        },
                        {
                            label: 'Max Latency (ms)',
                            data: providers.map(p => p.latency_max_ms),
                            backgroundColor: '#ff6b6b',
                            borderColor: '#ff5252',
                            borderWidth: 1
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            labels: { color: '#999', font: { size: 11 } }
                        }
                    },
                    scales: {
                        y: {
                            ticks: { color: '#999' },
                            grid: { color: '#333' }
                        },
                        x: {
                            ticks: { color: '#999' },
                            grid: { color: '#333' }
                        }
                    }
                }
            });
        }

        function exportJSON() {
            const output = document.getElementById('output').textContent;
            const data = { timestamp: new Date().toISOString(), result: output };
            const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
            downloadFile(blob, 'debate_result.json');
        }

        function exportText() {
            const output = document.getElementById('output').textContent;
            const blob = new Blob([output], { type: 'text/plain' });
            downloadFile(blob, 'debate_result.txt');
        }

        function downloadFile(blob, filename) {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }

        setInterval(refreshJobs, 1000);
        refreshJobs();

        /* Load query history on page load */
        loadQueryHistory();
        /* Refresh history every 5 seconds */
        setInterval(loadQueryHistory, 5000);
    </script>
</body>
</html>
)HTML";
}

void HttpServer::handle_metrics(int client) {
    std::string metrics_text = util::Metrics::instance().get_prometheus_text();
    send_response(client, 200, "text/plain; version=0.0.4", metrics_text);
}

void HttpServer::handle_stats(int client) {
    std::string stats_json = util::Metrics::instance().get_json_summary();
    send_response(client, 200, "application/json", stats_json);
}

void HttpServer::handle_history(int client) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;
    ss << "{\n  \"history\": [\n";

    for (size_t i = 0; i < query_history_.size(); i++) {
        const auto& job = query_history_[i];
        if (i > 0) ss << ",\n";
        ss << "    {\n";
        ss << "      \"job_id\": " << job.job_id << ",\n";
        ss << "      \"query\": \"" << job.query << "\",\n";
        ss << "      \"provider\": \"" << job.provider << "\",\n";
        ss << "      \"status\": " << job.status << ",\n";
        ss << "      \"duration_ms\": " << job.duration_ms << ",\n";
        ss << "      \"success\": " << (job.success ? "true" : "false") << "\n";
        ss << "    }";
    }

    ss << "\n  ]\n}\n";
    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_history_search(int client, const std::string& search_text) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;
    ss << "{\n  \"results\": [\n";

    bool first = true;
    for (const auto& job : query_history_) {
        /* Simple substring search */
        if (job.query.find(search_text) != std::string::npos) {
            if (!first) ss << ",\n";
            ss << "    {\n";
            ss << "      \"job_id\": " << job.job_id << ",\n";
            ss << "      \"query\": \"" << job.query << "\",\n";
            ss << "      \"provider\": \"" << job.provider << "\",\n";
            ss << "      \"status\": " << job.status << ",\n";
            ss << "      \"duration_ms\": " << job.duration_ms << "\n";
            ss << "    }";
            first = false;
        }
    }

    ss << "\n  ]\n}\n";
    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_debate_progress(int client, int job_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto job_iter = jobs_.find(job_id);
    if (job_iter == jobs_.end()) {
        send_response(client, 404, "application/json", "{\"error\":\"job not found\"}");
        return;
    }

    const auto& job = job_iter->second;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"job_id\": " << job.job_id << ",\n";
    ss << "  \"status\": " << job.status << ",\n";
    ss << "  \"current_round\": " << job.current_round << ",\n";
    ss << "  \"total_rounds\": " << job.rounds << ",\n";
    ss << "  \"analyst_count\": " << job.analyst_count << ",\n";
    ss << "  \"query\": \"" << job.query << "\",\n";
    ss << "  \"provider\": \"" << job.provider << "\",\n";
    ss << "  \"duration_ms\": " << job.duration_ms << ",\n";
    ss << "  \"progress_percent\": " << (job.rounds > 0 ? (100 * job.current_round / job.rounds) : 0) << "\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_debate_visualization(int client, int job_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto job_iter = jobs_.find(job_id);
    if (job_iter == jobs_.end()) {
        send_response(client, 404, "application/json", "{\"error\":\"job not found\"}");
        return;
    }

    const auto& job = job_iter->second;

    /* Build analysts array */
    std::ostringstream analysts_json;
    analysts_json << "[\n";
    for (size_t i = 0; i < job.analysts.size(); ++i) {
        const auto& analyst = job.analysts[i];
        if (i > 0) analysts_json << ",\n";
        analysts_json << "    {\n";
        analysts_json << "      \"id\": \"" << analyst.id << "\",\n";
        analysts_json << "      \"name\": \"" << analyst.name << "\",\n";
        analysts_json << "      \"provider\": \"" << analyst.model_provider << "\",\n";
        analysts_json << "      \"confidence\": " << analyst.confidence << ",\n";
        analysts_json << "      \"votes\": " << analyst.votes << ",\n";
        analysts_json << "      \"eliminated_round\": " << analyst.eliminated_round << ",\n";
        analysts_json << "      \"argument_strength\": " << analyst.argument_strength << "\n";
        analysts_json << "    }\n";
    }
    analysts_json << "  ]";

    /* Build full response */
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"job_id\": " << job.job_id << ",\n";
    ss << "  \"query\": \"" << job.query << "\",\n";
    ss << "  \"status\": " << job.status << ",\n";
    ss << "  \"current_round\": " << job.current_round << ",\n";
    ss << "  \"total_rounds\": " << job.rounds << ",\n";
    ss << "  \"provider\": \"" << job.provider << "\",\n";
    ss << "  \"duration_ms\": " << job.duration_ms << ",\n";
    ss << "  \"analysts\": " << analysts_json.str() << ",\n";
    ss << "  \"winner\": {\n";
    ss << "    \"analyst_id\": \"analysis_pending\",\n";
    ss << "    \"confidence\": 0\n";
    ss << "  }\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_metrics_trends(int client, const std::string& params) {
    /* Parse hours parameter (default 24) */
    int hours = 24;
    size_t h_pos = params.find("hours=");
    if (h_pos != std::string::npos) {
        try {
            hours = std::stoi(params.substr(h_pos + 6));
        } catch (...) {}
    }

    auto& metrics = util::Metrics::instance();
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"hours\": " << hours << ",\n";
    ss << "  \"providers\": {\n";

    /* Collect provider data */
    auto all_stats = metrics.get_all_stats();
    for (size_t i = 0; i < all_stats.size(); ++i) {
        if (i > 0) ss << ",\n";
        const auto& stats = all_stats[i];
        auto percentiles = metrics.get_percentiles(stats.provider, hours);
        ss << "    \"" << stats.provider << "\": {\n";
        ss << "      \"latencies\": [" << percentiles.p50 << ", " << percentiles.p95 << ", " << percentiles.p99 << "],\n";
        ss << "      \"success_rate\": " << (stats.queries_total > 0 ? (100.0 * stats.queries_success / stats.queries_total) : 0.0) << "\n";
        ss << "    }\n";
    }
    ss << "  }\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_metrics_percentiles(int client, const std::string& params) {
    /* Parse provider and hours parameters */
    std::string provider = "ollama";
    int hours = 24;

    size_t p_pos = params.find("provider=");
    if (p_pos != std::string::npos) {
        size_t end = params.find("&", p_pos);
        provider = params.substr(p_pos + 9, (end == std::string::npos) ? std::string::npos : (end - p_pos - 9));
    }

    size_t h_pos = params.find("hours=");
    if (h_pos != std::string::npos) {
        try {
            hours = std::stoi(params.substr(h_pos + 6));
        } catch (...) {}
    }

    auto& metrics = util::Metrics::instance();
    auto percentiles = metrics.get_percentiles(provider, hours);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"provider\": \"" << provider << "\",\n";
    ss << "  \"hours\": " << hours << ",\n";
    ss << "  \"p50\": " << percentiles.p50 << ",\n";
    ss << "  \"p95\": " << percentiles.p95 << ",\n";
    ss << "  \"p99\": " << percentiles.p99 << "\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_metrics_tokens(int client, const std::string& params) {
    /* Parse hours parameter */
    int hours = 0;
    size_t h_pos = params.find("hours=");
    if (h_pos != std::string::npos) {
        try {
            hours = std::stoi(params.substr(h_pos + 6));
        } catch (...) {}
    }

    auto& metrics = util::Metrics::instance();
    auto histogram = metrics.get_token_distribution(hours);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"hours\": " << hours << ",\n";
    ss << "  \"bins\": [";
    for (size_t i = 0; i < histogram.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << histogram[i];
    }
    ss << "],\n";
    ss << "  \"labels\": [";
    for (size_t i = 0; i < histogram.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << (i*500) << "-" << ((i+1)*500) << "\"";
    }
    ss << "]\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_metrics_models(int client, const std::string& params) {
    /* Parse hours parameter */
    int hours = 24;
    size_t h_pos = params.find("hours=");
    if (h_pos != std::string::npos) {
        try {
            hours = std::stoi(params.substr(h_pos + 6));
        } catch (...) {}
    }

    auto& metrics = util::Metrics::instance();
    auto all_stats = metrics.get_all_stats();

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"hours\": " << hours << ",\n";
    ss << "  \"models\": [\n";
    for (size_t i = 0; i < all_stats.size(); ++i) {
        if (i > 0) ss << ",\n";
        const auto& stats = all_stats[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << stats.provider << "\",\n";
        ss << "      \"provider\": \"" << stats.provider << "\",\n";
        ss << "      \"avg_latency\": " << stats.latency_ms_avg << ",\n";
        ss << "      \"success_rate\": " << (stats.queries_total > 0 ? (100.0 * stats.queries_success / stats.queries_total) : 0.0) << ",\n";
        ss << "      \"count\": " << stats.queries_total << "\n";
        ss << "    }\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

void HttpServer::handle_metrics_timeseries(int client, const std::string& params) {
    /* Parse hours and provider parameters */
    int hours = 24;
    std::string provider = "ollama";

    size_t h_pos = params.find("hours=");
    if (h_pos != std::string::npos) {
        try {
            hours = std::stoi(params.substr(h_pos + 6));
        } catch (...) {}
    }

    size_t p_pos = params.find("provider=");
    if (p_pos != std::string::npos) {
        size_t end = params.find("&", p_pos);
        provider = params.substr(p_pos + 9, (end == std::string::npos) ? std::string::npos : (end - p_pos - 9));
    }

    auto& metrics = util::Metrics::instance();
    auto timeseries = metrics.get_timeseries(provider, hours);
    auto all_stats = metrics.get_all_stats();

    /* Calculate throughput and error rate */
    int total_queries = 0;
    int failed_queries = 0;
    for (const auto& stats : all_stats) {
        total_queries += stats.queries_total;
        failed_queries += stats.queries_failed;
    }

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"provider\": \"" << provider << "\",\n";
    ss << "  \"hours\": " << hours << ",\n";
    ss << "  \"timestamps\": [";
    for (size_t i = 0; i < timeseries.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << timeseries[i].timestamp_ms;
    }
    ss << "],\n";
    ss << "  \"latencies\": [";
    for (size_t i = 0; i < timeseries.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << timeseries[i].value;
    }
    ss << "],\n";
    ss << "  \"throughput_qpm\": " << (total_queries / std::max(1, hours/60)) << ",\n";
    ss << "  \"error_rate\": " << (total_queries > 0 ? (100.0 * failed_queries / total_queries) : 0.0) << "\n";
    ss << "}\n";

    send_response(client, 200, "application/json", ss.str());
}

}  /* namespace http */
}  /* namespace tribunal */
