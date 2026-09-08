#include "LLMClient.h"
#include "OllamaClient.h"
#include "ClaudeClient.h"
#include "GoogleAntigravityClient.h"
#include <stdexcept>
#include <algorithm>

namespace tribunal {
namespace llm {

std::unique_ptr<LLMClient> ClientFactory::create(
    const std::string& provider_type,
    const std::string& config
) {
    std::string lower_type = provider_type;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);

    if (lower_type == "ollama") {
        /* config = "http://localhost:11434" or empty for default */
        std::string url = config.empty() ? "http://localhost:11434" : config;
        return std::make_unique<OllamaClient>(url);
    }
    else if (lower_type == "claude") {
        /* config = API key (from env CLAUDE_API_KEY if empty) */
        return std::make_unique<ClaudeClient>(config);
    }
    else if (lower_type == "google-agy" || lower_type == "google_agy") {
        /* config = API key (from env GOOGLE_AGY_API_KEY if empty) */
        return std::make_unique<GoogleAntigravityClient>(config);
    }
    else if (lower_type == "openai") {
        throw std::invalid_argument("OpenAI client not yet implemented");
    }
    else if (lower_type == "vllm") {
        throw std::invalid_argument("vLLM client not yet implemented");
    }
    else {
        throw std::invalid_argument(
            "Unknown LLM provider: " + provider_type +
            " (supported: ollama, claude, google-agy)"
        );
    }
}

std::unique_ptr<LLMClient> ClientFactory::create_from_config(
    const std::string& api_type,
    const std::string& claude_api_key,
    const std::string& claude_model,
    const std::string& google_agy_api_key,
    const std::string& google_agy_model,
    const std::string& google_agy_endpoint,
    const std::string& ollama_url,
    int timeout_sec
) {
    std::string lower_type = api_type;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);

    if (lower_type == "ollama") {
        std::string url = ollama_url.empty() ? "http://localhost:11434" : ollama_url;
        return std::make_unique<OllamaClient>(url, timeout_sec);
    }
    else if (lower_type == "claude") {
        std::string model = claude_model.empty() ? "claude-3-5-sonnet-20241022" : claude_model;
        return std::make_unique<ClaudeClient>(claude_api_key, model, timeout_sec);
    }
    else if (lower_type == "google-agy" || lower_type == "google_agy") {
        return std::make_unique<GoogleAntigravityClient>(google_agy_api_key, google_agy_model, google_agy_endpoint, timeout_sec);
    }
    else {
        throw std::invalid_argument(
            "Unknown LLM provider: " + api_type +
            " (supported: ollama, claude, google-agy)"
        );
    }
}

}  /* namespace llm */
}  /* namespace tribunal */
