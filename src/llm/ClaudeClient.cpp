#include "ClaudeClient.h"
#include <curl/curl.h>
#include <sstream>
#include <cstdlib>

namespace tribunal {
namespace llm {

/* Callback for CURL response writing */
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

ClaudeClient::ClaudeClient(const std::string& api_key, const std::string& model, int timeout_sec)
    : api_key_(api_key), model_(model), timeout_sec_(timeout_sec) {
    /* If no API key provided, try environment variable */
    if (api_key_.empty()) {
        const char* env_key = std::getenv("CLAUDE_API_KEY");
        if (env_key) {
            api_key_ = env_key;
        }
    }
}

PromptResponse ClaudeClient::query(const PromptRequest& request) {
    try {
        std::string payload = build_messages_request(request);
        std::string response = make_request("/messages", "POST", payload);
        return parse_messages_response(response);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return PromptResponse{"", 0, false, last_error_};
    }
}

PromptResponse ClaudeClient::query_streaming(const PromptRequest& request, const StreamCallback& callback) {
    /* For now, fall back to non-streaming query and call callback on completion */
    PromptResponse response = query(request);
    if (response.error.empty()) {
        callback(response.text, true);
    }
    return response;
}

std::vector<std::string> ClaudeClient::available_models() {
    return {
        "claude-3-opus-20240229",
        "claude-3-sonnet-20240229",
        "claude-3-haiku-20240307",
        "claude-3-5-sonnet-20241022",
    };
}

std::optional<std::string> ClaudeClient::get_model_info(const std::string& model) {
    auto models = available_models();
    for (const auto& m : models) {
        if (m == model) {
            return "Claude model: " + model;
        }
    }
    return std::nullopt;
}

bool ClaudeClient::is_available() {
    if (api_key_.empty()) {
        last_error_ = "No API key configured (set CLAUDE_API_KEY or claude_api_key in config)";
        return false;
    }

    try {
        /* Quick availability check: list models */
        available_models();
        return true;
    } catch (...) {
        return false;
    }
}

std::string ClaudeClient::make_request(const std::string& endpoint, const std::string& method, const std::string& payload) {
    if (api_key_.empty()) {
        throw std::runtime_error("Claude API key not configured");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    std::string url = "https://api.anthropic.com/v1" + endpoint;

    /* Set up headers */
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "x-api-key: " + api_key_;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    /* Set method and payload */
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    } else if (method == "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    }

    /* Execute request */
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string error = "CURL error: " + std::string(curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw std::runtime_error(error);
    }

    /* Check HTTP response code */
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (response_code != 200) {
        throw std::runtime_error("Claude API returned HTTP " + std::to_string(response_code) + ": " + response);
    }

    return response;
}

std::string ClaudeClient::build_messages_request(const PromptRequest& request) {
    /* Build JSON request for Claude Messages API */
    std::stringstream ss;
    ss << "{"
       << "\"model\":\"" << request.model << "\","
       << "\"max_tokens\":" << request.max_tokens << ","
       << "\"system\":\"" << request.system_prompt << "\","
       << "\"messages\":[{"
       << "\"role\":\"user\","
       << "\"content\":\"" << request.user_message << "\""
       << "}]"
       << "}";
    return ss.str();
}

PromptResponse ClaudeClient::parse_messages_response(const std::string& response) {
    /* Simple JSON parsing for response */
    PromptResponse result;

    /* Look for "text" field in response (simplified parsing) */
    size_t text_pos = response.find("\"text\":\"");
    if (text_pos != std::string::npos) {
        size_t start = text_pos + 8;
        size_t end = response.find("\"", start);
        if (end != std::string::npos) {
            result.text = response.substr(start, end - start);
        }
    }

    /* Try to extract token usage */
    size_t usage_pos = response.find("\"usage\":");
    if (usage_pos != std::string::npos) {
        size_t input_pos = response.find("\"input_tokens\":", usage_pos);
        if (input_pos != std::string::npos) {
            size_t output_pos = response.find("\"output_tokens\":", usage_pos);
            if (output_pos != std::string::npos) {
                try {
                    int input_tokens = std::stoi(response.substr(input_pos + 15));
                    int output_tokens = std::stoi(response.substr(output_pos + 16));
                    result.tokens_used = input_tokens + output_tokens;
                } catch (...) {}
            }
        }
    }

    return result;
}

}  /* namespace llm */
}  /* namespace tribunal */
