#ifndef TRIBUNAL_CLAUDE_CLIENT_H
#define TRIBUNAL_CLAUDE_CLIENT_H

/**
 * llm/ClaudeClient.h - Anthropic Claude API Client
 *
 * Implements LLMClient interface for Claude models via Anthropic Messages API.
 * Supports streaming via server-sent events.
 *
 * Models: claude-3-opus-20240229, claude-3-sonnet-20240229, claude-3-haiku-20240307,
 *         claude-3-5-sonnet-20241022 (latest)
 */

#pragma once

#include "LLMClient.h"
#include <string>
#include <vector>
#include <memory>

namespace tribunal {
namespace llm {

/**
 * ClaudeClient - Anthropic Claude API wrapper
 *
 * Uses the Messages API (https://docs.anthropic.com/en/api/messages)
 * to query Claude models.
 */
class ClaudeClient : public LLMClient {
public:
    /**
     * Constructor for Claude API client
     *
     * @param api_key      Anthropic API key (CLAUDE_API_KEY environment variable if empty)
     * @param model        Default model to use (default: claude-3-5-sonnet-20241022)
     * @param timeout_sec  Request timeout in seconds (default: 120)
     */
    ClaudeClient(
        const std::string& api_key = "",
        const std::string& model = "claude-3-5-sonnet-20241022",
        int timeout_sec = 120
    );

    ~ClaudeClient() override = default;

    /* LLMClient interface implementation */
    PromptResponse query(const PromptRequest& request) override;
    PromptResponse query_streaming(const PromptRequest& request, const StreamCallback& callback) override;
    std::vector<std::string> available_models() override;
    std::optional<std::string> get_model_info(const std::string& model) override;
    bool is_available() override;
    std::string get_name() const override { return "Claude API"; }
    std::string get_provider_type() const override { return "claude"; }
    bool supports_streaming() const override { return true; }
    std::string get_last_error() const override { return last_error_; }

private:
    std::string api_key_;
    std::string model_;
    int timeout_sec_;
    std::string last_error_;

    /**
     * make_request - Low-level HTTP request to Claude API
     *
     * @param endpoint   API endpoint path
     * @param method     HTTP method (GET, POST)
     * @param payload    JSON payload for POST requests
     * @return  Response body
     * @throws std::runtime_error on network/API errors
     */
    std::string make_request(
        const std::string& endpoint,
        const std::string& method,
        const std::string& payload = ""
    );

    /**
     * build_messages_request - Build JSON payload for Messages API
     *
     * @param request  Prompt request parameters
     * @return  JSON payload string
     */
    std::string build_messages_request(const PromptRequest& request);

    /**
     * parse_messages_response - Extract text from Messages API response
     *
     * @param response  JSON response body
     * @return  PromptResponse with text and metadata
     */
    PromptResponse parse_messages_response(const std::string& response);
};

}  /* namespace llm */
}  /* namespace tribunal */

#endif /* TRIBUNAL_CLAUDE_CLIENT_H */
