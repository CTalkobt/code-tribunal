#include "GoogleAntigravityClient.h"
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

GoogleAntigravityClient::GoogleAntigravityClient(
    const std::string& api_key,
    const std::string& model,
    const std::string& endpoint,
    int timeout_sec
) : api_key_(api_key), model_(model), endpoint_(endpoint), timeout_sec_(timeout_sec) {
    /* Try environment variables if values not provided */
    if (api_key_.empty()) {
        const char* env_key = std::getenv("GOOGLE_AGY_API_KEY");
        if (env_key) api_key_ = env_key;
    }

    if (model_.empty()) {
        const char* env_model = std::getenv("GOOGLE_AGY_MODEL");
        if (env_model) model_ = env_model;
    }

    if (endpoint_.empty()) {
        const char* env_endpoint = std::getenv("GOOGLE_AGY_ENDPOINT");
        if (env_endpoint) {
            endpoint_ = env_endpoint;
        } else {
            endpoint_ = "https://agy.googleapis.com/v1beta1";
        }
    }
}

PromptResponse GoogleAntigravityClient::query(const PromptRequest& request) {
    try {
        std::string payload = build_request_payload(request);
        std::string response = make_request("/chat/completions", "POST", payload);
        return parse_response(response);
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return PromptResponse{"", 0, false, last_error_};
    }
}

PromptResponse GoogleAntigravityClient::query_streaming(const PromptRequest& request, const StreamCallback& callback) {
    /* For now, fall back to non-streaming and call callback on completion */
    PromptResponse response = query(request);
    if (response.error.empty()) {
        callback(response.text, true);
    }
    return response;
}

std::vector<std::string> GoogleAntigravityClient::available_models() {
    if (!model_.empty()) {
        return {model_};
    }
    /* Return common Antigravity models */
    return {
        "claude-3-5-sonnet-20241022",
        "claude-3-opus-20240229",
        "gpt-4-turbo",
        "llama-3-70b",
    };
}

std::optional<std::string> GoogleAntigravityClient::get_model_info(const std::string& model) {
    auto models = available_models();
    for (const auto& m : models) {
        if (m == model) {
            return "Google Antigravity model: " + model;
        }
    }
    return std::nullopt;
}

bool GoogleAntigravityClient::is_available() {
    if (api_key_.empty()) {
        last_error_ = "No API key configured (set GOOGLE_AGY_API_KEY or google_agy_api_key in config)";
        return false;
    }

    if (model_.empty()) {
        last_error_ = "No model configured (set GOOGLE_AGY_MODEL or google_agy_model in config)";
        return false;
    }

    try {
        /* Quick availability check */
        available_models();
        return true;
    } catch (...) {
        return false;
    }
}

std::string GoogleAntigravityClient::make_request(const std::string& path, const std::string& method, const std::string& payload) {
    if (api_key_.empty()) {
        throw std::runtime_error("Google Antigravity API key not configured");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    std::string url = endpoint_ + path + "?key=" + api_key_;

    /* Set up headers */
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_sec_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    /* Set method and payload */
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
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
        throw std::runtime_error("Google Antigravity API returned HTTP " + std::to_string(response_code) + ": " + response);
    }

    return response;
}

std::string GoogleAntigravityClient::build_request_payload(const PromptRequest& request) {
    /* Build JSON request for Google Antigravity API */
    std::stringstream ss;
    ss << "{"
       << "\"model\":\"" << request.model << "\","
       << "\"max_tokens\":" << request.max_tokens << ","
       << "\"messages\":[{"
       << "\"role\":\"user\","
       << "\"content\":\"" << request.user_message << "\""
       << "}]"
       << "}";
    return ss.str();
}

PromptResponse GoogleAntigravityClient::parse_response(const std::string& response) {
    /* Simple JSON parsing for response */
    PromptResponse result;

    /* Look for content in response (simplified parsing) */
    size_t content_pos = response.find("\"content\":\"");
    if (content_pos != std::string::npos) {
        size_t start = content_pos + 11;
        size_t end = response.find("\"", start);
        if (end != std::string::npos) {
            result.text = response.substr(start, end - start);
        }
    }

    return result;
}

}  /* namespace llm */
}  /* namespace tribunal */
