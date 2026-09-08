#ifndef TRIBUNAL_GOOGLE_AGY_CLIENT_H
#define TRIBUNAL_GOOGLE_AGY_CLIENT_H

/**
 * llm/GoogleAntigravityClient.h - Google Antigravity API Client
 *
 * Implements LLMClient interface for Google Antigravity models.
 *
 * Requires:
 * - GOOGLE_AGY_API_KEY environment variable or google_agy_api_key config option
 * - GOOGLE_AGY_MODEL environment variable or google_agy_model config option
 * - Optional GOOGLE_AGY_ENDPOINT for custom API endpoint
 */

#pragma once

#include "LLMClient.h"
#include <string>
#include <vector>
#include <memory>

namespace tribunal {
namespace llm {

/**
 * GoogleAntigravityClient - Google Antigravity API wrapper
 *
 * Interfaces with Google Antigravity to query LLMs.
 */
class GoogleAntigravityClient : public LLMClient {
public:
    /**
     * Constructor for Google Antigravity client
     *
     * @param api_key       Google API key (GOOGLE_AGY_API_KEY env if empty)
     * @param model         Model name (GOOGLE_AGY_MODEL env if empty)
     * @param endpoint      Custom endpoint (GOOGLE_AGY_ENDPOINT env if empty)
     * @param timeout_sec   Request timeout in seconds (default: 120)
     */
    GoogleAntigravityClient(
        const std::string& api_key = "",
        const std::string& model = "",
        const std::string& endpoint = "",
        int timeout_sec = 120
    );

    ~GoogleAntigravityClient() override = default;

    /* LLMClient interface implementation */
    PromptResponse query(const PromptRequest& request) override;
    PromptResponse query_streaming(const PromptRequest& request, const StreamCallback& callback) override;
    std::vector<std::string> available_models() override;
    std::optional<std::string> get_model_info(const std::string& model) override;
    bool is_available() override;
    std::string get_name() const override { return "Google Antigravity"; }
    std::string get_provider_type() const override { return "google-agy"; }
    bool supports_streaming() const override { return true; }
    std::string get_last_error() const override { return last_error_; }

private:
    std::string api_key_;
    std::string model_;
    std::string endpoint_;
    int timeout_sec_;
    std::string last_error_;

    /**
     * make_request - Low-level HTTP request to Google Antigravity API
     *
     * @param path      API endpoint path
     * @param method    HTTP method
     * @param payload   JSON payload for POST requests
     * @return  Response body
     * @throws std::runtime_error on network/API errors
     */
    std::string make_request(
        const std::string& path,
        const std::string& method,
        const std::string& payload = ""
    );

    /**
     * build_request_payload - Build JSON payload for API
     *
     * @param request  Prompt request parameters
     * @return  JSON payload string
     */
    std::string build_request_payload(const PromptRequest& request);

    /**
     * parse_response - Extract text from API response
     *
     * @param response  JSON response body
     * @return  PromptResponse with text and metadata
     */
    PromptResponse parse_response(const std::string& response);
};

}  /* namespace llm */
}  /* namespace tribunal */

#endif /* TRIBUNAL_GOOGLE_AGY_CLIENT_H */
