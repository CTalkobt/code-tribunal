#include "Logging.h"

namespace tribunal {
namespace util {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    instance().min_level_ = level;
}

void Logger::debug(const std::string& msg) {
    log(LogLevel::Debug, msg);
}

void Logger::info(const std::string& msg) {
    log(LogLevel::Info, msg);
}

void Logger::warn(const std::string& msg) {
    log(LogLevel::Warning, msg);
}

void Logger::err(const std::string& msg) {
    log(LogLevel::Error, msg);
}

void Logger::log(LogLevel level, const std::string& msg) {
    instance().log_impl(level, msg);
}

void Logger::log_impl(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < min_level_) {
        return;  /* Below threshold, skip */
    }

    std::string formatted = "[" + timestamp() + "] [" + level_name(level) + "] " + msg;

    /* ERROR goes to stderr, others to stdout */
    if (level == LogLevel::Error) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
}

std::string Logger::level_name(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Silent:  return "NONE";
        default:                return "?";
    }
}

std::string Logger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

}  /* namespace util */
}  /* namespace tribunal */
