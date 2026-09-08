#ifndef TRIBUNAL_LLM_CLIENT_H
#define TRIBUNAL_LLM_CLIENT_H

/**
 * llm/LLMClient.h - Abstract LLM Client Interface
 *
 * Phase 2.1: Pluggable interface for LLM backends
 *
 * Defines the common interface for all LLM providers (Ollama, Claude, vLLM, OpenAI).
 * Concrete implementations inherit and override virtual methods.
 *
 * Design:
 * - Pure virtual base class (abstract interface)
 * - Query/streaming methods
 * - Model enumeration
 * - Error handling via exceptions
 * - Configuration via constructor
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <memory>

namespace tribunal {
namespace llm {

/**
 * PromptRequest - Parameters for a query to the LLM
 */
struct PromptRequest {
    std::string model;                      /* Model name (e.g., "llama3.2") */
    std::string system_prompt;              /* System prompt (role/context) */
    std::string user_message;               /* User prompt/query */
    float temperature = 0.7f;               /* Randomness (0.0-1.0) */
    int max_tokens = 4096;                  /* Max response length */
    std::vector<std::string> stop_sequences;  /* Strings that stop generation */
};

/**
 * PromptResponse - Response from LLM query
 */
struct PromptResponse {
    std::string text;                       /* Generated text */
    int tokens_used = 0;                    /* Tokens consumed */
    bool truncated = false;                 /* Hit max_tokens limit? */
    std::string error;                      /* Error message if failed */
};

/**
 * StreamCallback - Called when streaming response data arrives
 *
 * @param chunk  Partial response text
 * @param done   true when stream is complete
 */
using StreamCallback = std::function<void(const std::string& chunk, bool done)>;

/**
 * LLMClient - Abstract base class for LLM backends
 *
 * Subclasses implement specific providers (OllamaClient, ClaudeClient, etc.)
 * allowing pluggable backend switching.
 *
 * All methods are thread-safe (implementations must handle synchronization).
 */
class LLMClient {
public:
    virtual ~LLMClient() = default;

    /**
     * query - Execute a single LLM query
     *
     * Sends a prompt and waits for complete response.
     *
     * @param request  Prompt parameters (model, system, user message)
     * @return  Response with generated text and metadata
     * @throws std::runtime_error on network/API errors
     */
    virtual PromptResponse query(const PromptRequest& request) = 0;

    /**
     * query_streaming - Execute query with streaming response
     *
     * Calls callback as chunks arrive from the LLM.
     * Useful for real-time feedback.
     *
     * @param request   Prompt parameters
     * @param callback  Called with each chunk (chunk, done)
     * @return  Final aggregated response
     * @throws std::runtime_error on network/API errors
     */
    virtual PromptResponse query_streaming(
        const PromptRequest& request,
        const StreamCallback& callback
    ) = 0;

    /**
     * available_models - Get list of available models on this backend
     *
     * @return  Vector of model names (e.g., ["llama3.2", "mistral"])
     * @throws std::runtime_error if models cannot be fetched
     */
    virtual std::vector<std::string> available_models() = 0;

    /**
     * get_model_info - Get metadata about a specific model
     *
     * @param model  Model name
     * @return  Info object (or std::nullopt if model not found)
     */
    virtual std::optional<std::string> get_model_info(const std::string& model) = 0;

    /**
     * is_available - Check if backend is reachable and working
     *
     * Lightweight connectivity check (e.g., ping).
     *
     * @return  true if backend responds, false if unreachable
     */
    virtual bool is_available() = 0;

    /**
     * get_name - Human-readable name of this backend
     *
     * @return  Name like "Ollama", "Claude API", "vLLM"
     */
    virtual std::string get_name() const = 0;

    /**
     * get_provider_type - Machine-readable provider identifier
     *
     * @return  Provider name like "ollama", "claude", "openai", "vllm"
     */
    virtual std::string get_provider_type() const = 0;

    /**
     * supports_streaming - Whether this provider supports streaming
     *
     * @return  true if query_streaming is implemented
     */
    virtual bool supports_streaming() const {
        return false;
    }

    /**
     * supports_batch - Whether this provider supports batch API
     *
     * @return  true if batch_query is implemented
     */
    virtual bool supports_batch() const {
        return false;
    }

    /**
     * get_last_error - Error message from most recent failed operation
     *
     * @return  Error string (or empty if no error)
     */
    virtual std::string get_last_error() const {
        return "";
    }
};

/**
 * ClientFactory - Creates LLM client instances
 *
 * Factory pattern for constructing the right client based on provider type.
 */
class ClientFactory {
public:
    /**
     * create - Create client for given provider (legacy simple version)
     *
     * @param provider_type  "ollama", "claude", "openai", "vllm"
     * @param config         Configuration (URL, API key, etc.)
     * @return  Unique pointer to new client
     * @throws std::invalid_argument if provider type unknown
     */
    static std::unique_ptr<LLMClient> create(
        const std::string& provider_type,
        const std::string& config
    );

    /**
     * create_from_config - Create client with detailed parameters
     *
     * Extended factory for creating clients with full configuration details.
     * Supports claude, google-agy, and ollama providers.
     *
     * @param api_type           Provider type: "ollama", "claude", "google-agy"
     * @param claude_api_key     Claude API key (optional)
     * @param claude_model       Claude model name (optional, default: claude-3-5-sonnet-20241022)
     * @param google_agy_api_key Google Antigravity API key (optional)
     * @param google_agy_model   Google Antigravity model name (optional)
     * @param ollama_url         Ollama server URL (optional, default: http://localhost:11434)
     * @param timeout_sec        Request timeout in seconds (default: 120)
     * @return  Unique pointer to new client
     * @throws std::invalid_argument if api_type unknown or required config missing
     */
    static std::unique_ptr<LLMClient> create_from_config(
        const std::string& api_type,
        const std::string& claude_api_key = "",
        const std::string& claude_model = "",
        const std::string& google_agy_api_key = "",
        const std::string& google_agy_model = "",
        const std::string& ollama_url = "",
        int timeout_sec = 120
    );
};

}  /* namespace llm */
}  /* namespace tribunal */

#endif /* TRIBUNAL_LLM_CLIENT_H */
