#ifndef TRIBUNAL_CONFIG_VALIDATOR_H
#define TRIBUNAL_CONFIG_VALIDATOR_H

/**
 * core/ConfigValidator.h - Configuration Validation and Defaults
 *
 * Production-grade validation ensuring:
 * - All required configuration is present
 * - Values are within acceptable ranges
 * - Sensible fallbacks for optional values
 * - Clear error messages for misconfiguration
 */

#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace tribunal {
namespace core {

/**
 * ConfigValidationError - Exception for configuration issues
 */
class ConfigValidationError : public std::runtime_error {
public:
    explicit ConfigValidationError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * ConfigValidator - Validate and normalize configuration
 */
class ConfigValidator {
public:
    /**
     * validate - Validate configuration and throw on errors
     *
     * @param config  Configuration to validate
     * @throws ConfigValidationError if validation fails
     * @return true if valid
     */
    static bool validate(const Configuration& config);

    /**
     * validate_api_type - Ensure api_type is supported
     *
     * @param api_type  Provider type to check
     * @throws ConfigValidationError if unsupported
     * @return true if valid
     */
    static bool validate_api_type(const std::string& api_type);

    /**
     * validate_rounds - Ensure debate rounds are reasonable
     *
     * @param rounds  Number of rounds
     * @throws ConfigValidationError if out of range
     * @return true if valid
     */
    static bool validate_rounds(int rounds);

    /**
     * validate_models - Ensure sufficient models configured
     *
     * @param models  Vector of model names
     * @throws ConfigValidationError if insufficient
     * @return true if valid
     */
    static bool validate_models(const std::vector<std::string>& models);

    /**
     * validate_ollama_urls - Ensure Ollama endpoints are valid
     *
     * @param urls  Vector of Ollama endpoint URLs
     * @throws ConfigValidationError if invalid
     * @return true if valid
     */
    static bool validate_ollama_urls(const std::vector<std::string>& urls);

    /**
     * get_validation_errors - Collect all validation errors
     *
     * @param config  Configuration to check
     * @return Vector of error messages (empty if valid)
     */
    static std::vector<std::string> get_validation_errors(const Configuration& config);

    /**
     * apply_defaults - Apply sensible defaults for unset values
     *
     * @param config  Configuration to update
     * @return Updated configuration with defaults applied
     */
    static Configuration apply_defaults(Configuration config);

private:
    static constexpr int MIN_ROUNDS = 1;
    static constexpr int MAX_ROUNDS = 20;
    static constexpr int MIN_MODELS = 2;
    static constexpr int MAX_MODELS = 10;
};

}  /* namespace core */
}  /* namespace tribunal */

#endif /* TRIBUNAL_CONFIG_VALIDATOR_H */
