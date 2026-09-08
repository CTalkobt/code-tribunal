#ifndef TRIBUNAL_HTTP_SERVER_H
#define TRIBUNAL_HTTP_SERVER_H

/**
 * http/HttpServer.h - C++17 HTTP Server for Code-Tribunal
 *
 * Ported from tribunal_http.c to C++ with integration to C++17 Council API.
 *
 * Features:
 * - HTTP server on port 8080 with embedded React dashboard
 * - Query interface with real-time debate execution
 * - Job tracking for async task execution
 * - Streaming task output with WebSocket-like callbacks
 *
 * Usage:
 *   auto server = std::make_unique<HttpServer>(config);
 *   server->start();  // Blocks until Ctrl+C
 */

#pragma once

#include "../core/types.h"
#include "../core/Council.h"
#include "../llm/LLMClient.h"
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <ctime>

namespace tribunal {
namespace http {

/**
 * AnalystData - Tracks individual analyst information for visualization
 */
struct AnalystData {
    std::string id;           /* Analyst identifier */
    std::string name;         /* Model name */
    std::string model_provider;
    int confidence = 0;       /* Final confidence [0-100] */
    int votes = 0;            /* Final vote count */
    int eliminated_round = -1; /* -1 if not eliminated */
    float argument_strength = 0.0f; /* Average argument strength */
};

/**
 * ExecutionJob - Tracks async query execution with progress
 */
struct ExecutionJob {
    int job_id = -1;
    int status = 0;           /* 0=running, 1=complete, 2=failed */
    std::string query;
    std::string output;
    std::string error;
    std::string provider;
    int rounds = 4;
    int current_round = 0;    /* For progress tracking */
    int analyst_count = 0;    /* Active analysts */
    std::vector<AnalystData> analysts; /* Individual analyst tracking */
    std::time_t start_time = 0;
    int duration_ms = 0;
    bool success = false;
};

/**
 * HttpServer - C++17 HTTP server with embedded React dashboard
 *
 * Serves:
 * - GET /           - React dashboard HTML
 * - GET /api/tasks  - List pending/completed jobs (JSON)
 * - POST /api/query - Execute debate query asynchronously (JSON)
 * - GET /api/query?job_id=X - Poll job status and output (JSON)
 * - GET /metrics    - Prometheus metrics (text format)
 * - GET /api/stats  - JSON metrics summary
 */
class HttpServer {
public:
    /**
     * Constructor - Initialize HTTP server
     *
     * @param config      Configuration (rounds, models)
     * @param llm_client  LLM client for queries
     */
    HttpServer(
        const core::Configuration& config,
        std::unique_ptr<llm::LLMClient> llm_client
    );

    /**
     * Destructor - Clean up socket and threads
     */
    ~HttpServer();

    /**
     * start - Start HTTP server (blocking, runs until SIGINT)
     *
     * @return 0 on clean shutdown, non-zero on error
     */
    int start();

    /**
     * stop - Stop HTTP server (can be called from another thread)
     */
    void stop();

private:
    core::Configuration config_;
    std::unique_ptr<llm::LLMClient> llm_client_;
    std::unique_ptr<core::CouncilOrchestrator> council_;

    int server_socket_ = -1;
    bool running_ = false;
    std::mutex mutex_;
    std::map<int, ExecutionJob> jobs_;
    int next_job_id_ = 1;
    std::vector<ExecutionJob> query_history_;  /* Session-based query history */

    /**
     * server_thread - Main HTTP server loop
     */
    void server_thread();

    /**
     * handle_request - Process HTTP request
     *
     * @param client  Connected socket
     */
    void handle_request(int client);

    /**
     * parse_request - Parse HTTP request headers
     *
     * @param buffer  Raw request data
     * @param method  HTTP method (GET/POST)
     * @param path    Request path
     * @param body    Request body (for POST)
     * @return true if parsed successfully
     */
    bool parse_request(
        const std::string& buffer,
        std::string& method,
        std::string& path,
        std::string& body
    );

    /**
     * send_response - Send HTTP response to client
     *
     * @param client      Socket
     * @param status_code HTTP status (200, 404, etc.)
     * @param content_type Content-Type header
     * @param body        Response body
     */
    void send_response(
        int client,
        int status_code,
        const std::string& content_type,
        const std::string& body
    );

    /**
     * handle_root - GET / - Dashboard HTML
     */
    void handle_root(int client);

    /**
     * handle_tasks - GET /api/tasks - List jobs
     */
    void handle_tasks(int client);

    /**
     * handle_query - POST /api/query - Execute debate
     *
     * @param client Socket
     * @param body   JSON request body
     */
    void handle_query(int client, const std::string& body);

    /**
     * handle_query_status - GET /api/query?job_id=X - Poll job status
     *
     * @param client Socket
     * @param job_id Job ID from query param
     */
    void handle_query_status(int client, int job_id);

    /**
     * get_dashboard_html - Return embedded React dashboard
     */
    std::string get_dashboard_html() const;

    /**
     * execute_query_async - Run debate in background thread
     *
     * @param job  Job to execute
     */
    void execute_query_async(ExecutionJob job);

    /**
     * allocate_job - Create and track new execution job
     *
     * @param query Request query text
     * @return Job with allocated ID
     */
    ExecutionJob allocate_job(const std::string& query);

    /**
     * find_job - Retrieve job by ID
     *
     * @param job_id Job ID
     * @return Job if found, empty Job if not found
     */
    ExecutionJob find_job(int job_id);

    /**
     * update_job_output - Update job with output
     *
     * @param job_id Job ID
     * @param output Debate result
     * @param status 0=running, 1=complete, 2=failed
     */
    void update_job_output(int job_id, const std::string& output, int status);

    /**
     * handle_metrics - GET /metrics - Prometheus metrics endpoint
     */
    void handle_metrics(int client);

    /**
     * handle_stats - GET /api/stats - JSON metrics summary
     */
    void handle_stats(int client);

    /**
     * handle_history - GET /api/history - Query history with pagination
     */
    void handle_history(int client);

    /**
     * handle_history_search - GET /api/history/search - Search query history
     */
    void handle_history_search(int client, const std::string& query_text);

    /**
     * handle_debate_progress - GET /api/debate/{job_id}/progress - Real-time debate progress
     */
    void handle_debate_progress(int client, int job_id);

    /**
     * handle_debate_visualization - GET /api/debate/{job_id}/visualization - Detailed visualization data
     */
    void handle_debate_visualization(int client, int job_id);
};

}  /* namespace http */
}  /* namespace tribunal */

#endif /* TRIBUNAL_HTTP_SERVER_H */
