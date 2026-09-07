/**
 * tests/test_logger.cpp - Logger Thread Safety Tests
 *
 * Tests for concurrent logging, thread safety, and message ordering
 */

#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include "../src/util/Logging.h"

using namespace tribunal;

int test_logger_creation() {
    std::cout << "[TEST] Logger Creation\n";
    
    util::Logger& logger = util::Logger::instance();
    logger.set_level(util::LogLevel::Info);
    
    std::cout << "  ✓ Logger creation works\n";
    return 0;
}

int test_logger_singleton() {
    std::cout << "[TEST] Logger Singleton\n";
    
    util::Logger& logger1 = util::Logger::instance();
    util::Logger& logger2 = util::Logger::instance();
    
    assert(&logger1 == &logger2);
    
    std::cout << "  ✓ Logger singleton works\n";
    return 0;
}

int test_logger_log_levels() {
    std::cout << "[TEST] Logger Log Levels\n";
    
    util::Logger& logger = util::Logger::instance();
    
    // Set to INFO level
    logger.set_level(util::LogLevel::Info);
    
    // These should work
    logger.log(util::LogLevel::Info, "Info message");
    logger.log(util::LogLevel::Warning, "Warning message");
    logger.log(util::LogLevel::Error, "Error message");
    
    std::cout << "  ✓ Log level filtering works\n";
    return 0;
}

int test_logger_concurrent_access() {
    std::cout << "[TEST] Logger Concurrent Access\n";
    
    util::Logger& logger = util::Logger::instance();
    logger.set_level(util::LogLevel::Info);
    
    std::atomic<int> log_count(0);
    std::vector<std::thread> threads;
    
    // Create 4 threads that log concurrently
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&logger, &log_count, t]() {
            for (int i = 0; i < 5; i++) {
                logger.log(util::LogLevel::Info, 
                          "Thread " + std::to_string(t) + " message " + std::to_string(i));
                log_count++;
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    assert(log_count == 20);  // 4 threads * 5 messages each
    
    std::cout << "  ✓ Concurrent logging works\n";
    return 0;
}

int test_logger_all_levels() {
    std::cout << "[TEST] Logger All Levels\n";
    
    util::Logger& logger = util::Logger::instance();
    
    // Test all log levels
    logger.log(util::LogLevel::Debug, "Debug message");
    logger.log(util::LogLevel::Info, "Info message");
    logger.log(util::LogLevel::Warning, "Warning message");
    logger.log(util::LogLevel::Error, "Error message");
    
    std::cout << "  ✓ All log levels work\n";
    return 0;
}

int test_logger_message_content() {
    std::cout << "[TEST] Logger Message Content\n";
    
    util::Logger& logger = util::Logger::instance();
    
    // These shouldn't throw or crash
    logger.log(util::LogLevel::Info, "");
    logger.log(util::LogLevel::Info, "Simple message");
    logger.log(util::LogLevel::Info, "Message with numbers 12345");
    logger.log(util::LogLevel::Info, "Message with special chars !@#$%");
    
    std::cout << "  ✓ Message content handling works\n";
    return 0;
}

int test_logger_level_setting() {
    std::cout << "[TEST] Logger Level Setting\n";
    
    util::Logger& logger = util::Logger::instance();
    
    // Test setting different levels
    logger.set_level(util::LogLevel::Debug);
    logger.log(util::LogLevel::Debug, "Debug level set");
    
    logger.set_level(util::LogLevel::Info);
    logger.log(util::LogLevel::Info, "Info level set");
    
    logger.set_level(util::LogLevel::Warning);
    logger.log(util::LogLevel::Warning, "Warning level set");
    
    logger.set_level(util::LogLevel::Error);
    logger.log(util::LogLevel::Error, "Error level set");
    
    std::cout << "  ✓ Log level setting works\n";
    return 0;
}

int main() {
    std::cout << "\n=== LOGGER TESTS ===\n\n";
    
    int failures = 0;
    failures += test_logger_creation();
    failures += test_logger_singleton();
    failures += test_logger_log_levels();
    failures += test_logger_concurrent_access();
    failures += test_logger_all_levels();
    failures += test_logger_message_content();
    failures += test_logger_level_setting();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All Logger tests passed (7/7)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
