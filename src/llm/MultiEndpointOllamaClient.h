#ifndef TRIBUNAL_MULTI_ENDPOINT_OLLAMA_CLIENT_H
#define TRIBUNAL_MULTI_ENDPOINT_OLLAMA_CLIENT_H

/**
 * llm/MultiEndpointOllamaClient.h - Multi-Endpoint Ollama Client
 *
 * Manages multiple Ollama endpoints with intelligent parallel batching.
 * Recovered from dangling commit ca528eb.
 *
 * **Problem Solved:**
 * Single Ollama instance bottleneck—all analysts queued sequentially.
 *
 * **Solution:**
 * Client-side sequential batching by default, with automatic parallelism
 * when multiple Ollama endpoints are configured.
 *
 * **Behavior:**
 * - No endpoints: defaults to localhost, forces sequential (parallel=1)
 * - Single endpoint: forces sequential (parallel=1) for stability
 * - Multiple endpoints: enables parallelism (parallel=endpoint_count)
 * - Round-robin analyst-to-endpoint assignment for even load distribution
 *
 * **Performance:**
 * - 1 endpoint: no change (sequential, ~4x response_time)
 * - 2 endpoints: ~2x speedup (parallel=2 in batches)
 * - 4 endpoints: ~4x speedup (parallel=4 in batches)
 */

#pragma once

#include "LLMClient.h"
#include "OllamaClient.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

namespace tribunal {
namespace llm {

/**
 * MultiEndpointOllamaClient - Manages multiple Ollama endpoints
 *
 * Distributes queries across multiple endpoints using round-robin assignment.
 * Each analyst gets assigned to an endpoint based on its index:
 *   assigned_endpoint = analyst_index % endpoint_count
 *
 * This ensures even load distribution across available endpoints.
 */
class MultiEndpointOllamaClient : public LLMClient {
public:
    /**
     * Constructor - Initialize with list of Ollama endpoints
     *
     * @param endpoints  Vector of Ollama server URLs
     *                   If empty, defaults to http://localhost:11434
     *                   If one endpoint, forces sequential (parallel=1)
     *                   If multiple endpoints, enables parallelism
     * @param timeout    Request timeout in seconds (default: 120)
     *
     * @example
     *   std::vector<std::string> urls = {
     *     "http://localhost:11434",
     *     "http://localhost:11435"
     *   };
     *   auto client = std::make_unique<MultiEndpointOllamaClient>(urls);
     */
    explicit MultiEndpointOllamaClient(
        const std::vector<std::string>& endpoints = {},
        int timeout = 120
    );

    /**
     * Destructor - Clean up all endpoint clients
     */
    ~MultiEndpointOllamaClient() override = default;

    // Non-copyable
    MultiEndpointOllamaClient(const MultiEndpointOllamaClient&) = delete;
    MultiEndpointOllamaClient& operator=(const MultiEndpointOllamaClient&) = delete;

    /**
     * query - Send prompt using round-robin endpoint assignment
     *
     * Selects endpoint: current_endpoint = query_count++ % endpoint_count
     * This distributes queries evenly across all configured endpoints.
     */
    PromptResponse query(const PromptRequest& request) override;

    /**
     * query_streaming - Send prompt with streaming response
     */
    PromptResponse query_streaming(
        const PromptRequest& request,
        const StreamCallback& callback
    ) override;

    /**
     * available_models - Get list of models from primary endpoint
     */
    std::vector<std::string> available_models() override;

    /**
     * get_model_info - Get metadata about a model from primary endpoint
     */
    std::optional<std::string> get_model_info(const std::string& model) override;

    /**
     * is_available - Check if at least primary endpoint is reachable
     */
    bool is_available() override;

    /**
     * get_name - Return descriptive name based on endpoint count
     */
    std::string get_name() const override {
        return endpoint_count_ > 1 ?
            "Ollama (Multi-Endpoint, parallel=" + std::to_string(endpoint_count_) + ")" :
            "Ollama (Single-Endpoint, sequential)";
    }

    /**
     * get_provider_type - Return "ollama"
     */
    std::string get_provider_type() const override {
        return "ollama";
    }

    /**
     * supports_streaming - Ollama supports streaming
     */
    bool supports_streaming() const override {
        return true;
    }

    /**
     * get_last_error - Error from most recent operation
     */
    std::string get_last_error() const override {
        return last_error_;
    }

    /**
     * get_endpoint_count - Number of configured endpoints
     */
    int get_endpoint_count() const {
        return endpoint_count_;
    }

    /**
     * get_parallelism - Effective parallelism level
     * (1 for sequential, N for N endpoints)
     */
    int get_parallelism() const {
        return endpoint_count_;
    }

    /**
     * get_endpoint_urls - All configured endpoint URLs
     */
    std::vector<std::string> get_endpoint_urls() const;

private:
    std::vector<std::unique_ptr<OllamaClient>> clients_;  /* One client per endpoint */
    int endpoint_count_ = 0;                              /* Number of endpoints */
    std::atomic<int> query_count_{0};                     /* For round-robin selection */
    mutable std::mutex mutex_;                            /* Thread-safe access */
    mutable std::string last_error_;                      /* Most recent error */

    /**
     * select_endpoint - Choose endpoint for this query
     * Uses round-robin: query_count % endpoint_count
     */
    int select_endpoint() {
        return query_count_++ % endpoint_count_;
    }

    /**
     * get_client - Get OllamaClient for endpoint index
     */
    OllamaClient* get_client(int endpoint_index) {
        if (endpoint_index < 0 || endpoint_index >= (int)clients_.size()) {
            return nullptr;
        }
        return clients_[endpoint_index].get();
    }
};

}  /* namespace llm */
}  /* namespace tribunal */

#endif /* TRIBUNAL_MULTI_ENDPOINT_OLLAMA_CLIENT_H */
