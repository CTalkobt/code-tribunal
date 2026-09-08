#include "ConfigParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace tribunal {
namespace core {

Configuration ConfigParser::load(const std::string& config_path) {
    Configuration config;  /* Start with defaults */

    std::ifstream file(config_path);
    if (!file.is_open()) {
        apply_env_overrides(config);
        return config;  /* Return defaults if file not found */
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string key, value;
        if (parse_line(line, key, value)) {
            set_config_value(config, key, value);
        }
    }

    /* Apply environment variable overrides for sensitive data */
    apply_env_overrides(config);

    return config;
}

Configuration ConfigParser::load_or_default(const std::string& config_path) {
    return load(config_path);  /* load() already returns defaults on failure */
}

bool ConfigParser::parse_line(const std::string& line, std::string& key, std::string& value) {
    /* Trim the line */
    std::string trimmed = trim(line);

    /* Skip empty lines and comments */
    if (trimmed.empty() || trimmed[0] == '#') {
        return false;
    }

    /* Find the equals sign */
    size_t eq_pos = trimmed.find('=');
    if (eq_pos == std::string::npos) {
        return false;  /* No equals sign */
    }

    /* Extract key and value */
    key = trim(trimmed.substr(0, eq_pos));
    value = trim(trimmed.substr(eq_pos + 1));

    /* Remove inline comments from value */
    size_t comment_pos = value.find('#');
    if (comment_pos != std::string::npos) {
        value = trim(value.substr(0, comment_pos));
    }

    return !key.empty() && !value.empty();
}

void ConfigParser::set_config_value(Configuration& config, const std::string& key, const std::string& value) {
    if (key == "rounds") {
        config.rounds = std::stoi(value);
    } else if (key == "election_start") {
        config.election_start = std::stoi(value);
    } else if (key == "election_backoff") {
        config.election_backoff = std::stoi(value);
    } else if (key == "election_max") {
        config.election_max = std::stoi(value);
    } else if (key == "judge_model") {
        config.judge_model = value;
    } else if (key == "spawn_enabled") {
        config.spawn_enabled = std::stoi(value);
    } else if (key == "spawn_check_round") {
        config.spawn_check_round = std::stoi(value);
    } else if (key == "prune_enabled") {
        config.prune_enabled = std::stoi(value);
    } else if (key == "prune_interval") {
        config.prune_interval = std::stoi(value);
    } else if (key == "prune_grace") {
        config.prune_grace = std::stoi(value);
    } else if (key == "min_analysts") {
        config.min_analysts = std::stoi(value);
    } else if (key == "prune_respawn") {
        config.prune_respawn = std::stoi(value);
    } else if (key == "model") {
        config.models.push_back(value);
    } else if (key == "ollama_url") {
        config.ollama_urls.push_back(value);
    } else if (key == "ollama_timeout") {
        config.ollama_timeout = std::stoi(value);
    } else if (key == "api_type") {
        config.api_type = value;
    } else if (key == "ollama_api_url") {
        config.ollama_api_url = value;
    } else if (key == "claude_model") {
        config.claude_model = value;
    } else if (key == "claude_api_key") {
        config.claude_api_key = value;
    } else if (key == "google_agy_model") {
        config.google_agy_model = value;
    } else if (key == "google_agy_api_key") {
        config.google_agy_api_key = value;
    } else if (key == "google_agy_endpoint") {
        config.google_agy_endpoint = value;
    }
    /* Unknown keys are silently ignored for forward compatibility */
}

std::string ConfigParser::trim(const std::string& str) {
    /* Find first non-whitespace character */
    size_t start = 0;
    while (start < str.length() && std::isspace(str[start])) {
        start++;
    }

    /* Find last non-whitespace character */
    size_t end = str.length();
    while (end > start && std::isspace(str[end - 1])) {
        end--;
    }

    return str.substr(start, end - start);
}

std::string ConfigParser::read_claude_cli_credentials() {
    /* Try to read Claude CLI credentials from ~/.claude/.credentials.json */
    const char* home = std::getenv("HOME");
    if (!home) {
        return "";
    }

    std::string credentials_path = std::string(home) + "/.claude/.credentials.json";
    std::ifstream file(credentials_path);
    if (!file.is_open()) {
        return "";
    }

    std::string line;
    while (std::getline(file, line)) {
        /* Look for "accessToken":"..." in JSON */
        size_t token_pos = line.find("\"accessToken\":\"");
        if (token_pos != std::string::npos) {
            /* Extract the token value */
            size_t start = token_pos + 15;  /* len("\"accessToken\":\"") */
            size_t end = line.find("\"", start);
            if (end != std::string::npos) {
                return line.substr(start, end - start);
            }
        }
    }

    return "";
}

void ConfigParser::apply_env_overrides(Configuration& config) {
    /* API key environment variables override config file values */
    /* Priority: CLAUDE_API_KEY > ANTHROPIC_API_KEY > ~/.claude/.credentials.json */
    if (config.claude_api_key.empty()) {
        const char* claude_key = std::getenv("CLAUDE_API_KEY");
        if (claude_key && *claude_key) {
            config.claude_api_key = claude_key;
        } else {
            /* Try ANTHROPIC_API_KEY (Claude CLI standard) */
            const char* anthropic_key = std::getenv("ANTHROPIC_API_KEY");
            if (anthropic_key && *anthropic_key) {
                config.claude_api_key = anthropic_key;
            } else {
                /* Try to read from Claude CLI config */
                std::string cli_key = read_claude_cli_credentials();
                if (!cli_key.empty()) {
                    config.claude_api_key = cli_key;
                }
            }
        }
    }

    const char* google_agy_key = std::getenv("GOOGLE_AGY_API_KEY");
    if (google_agy_key && config.google_agy_api_key.empty()) {
        config.google_agy_api_key = google_agy_key;
    }

    const char* google_agy_model = std::getenv("GOOGLE_AGY_MODEL");
    if (google_agy_model && config.google_agy_model.empty()) {
        config.google_agy_model = google_agy_model;
    }
}

}  /* namespace core */
}  /* namespace tribunal */
