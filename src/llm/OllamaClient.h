#ifndef TRIBUNAL_OLLAMA_CLIENT_H
#define TRIBUNAL_OLLAMA_CLIENT_H

/**
 * llm/OllamaClient.h - Ollama HTTP Client Implementation
 *
 * Phase 2.2: Concrete implementation of LLMClient for Ollama
 *
 * Ports existing ollama.c logic to C++ with RAII and STL containers.
 * Communicates with local Ollama server via REST API.
 *
 * Default: http://localhost:11434
 */

#pragma once

#include "LLMClient.h"
#include <curl/curl.h>
#include <mutex>

namespace tribunal {
namespace llm {

/**
 * OllamaClient - HTTP client for Ollama LLM server
 *
 * Implements LLMClient interface using libcurl for HTTP requests.
 * Thread-safe via mutex-protected curl handle.
 *
 * Connection details:
 * - URL: http://localhost:11434/api/chat (default)
 * - Protocol: REST JSON
 * - Streaming: Supported via streaming=true parameter
 *
 * Example:
 *   auto client = std::make_unique<OllamaClient>("http://localhost:11434");
 *   PromptRequest req{"llama3.2", "You are a security expert.", "Find bugs in..."};
 *   auto resp = client->query(req);
 *   std::cout << resp.text << std::endl;
 */
class OllamaClient : public LLMClient {
public:
    /**
     * Constructor - Initialize Ollama client
     *
     * @param base_url  Ollama server URL (default: http://localhost:11434)
     * @param timeout   Request timeout in seconds (default: 120)
     * @throws std::runtime_error if curl initialization fails
     */
    explicit OllamaClient(
        const std::string& base_url = "http://localhost:11434",
        int timeout = 120
    );

    /**
     * Destructor - Clean up curl handle
     */
    ~OllamaClient() override;

    // Non-copyable
    OllamaClient(const OllamaClient&) = delete;
    OllamaClient& operator=(const OllamaClient&) = delete;

    /**
     * query - Send prompt to Ollama and get response
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
     * available_models - Get list of models from Ollama
     */
    std::vector<std::string> available_models() override;

    /**
     * get_model_info - Get metadata about a model
     */
    std::optional<std::string> get_model_info(const std::string& model) override;

    /**
     * is_available - Check if Ollama server is reachable
     */
    bool is_available() override;

    /**
     * get_name - Return "Ollama"
     */
    std::string get_name() const override {
        return "Ollama";
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

private:
    std::string base_url_;              /* Ollama server URL */
    int timeout_seconds_;               /* Request timeout */
    CURL* curl_ = nullptr;              /* libcurl handle (thread-local) */
    mutable std::mutex mutex_;          /* Thread-safe access */
    mutable std::string last_error_;    /* Most recent error */

    /**
     * build_request_json - Create JSON payload for /api/chat
     *
     * @param req  Prompt request
     * @return  JSON-formatted string
     */
    std::string build_request_json(const PromptRequest& req) const;

    /**
     * parse_response_json - Extract text from Ollama response JSON
     *
     * Handles both complete and streaming responses.
     *
     * @param json_response  JSON string from API
     * @return  Extracted text (or error description)
     */
    std::string parse_response_json(const std::string& json_response) const;

    /**
     * make_request - Execute HTTP POST to Ollama
     *
     * @param endpoint  API path ("/api/chat", "/api/tags", etc.)
     * @param payload   JSON payload to send
     * @param streaming  If true, use SSE streaming
     * @param callback   Called on each chunk (if streaming)
     * @return  Response text
     * @throws std::runtime_error on network/parse errors
     */
    std::string make_request(
        const std::string& endpoint,
        const std::string& payload,
        bool streaming = false,
        const StreamCallback& callback = nullptr
    );
};

}  /* namespace llm */
}  /* namespace tribunal */

#endif /* TRIBUNAL_OLLAMA_CLIENT_H */
