#include "OllamaClient.h"
#include <sstream>
#include <regex>
#include <stdexcept>

namespace tribunal {
namespace llm {

/* libcurl callback for HTTP response body */
static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

OllamaClient::OllamaClient(const std::string& base_url, int timeout)
    : base_url_(base_url), timeout_seconds_(timeout) {
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("Failed to initialize libcurl");
    }
}

OllamaClient::~OllamaClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

std::string OllamaClient::build_request_json(const PromptRequest& req) const {
    std::ostringstream oss;
    oss << R"({"model":")" << req.model << R"(","stream":false,"messages":[)"
        << R"({"role":"system","content":")" << req.system_prompt << R"("},)"
        << R"({"role":"user","content":")" << req.user_message << R"("}])"
        << R"(,"options":{"temperature":)" << req.temperature
        << R"(,"num_predict":)" << req.max_tokens << R"(}})";
    return oss.str();
}

std::string OllamaClient::parse_response_json(const std::string& json_response) const {
    /* Simple JSON extraction: find "content":"..." and extract text */
    std::string pattern = R"("content":")";
    size_t start = json_response.find(pattern);
    if (start == std::string::npos) {
        return "";
    }

    start += pattern.length();
    size_t end = json_response.find('"', start);
    if (end == std::string::npos) {
        return "";
    }

    return json_response.substr(start, end - start);
}

std::string OllamaClient::make_request(
    const std::string& endpoint,
    const std::string& payload,
    bool streaming,
    const StreamCallback& callback
) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, (long)timeout_seconds_);
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, nullptr);

    /* Set up headers */
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    /* Set request body */
    if (!payload.empty()) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, payload.c_str());
    }

    /* Set up response callback */
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

    /* Execute request */
    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        last_error_ = std::string("curl error: ") + curl_easy_strerror(res);
        throw std::runtime_error(last_error_);
    }

    return response;
}

PromptResponse OllamaClient::query(const PromptRequest& request) {
    try {
        std::string payload = build_request_json(request);
        std::string response = make_request("/api/chat", payload, false);
        std::string text = parse_response_json(response);

        return PromptResponse{text, 0, false, ""};
    } catch (const std::exception& e) {
        return PromptResponse{"", 0, false, e.what()};
    }
}

PromptResponse OllamaClient::query_streaming(
    const PromptRequest& request,
    const StreamCallback& callback
) {
    try {
        std::string payload = build_request_json(request);
        std::string response = make_request("/api/chat", payload, true, callback);
        std::string text = parse_response_json(response);

        return PromptResponse{text, 0, false, ""};
    } catch (const std::exception& e) {
        return PromptResponse{"", 0, false, e.what()};
    }
}

std::vector<std::string> OllamaClient::available_models() {
    try {
        std::string response = make_request("/api/tags", "", false);
        std::vector<std::string> models;

        /* Simple JSON parsing: look for "name":"<model_name>" entries */
        std::string pattern = R"("name":")";
        size_t pos = 0;

        while ((pos = response.find(pattern, pos)) != std::string::npos) {
            pos += pattern.length();
            size_t end = response.find('"', pos);
            if (end != std::string::npos) {
                models.push_back(response.substr(pos, end - pos));
                pos = end + 1;
            }
        }

        return models;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        return {};
    }
}

std::optional<std::string> OllamaClient::get_model_info(const std::string& model) {
    try {
        std::string payload = R"({"model":")" + model + R"("})";
        std::string response = make_request("/api/show", payload);
        if (!response.empty()) {
            return response;
        }
    } catch (const std::exception& e) {
        last_error_ = e.what();
    }
    return std::nullopt;
}

bool OllamaClient::is_available() {
    try {
        std::string response = make_request("/api/tags", "");
        return !response.empty();
    } catch (...) {
        return false;
    }
}

}  /* namespace llm */
}  /* namespace tribunal */
