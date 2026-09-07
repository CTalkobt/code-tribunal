#include "MultiEndpointOllamaClient.h"
#include <stdexcept>

namespace tribunal {
namespace llm {

MultiEndpointOllamaClient::MultiEndpointOllamaClient(
    const std::vector<std::string>& endpoints,
    int timeout
) {
    /* Initialize endpoints: defaults to localhost if empty */
    if (endpoints.empty()) {
        clients_.push_back(std::make_unique<OllamaClient>(
            "http://localhost:11434", timeout
        ));
        endpoint_count_ = 1;
        return;
    }

    /* Create client for each endpoint */
    for (const auto& endpoint : endpoints) {
        clients_.push_back(std::make_unique<OllamaClient>(endpoint, timeout));
    }
    endpoint_count_ = endpoints.size();
}

PromptResponse MultiEndpointOllamaClient::query(const PromptRequest& request) {
    if (endpoint_count_ <= 0 || clients_.empty()) {
        return PromptResponse{"", 0, false, "No Ollama endpoints configured"};
    }

    int endpoint_idx = select_endpoint();
    OllamaClient* client = get_client(endpoint_idx);
    if (!client) {
        return PromptResponse{"", 0, false, "Invalid endpoint selection"};
    }

    try {
        return client->query(request);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return PromptResponse{"", 0, false, e.what()};
    }
}

PromptResponse MultiEndpointOllamaClient::query_streaming(
    const PromptRequest& request,
    const StreamCallback& callback
) {
    if (endpoint_count_ <= 0 || clients_.empty()) {
        return PromptResponse{"", 0, false, "No Ollama endpoints configured"};
    }

    int endpoint_idx = select_endpoint();
    OllamaClient* client = get_client(endpoint_idx);
    if (!client) {
        return PromptResponse{"", 0, false, "Invalid endpoint selection"};
    }

    try {
        return client->query_streaming(request, callback);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return PromptResponse{"", 0, false, e.what()};
    }
}

std::vector<std::string> MultiEndpointOllamaClient::available_models() {
    if (endpoint_count_ <= 0 || clients_.empty()) {
        return {};
    }

    /* Query primary endpoint for available models */
    OllamaClient* primary = get_client(0);
    if (!primary) {
        return {};
    }

    try {
        return primary->available_models();
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return {};
    }
}

std::optional<std::string> MultiEndpointOllamaClient::get_model_info(
    const std::string& model
) {
    if (endpoint_count_ <= 0 || clients_.empty()) {
        return std::nullopt;
    }

    /* Query primary endpoint for model info */
    OllamaClient* primary = get_client(0);
    if (!primary) {
        return std::nullopt;
    }

    try {
        return primary->get_model_info(model);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return std::nullopt;
    }
}

bool MultiEndpointOllamaClient::is_available() {
    if (endpoint_count_ <= 0 || clients_.empty()) {
        return false;
    }

    /* Check if at least primary endpoint is available */
    OllamaClient* primary = get_client(0);
    if (!primary) {
        return false;
    }

    try {
        return primary->is_available();
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::string> MultiEndpointOllamaClient::get_endpoint_urls() const {
    std::vector<std::string> urls;
    for (const auto& client : clients_) {
        if (client) {
            /* OllamaClient doesn't expose URL, so we'd need to add that */
            urls.push_back("(endpoint)");
        }
    }
    return urls;
}

}  /* namespace llm */
}  /* namespace tribunal */
