#include "Resilience.h"
#include "Logging.h"
#include <thread>
#include <random>
#include <mutex>

namespace tribunal {
namespace util {

bool retryable(
    const std::function<bool()>& fn,
    const RetryPolicy& policy,
    std::string* error_msg
) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> jitter_dist(0, 100);

    int delay_ms = policy.initial_delay_ms;

    for (int attempt = 1; attempt <= policy.max_attempts; attempt++) {
        try {
            if (fn()) {
                return true;
            }
        } catch (const std::exception& e) {
            if (error_msg) {
                *error_msg = e.what();
            }
        }

        if (attempt < policy.max_attempts) {
            /* Calculate backoff with optional jitter */
            int actual_delay = delay_ms;
            if (policy.jitter_enabled) {
                int jitter_percent = jitter_dist(gen);  /* 0-100% */
                actual_delay = (delay_ms * (100 + jitter_percent)) / 100;
            }

            /* Cap delay at maximum */
            if (actual_delay > policy.max_delay_ms) {
                actual_delay = policy.max_delay_ms;
            }

            Logger::instance().log(
                LogLevel::Debug,
                "Retry attempt " + std::to_string(attempt) + "/" + std::to_string(policy.max_attempts) +
                " - waiting " + std::to_string(actual_delay) + "ms"
            );

            std::this_thread::sleep_for(std::chrono::milliseconds(actual_delay));

            /* Calculate next delay */
            delay_ms = static_cast<int>(delay_ms * policy.backoff_multiplier);
        }
    }

    if (error_msg && error_msg->empty()) {
        *error_msg = "All " + std::to_string(policy.max_attempts) + " retry attempts exhausted";
    }

    return false;
}

CircuitBreaker::CircuitBreaker(
    int failure_threshold,
    int recovery_timeout_ms,
    int success_threshold
) : failure_threshold_(failure_threshold),
    recovery_timeout_ms_(recovery_timeout_ms),
    success_threshold_(success_threshold) {
}

bool CircuitBreaker::is_open() const {
    if (state_ == Open) {
        /* Check if recovery timeout expired - transition to half-open */
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_failure_time_
        ).count();

        if (elapsed >= recovery_timeout_ms_) {
            const_cast<CircuitBreaker*>(this)->state_ = HalfOpen;
            const_cast<CircuitBreaker*>(this)->success_count_ = 0;
            Logger::instance().log(LogLevel::Info, "Circuit breaker transitioning to half-open");
            return false;
        }
        return true;
    }
    return false;
}

std::string CircuitBreaker::get_state() const {
    switch (state_) {
        case Closed:
            return "closed";
        case Open:
            return "open";
        case HalfOpen:
            return "half-open";
        default:
            return "unknown";
    }
}

void CircuitBreaker::reset() {
    state_ = Closed;
    failure_count_ = 0;
    success_count_ = 0;
    Logger::instance().log(LogLevel::Info, "Circuit breaker reset to closed");
}

void CircuitBreaker::record_success() {
    if (state_ == HalfOpen) {
        success_count_++;
        if (success_count_ >= success_threshold_) {
            state_ = Closed;
            failure_count_ = 0;
            Logger::instance().log(LogLevel::Info, "Circuit breaker closed after successful recovery");
        }
    } else if (state_ == Closed) {
        failure_count_ = 0;  /* Reset failure count on success */
    }
}

void CircuitBreaker::record_failure(const std::string& error) {
    if (state_ == HalfOpen) {
        /* Failure in half-open returns to open */
        state_ = Open;
        last_failure_time_ = std::chrono::high_resolution_clock::now();
        Logger::instance().log(LogLevel::Warning, "Circuit breaker reopened after failure: " + error);
    } else if (state_ == Closed) {
        failure_count_++;
        if (failure_count_ >= failure_threshold_) {
            state_ = Open;
            last_failure_time_ = std::chrono::high_resolution_clock::now();
            Logger::instance().log(
                LogLevel::Error,
                "Circuit breaker opened after " + std::to_string(failure_count_) + " failures: " + error
            );
        }
    }
}

TimeoutGuard::TimeoutGuard(int timeout_ms, const std::string& name)
    : timeout_ms_(timeout_ms), name_(name),
      start_time_(std::chrono::high_resolution_clock::now()) {
}

void TimeoutGuard::check() const {
    if (elapsed_ms() > timeout_ms_) {
        throw std::runtime_error(
            name_ + " exceeded timeout of " + std::to_string(timeout_ms_) + "ms"
        );
    }
}

long long TimeoutGuard::elapsed_ms() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_
    ).count();
}

long long TimeoutGuard::remaining_ms() const {
    return std::max(0LL, static_cast<long long>(timeout_ms_) - elapsed_ms());
}

}  /* namespace util */
}  /* namespace tribunal */
