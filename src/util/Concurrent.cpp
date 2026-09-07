#include "Concurrent.h"
#include <thread>
#include <stdexcept>

namespace tribunal {
namespace util {

size_t get_cpu_count() {
    size_t count = std::thread::hardware_concurrency();
    return count > 0 ? count : 1;
}

size_t get_optimal_thread_count() {
    size_t count = get_cpu_count();
    return count > 1 ? count - 1 : 2;
}

ThreadPool::ThreadPool(size_t num_threads) : num_threads_(num_threads) {
    if (num_threads_ == 0) {
        num_threads_ = get_optimal_thread_count();
    }

    /* Create worker threads */
    for (size_t i = 0; i < num_threads_; i++) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("Cannot enqueue task: ThreadPool is stopped");
        }
        tasks_.push(task);
    }
    queue_cv_.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    done_cv_.wait(lock, [this]() {
        return tasks_.empty() && active_tasks_ == 0;
    });
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    queue_cv_.notify_all();

    /* Join all worker threads */
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t ThreadPool::thread_count() const {
    return num_threads_;
}

size_t ThreadPool::pending_tasks() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
    return tasks_.size() + active_tasks_.load();
}

bool ThreadPool::is_running() const {
    return !stop_;
}

void ThreadPool::worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        /* Wait for task or stop signal */
        queue_cv_.wait(lock, [this]() {
            return !tasks_.empty() || stop_;
        });

        /* Check stop before exiting if queue is empty */
        if (tasks_.empty()) {
            if (stop_) {
                break;
            }
            continue;
        }

        /* Get task from queue */
        auto task = std::move(tasks_.front());
        tasks_.pop();

        active_tasks_++;
        lock.unlock();

        /* Execute task (without holding lock) */
        try {
            task();
        } catch (...) {
            /* Suppress exceptions in worker threads */
        }

        active_tasks_--;

        /* Notify if all work is done */
        if (tasks_.empty() && active_tasks_ == 0) {
            done_cv_.notify_all();
        }
    }
}

}  /* namespace util */
}  /* namespace tribunal */
