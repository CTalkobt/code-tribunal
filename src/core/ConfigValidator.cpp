#include "ConfigValidator.h"
#include "../util/Logging.h"
#include <algorithm>
#include <regex>

namespace tribunal {
namespace core {

bool ConfigValidator::validate(const Configuration& config) {
    auto errors = get_validation_errors(config);
    if (!errors.empty()) {
        std::string message = "Configuration validation failed:\n";
        for (const auto& error : errors) {
            message += "  - " + error + "\n";
        }
        throw ConfigValidationError(message);
    }
    return true;
}

bool ConfigValidator::validate_api_type(const std::string& api_type) {
    std::vector<std::string> valid_types = {"ollama", "claude", "google-agy"};
    if (std::find(valid_types.begin(), valid_types.end(), api_type) == valid_types.end()) {
        throw ConfigValidationError(
            "Unsupported api_type: '" + api_type + "'. Supported: ollama, claude, google-agy"
        );
    }
    return true;
}

bool ConfigValidator::validate_rounds(int rounds) {
    if (rounds < MIN_ROUNDS || rounds > MAX_ROUNDS) {
        throw ConfigValidationError(
            "rounds must be between " + std::to_string(MIN_ROUNDS) +
            " and " + std::to_string(MAX_ROUNDS) + ", got " + std::to_string(rounds)
        );
    }
    return true;
}

bool ConfigValidator::validate_models(const std::vector<std::string>& models) {
    if (models.empty()) {
        throw ConfigValidationError("At least one model must be configured");
    }
    if (models.size() < MIN_MODELS) {
        util::Logger::instance().log(
            util::LogLevel::Warning,
            "Only " + std::to_string(models.size()) + " model(s) configured; " +
            std::to_string(MIN_MODELS) + " recommended for debate"
        );
    }
    if (models.size() > MAX_MODELS) {
        throw ConfigValidationError(
            "Too many models: " + std::to_string(models.size()) + " (max: " + std::to_string(MAX_MODELS) + ")"
        );
    }
    return true;
}

bool ConfigValidator::validate_ollama_urls(const std::vector<std::string>& urls) {
    if (urls.empty()) {
        return true;  /* Optional for non-ollama providers */
    }

    /* Basic URL validation */
    std::regex url_pattern("^https?://[a-zA-Z0-9.-]+:\\d{1,5}/?$");

    for (const auto& url : urls) {
        if (!std::regex_match(url, url_pattern)) {
            throw ConfigValidationError("Invalid Ollama URL format: " + url);
        }
    }

    return true;
}

std::vector<std::string> ConfigValidator::get_validation_errors(const Configuration& config) {
    std::vector<std::string> errors;

    /* Check api_type */
    try {
        validate_api_type(config.api_type);
    } catch (const ConfigValidationError& e) {
        errors.push_back(e.what());
    }

    /* Check rounds */
    try {
        validate_rounds(config.rounds);
    } catch (const ConfigValidationError& e) {
        errors.push_back(e.what());
    }

    /* Check models */
    if (config.models.size() < MIN_MODELS) {
        errors.push_back(
            "Insufficient models: " + std::to_string(config.models.size()) +
            " (minimum " + std::to_string(MIN_MODELS) + " required)"
        );
    }

    /* Check Ollama configuration if using Ollama */
    if (config.api_type == "ollama" && config.ollama_urls.empty()) {
        /* Ollama has default localhost, so this is OK */
    } else if (config.api_type == "ollama") {
        try {
            validate_ollama_urls(config.ollama_urls);
        } catch (const ConfigValidationError& e) {
            errors.push_back(e.what());
        }
    }

    /* Check Claude configuration if using Claude */
    if (config.api_type == "claude") {
        if (config.claude_api_key.empty()) {
            errors.push_back("Claude provider selected but claude_api_key not configured");
        }
    }

    /* Check Google Antigravity configuration if using Google AGY */
    if (config.api_type == "google-agy") {
        if (config.google_agy_api_key.empty()) {
            errors.push_back("Google Antigravity provider selected but google_agy_api_key not configured");
        }
    }

    return errors;
}

Configuration ConfigValidator::apply_defaults(Configuration config) {
    /* Set defaults for unset values */
    if (config.rounds == 0) {
        config.rounds = 4;
    }

    if (config.models.empty()) {
        config.models = {"llama3.2", "mistral", "neural-chat", "dolphin-mixtral"};
    }

    if (config.api_type.empty()) {
        config.api_type = "ollama";
    }

    if (config.ollama_urls.empty()) {
        config.ollama_urls.push_back("http://localhost:11434");
    }

    if (config.claude_model.empty()) {
        config.claude_model = "claude-3-5-sonnet-20241022";
    }

    if (config.google_agy_model.empty()) {
        config.google_agy_model = "claude-3-5-sonnet-20241022";
    }

    /* Validate after applying defaults */
    try {
        validate(config);
    } catch (const ConfigValidationError& e) {
        util::Logger::instance().log(util::LogLevel::Error, std::string(e.what()));
    }

    return config;
}

}  /* namespace core */
}  /* namespace tribunal */
