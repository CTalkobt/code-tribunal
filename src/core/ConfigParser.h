#ifndef TRIBUNAL_CONFIG_PARSER_H
#define TRIBUNAL_CONFIG_PARSER_H

/**
 * core/ConfigParser.h - Configuration File Parser
 *
 * Reads key=value configuration from council.conf and populates Configuration struct.
 *
 * Features:
 * - Parses INI-style config files (key=value format)
 * - Supports multiple values (repeated key=value lines)
 * - Ignores comments (lines starting with #)
 * - Provides sensible defaults for missing options
 * - Allows CLI arguments to override config file settings
 *
 * Usage:
 *   core::Configuration config = core::ConfigParser::load("config/council.conf");
 *   // CLI args can override:
 *   config.rounds = 6;  // CLI override
 */

#pragma once

#include "types.h"
#include <string>
#include <optional>

namespace tribunal {
namespace core {

/**
 * ConfigParser - Loads and parses council.conf configuration files
 */
class ConfigParser {
public:
    /**
     * load - Load configuration from file
     *
     * @param config_path  Path to council.conf (default: config/council.conf)
     * @return Configuration struct populated from file, with defaults for missing values
     *
     * If file doesn't exist or is unreadable, returns default Configuration
     * with all default values intact.
     */
    static Configuration load(const std::string& config_path = "config/council.conf");

    /**
     * load_or_default - Load configuration, fallback to defaults
     *
     * @param config_path  Path to configuration file
     * @return Configuration with values from file or defaults
     *
     * Never fails - returns defaults if file not found.
     */
    static Configuration load_or_default(const std::string& config_path = "config/council.conf");

private:
    /**
     * parse_line - Parse a single key=value line
     *
     * @param line  Line to parse (may contain comments)
     * @param key   Output: extracted key
     * @param value Output: extracted value
     * @return true if valid key=value found, false if empty or comment
     */
    static bool parse_line(const std::string& line, std::string& key, std::string& value);

    /**
     * set_config_value - Set a configuration value by key
     *
     * @param config Configuration struct to update
     * @param key    Configuration key (e.g., "rounds", "model", "ollama_url")
     * @param value  String value to parse and set
     */
    static void set_config_value(Configuration& config, const std::string& key, const std::string& value);

    /**
     * trim - Remove leading/trailing whitespace
     *
     * @param str  String to trim
     * @return Trimmed string
     */
    static std::string trim(const std::string& str);

    /**
     * apply_env_overrides - Apply environment variable overrides for API keys
     *
     * Environment variables take precedence over config file values:
     * - CLAUDE_API_KEY → claude_api_key (if empty from config)
     * - GOOGLE_AGY_API_KEY → google_agy_api_key (if empty from config)
     * - GOOGLE_AGY_MODEL → google_agy_model (if empty from config)
     *
     * @param config Configuration struct to update with env vars
     */
    static void apply_env_overrides(Configuration& config);
};

}  /* namespace core */
}  /* namespace tribunal */

#endif /* TRIBUNAL_CONFIG_PARSER_H */
