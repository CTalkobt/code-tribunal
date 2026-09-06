#include "LLMClient.h"
#include "OllamaClient.h"
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
        /* TODO: Phase 2.3 - Implement ClaudeClient */
        throw std::invalid_argument("Claude client not yet implemented (Phase 2.3)");
    }
    else if (lower_type == "openai") {
        /* TODO: Future phase - Implement OpenAI client */
        throw std::invalid_argument("OpenAI client not yet implemented");
    }
    else if (lower_type == "vllm") {
        /* TODO: Future phase - Implement vLLM client */
        throw std::invalid_argument("vLLM client not yet implemented");
    }
    else {
        throw std::invalid_argument(
            "Unknown LLM provider: " + provider_type +
            " (supported: ollama, claude, openai, vllm)"
        );
    }
}

}  /* namespace llm */
}  /* namespace tribunal */
