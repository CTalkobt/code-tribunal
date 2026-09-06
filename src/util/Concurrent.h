#ifndef TRIBUNAL_CONCURRENT_H
#define TRIBUNAL_CONCURRENT_H

/**
 * util/Concurrent.h - Thread utilities and synchronization primitives
 *
 * Phase 1.3.2: C++ wrappers around pthreads for cleaner concurrency
 *
 * Features:
 * - ThreadPool: Worker thread pool for parallel task execution
 * - Mutex wrappers (use std::mutex directly)
 * - ConditionVariable wrappers (use std::condition_variable)
 * - Thread-safe queue for work distribution
 *
 * Design: Replace manual pthread_create/pthread_join with
 * higher-level abstractions that integrate with C++ RAII.
 */

#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <optional>

namespace tribunal {
namespace util {

/**
 * ThreadPool - Worker thread pool for parallel task execution
 *
 * Distributes work across N worker threads. Scales based on CPU count.
 *
 * Usage:
 *   ThreadPool pool(4);  // 4 worker threads
 *   pool.enqueue([](){ do_work(); });
 *   pool.enqueue([](){ do_more_work(); });
 *   pool.wait();  // Wait for all work to complete
 *   // pool destroyed -> workers joined
 */
class ThreadPool {
public:
    /**
     * Constructor - Create thread pool with N workers
     *
     * @param num_threads  Number of worker threads (0 = auto-detect CPU count)
     */
    explicit ThreadPool(size_t num_threads = 0);

    /**
     * Destructor - Wait for all work to complete, then join all threads
     */
    ~ThreadPool();

    // Non-copyable, non-moveable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Enqueue a task for asynchronous execution
     *
     * @param task  Callable (std::function, lambda, etc.)
     * @throws std::runtime_error if pool is stopped
     */
    void enqueue(std::function<void()> task);

    /**
     * Wait for all currently enqueued tasks to complete
     *
     * Does not stop the pool; tasks can still be enqueued.
     * Blocks until all workers finish current work.
     */
    void wait();

    /**
     * Stop the pool and wait for all workers to finish
     *
     * After stop(), no new tasks can be enqueued.
     * Existing tasks are completed.
     * Called automatically by destructor.
     */
    void stop();

    /**
     * Get number of worker threads
     */
    size_t thread_count() const;

    /**
     * Get number of pending tasks (approximate)
     */
    size_t pending_tasks() const;

    /**
     * Check if pool is running
     */
    bool is_running() const;

private:
    size_t num_threads_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable done_cv_;
    std::atomic<bool> stop_ = false;
    std::atomic<int> active_tasks_ = 0;

    /**
     * Worker thread main loop
     */
    void worker_loop();
};

/**
 * ScopedLock - RAII wrapper for std::unique_lock
 *
 * Automatically locks on construction, unlocks on destruction.
 * Use std::lock_guard or std::unique_lock directly in C++17.
 */
using ScopedLock = std::lock_guard<std::mutex>;

/**
 * get_cpu_count - Get number of available CPU cores
 *
 * @return  Number of cores (at least 1)
 */
size_t get_cpu_count();

/**
 * get_optimal_thread_count - Recommended thread pool size
 *
 * Returns CPU count - 1 (leaving one core free for OS/main thread)
 *
 * @return  Optimal number of worker threads (at least 2)
 */
size_t get_optimal_thread_count();

}  /* namespace util */
}  /* namespace tribunal */

#endif /* TRIBUNAL_CONCURRENT_H */
