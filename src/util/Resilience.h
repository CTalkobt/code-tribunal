#ifndef TRIBUNAL_RESILIENCE_H
#define TRIBUNAL_RESILIENCE_H

/**
 * util/Resilience.h - Retry Logic and Circuit Breaker Patterns
 *
 * Production-grade resilience for transient failures:
 * - Exponential backoff with jitter for retries
 * - Circuit breaker to prevent cascading failures
 * - Timeout management for hanging operations
 * - Graceful degradation strategies
 */

#pragma once

#include <string>
#include <functional>
#include <chrono>
#include <stdexcept>

namespace tribunal {
namespace util {

/**
 * RetryPolicy - Configuration for retry behavior
 */
struct RetryPolicy {
    int max_attempts = 3;                    /* Maximum retry attempts */
    int initial_delay_ms = 100;              /* Initial backoff in ms */
    int max_delay_ms = 5000;                 /* Max backoff cap */
    double backoff_multiplier = 2.0;         /* Exponential backoff factor */
    bool jitter_enabled = true;              /* Add randomness to delays */

    static RetryPolicy aggressive() {
        return {5, 50, 2000, 2.0, true};
    }

    static RetryPolicy moderate() {
        return {3, 100, 5000, 2.0, true};
    }

    static RetryPolicy conservative() {
        return {2, 500, 10000, 1.5, false};
    }
};

/**
 * Retryable - Execute function with exponential backoff and retries
 *
 * @param fn          Function to execute (returns bool success)
 * @param policy      Retry policy configuration
 * @param error_msg   Output error message if all retries fail
 * @return true if succeeded, false if exhausted retries
 */
bool retryable(
    const std::function<bool()>& fn,
    const RetryPolicy& policy = RetryPolicy::moderate(),
    std::string* error_msg = nullptr
);

/**
 * CircuitBreaker - Prevent cascading failures by stopping requests
 *
 * States: Closed (OK) → Open (failing) → Half-Open (testing) → Closed
 * - Closed: requests pass through normally
 * - Open: requests fail immediately without trying
 * - Half-Open: allow limited requests to test recovery
 */
class CircuitBreaker {
public:
    /**
     * Constructor - Initialize circuit breaker
     *
     * @param failure_threshold  Failures before opening (default: 5)
     * @param recovery_timeout   Milliseconds before half-open (default: 30000)
     * @param success_threshold  Successes in half-open to close (default: 2)
     */
    CircuitBreaker(
        int failure_threshold = 5,
        int recovery_timeout_ms = 30000,
        int success_threshold = 2
    );

    /**
     * call - Execute function through circuit breaker
     *
     * @param fn  Function to execute
     * @return Result of fn, or throws if circuit is open
     * @throws std::runtime_error if circuit breaker is open
     */
    template<typename F>
    auto call(F fn) -> decltype(fn()) {
        if (is_open()) {
            throw std::runtime_error("Circuit breaker is open: service unavailable");
        }

        try {
            auto result = fn();
            record_success();
            return result;
        } catch (const std::exception& e) {
            record_failure(e.what());
            throw;
        }
    }

    /**
     * is_open - Check if circuit is currently open
     */
    bool is_open() const;

    /**
     * get_state - Get human-readable circuit state
     *
     * @return "closed", "open", or "half-open"
     */
    std::string get_state() const;

    /**
     * reset - Manually reset circuit breaker
     */
    void reset();

private:
    enum State { Closed, Open, HalfOpen };

    int failure_threshold_;
    int recovery_timeout_ms_;
    int success_threshold_;

    State state_ = Closed;
    int failure_count_ = 0;
    int success_count_ = 0;
    std::chrono::high_resolution_clock::time_point last_failure_time_;

    void record_success();
    void record_failure(const std::string& error);
};

/**
 * TimeoutGuard - RAII guard for operation timeouts
 *
 * Tracks elapsed time and throws if timeout exceeded.
 */
class TimeoutGuard {
public:
    /**
     * Constructor - Start timeout timer
     *
     * @param timeout_ms  Timeout duration in milliseconds
     * @param name        Operation name for error messages
     */
    TimeoutGuard(int timeout_ms, const std::string& name = "operation");

    /**
     * check - Throw if timeout exceeded
     *
     * @throws std::runtime_error if timeout exceeded
     */
    void check() const;

    /**
     * elapsed - Get milliseconds elapsed
     */
    long long elapsed_ms() const;

    /**
     * remaining - Get milliseconds remaining
     */
    long long remaining_ms() const;

private:
    int timeout_ms_;
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_time_;
};

}  /* namespace util */
}  /* namespace tribunal */

#endif /* TRIBUNAL_RESILIENCE_H */
