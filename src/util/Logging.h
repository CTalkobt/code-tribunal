#ifndef TRIBUNAL_LOGGING_H
#define TRIBUNAL_LOGGING_H

/**
 * util/Logging.h - Thread-safe structured logging
 *
 * Phase 1.3.1: Centralized logging for debug, info, warning, error messages
 *
 * Features:
 * - Thread-safe output to stdout/stderr
 * - Log levels: DEBUG, INFO, WARNING, ERROR
 * - Timestamp and source location (file:line) in each message
 * - Simple formatted output (printf-like)
 * - Singleton pattern for easy access
 *
 * Usage:
 *   LOG(INFO) << "Processing " << files.size() << " files";
 *   LOG(ERROR) << "Failed to open: " << filename;
 */

#pragma once

#include <iostream>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>

namespace tribunal {
namespace util {

/**
 * LogLevel - Severity of log message
 */
enum class LogLevel {
    Debug = 0,      /* Detailed debugging information */
    Info = 1,       /* General information (default) */
    Warning = 2,    /* Warning conditions */
    Error = 3,      /* Error conditions (serious) */
    Silent = 4      /* Suppress all logging */
};

/**
 * Logger - Thread-safe logging singleton
 *
 * All logging operations are serialized via mutex to prevent interleaved output.
 * Each log message includes timestamp and log level.
 *
 * Usage:
 *   Logger::info("Processing started");
 *   Logger::warn("Low memory: " + std::to_string(bytes) + " bytes");
 *   Logger::err("Failed to open file");
 */
class Logger {
public:
    /**
     * Get singleton instance
     */
    static Logger& instance();

    /**
     * Set minimum log level (messages below this level are dropped)
     */
    static void set_level(LogLevel level);

    /**
     * Log at DEBUG level
     */
    static void debug(const std::string& msg);

    /**
     * Log at INFO level
     */
    static void info(const std::string& msg);

    /**
     * Log at WARNING level
     */
    static void warn(const std::string& msg);

    /**
     * Log at ERROR level
     */
    static void err(const std::string& msg);

    /**
     * Log with explicit level
     */
    static void log(LogLevel level, const std::string& msg);

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::Info;

    /**
     * Internal logging (assumes mutex is held)
     */
    void log_impl(LogLevel level, const std::string& msg);

    /**
     * Get human-readable level name
     */
    std::string level_name(LogLevel level) const;

    /**
     * Get current timestamp as formatted string
     */
    std::string timestamp() const;
};

/**
 * LogStream - Stream-based logging helper
 *
 * Allows convenient stream-style logging:
 *   LOG(INFO) << "Value: " << value << " bytes";
 *
 * Message is flushed when LogStream is destroyed.
 */
class LogStream {
public:
    LogStream(LogLevel level) : level_(level) {}

    ~LogStream() {
        Logger::log(level_, stream_.str());
    }

    template <typename T>
    LogStream& operator<<(const T& value) {
        stream_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::ostringstream stream_;
};

/**
 * Convenience macro for stream-style logging
 * Usage: LOG(INFO) << "message";
 */
#define LOG(level) tribunal::util::LogStream(tribunal::util::LogLevel::level)

}  /* namespace util */
}  /* namespace tribunal */

#endif /* TRIBUNAL_LOGGING_H */
