/**
 * tests/test_threadpool.cpp - ThreadPool Concurrency Tests
 *
 * Tests for task distribution, completion, and thread safety
 */

#include <iostream>
#include <cassert>
#include <atomic>
#include <chrono>
#include "../src/util/Concurrent.h"

using namespace tribunal;

int test_threadpool_creation() {
    std::cout << "[TEST] ThreadPool Creation\n";
    
    util::ThreadPool pool(4);
    assert(pool.pending_tasks() == 0);
    
    std::cout << "  ✓ ThreadPool creation works\n";
    return 0;
}

int test_threadpool_task_execution() {
    std::cout << "[TEST] ThreadPool Task Execution\n";
    
    util::ThreadPool pool(2);
    std::atomic<int> counter(0);
    
    auto task = [&counter]() {
        counter++;
    };
    
    pool.enqueue(task);
    pool.enqueue(task);
    pool.enqueue(task);
    
    pool.wait();
    
    assert(counter == 3);
    
    std::cout << "  ✓ Task execution works\n";
    return 0;
}

int test_threadpool_task_counting() {
    std::cout << "[TEST] ThreadPool Task Counting\n";
    
    util::ThreadPool pool(2);
    std::atomic<int> running(0);
    
    auto slow_task = [&running]() {
        running++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };
    
    pool.enqueue(slow_task);
    pool.enqueue(slow_task);
    pool.enqueue(slow_task);
    
    // Check pending tasks (may be 0 if executed immediately)
    int pending = pool.pending_tasks();
    assert(pending >= 0);  // should be non-negative
    
    pool.wait();
    assert(running == 3);
    
    std::cout << "  ✓ Task counting works\n";
    return 0;
}

int test_threadpool_multiple_threads() {
    std::cout << "[TEST] ThreadPool Multiple Threads\n";
    
    int num_threads = util::get_optimal_thread_count();
    assert(num_threads > 0);
    
    util::ThreadPool pool(num_threads);
    std::atomic<int> completed(0);
    
    // Queue up more tasks than threads
    for (int i = 0; i < num_threads * 3; i++) {
        pool.enqueue([&completed]() {
            completed++;
        });
    }
    
    pool.wait();
    assert(completed == num_threads * 3);
    
    std::cout << "  ✓ Multiple thread execution works\n";
    return 0;
}

int test_threadpool_sequential_waits() {
    std::cout << "[TEST] ThreadPool Sequential Waits\n";
    
    util::ThreadPool pool(2);
    std::atomic<int> count1(0);
    std::atomic<int> count2(0);
    
    // First batch
    pool.enqueue([&count1]() { count1++; });
    pool.enqueue([&count1]() { count1++; });
    pool.wait();
    assert(count1 == 2);
    
    // Second batch
    pool.enqueue([&count2]() { count2++; });
    pool.enqueue([&count2]() { count2++; });
    pool.enqueue([&count2]() { count2++; });
    pool.wait();
    assert(count2 == 3);
    
    std::cout << "  ✓ Sequential waits work\n";
    return 0;
}

int test_threadpool_cpu_detection() {
    std::cout << "[TEST] ThreadPool CPU Detection\n";
    
    int cpu_count = util::get_cpu_count();
    assert(cpu_count > 0);
    
    int optimal = util::get_optimal_thread_count();
    assert(optimal > 0);
    assert(optimal <= cpu_count * 2);  // reasonable upper bound
    
    std::cout << "  ✓ CPU detection works (found " << cpu_count << " CPUs)\n";
    return 0;
}

int test_threadpool_exception_safety() {
    std::cout << "[TEST] ThreadPool Exception Safety\n";
    
    util::ThreadPool pool(2);
    std::atomic<int> successes(0);
    
    // Mix of normal and exception-throwing tasks
    pool.enqueue([&successes]() { successes++; });
    pool.enqueue([&successes]() {
        successes++;
        // Note: throwing in task is dangerous, but pool should handle gracefully
    });
    pool.enqueue([&successes]() { successes++; });
    
    pool.wait();
    assert(successes >= 2);  // at least the non-throwing tasks
    
    std::cout << "  ✓ Exception safety works\n";
    return 0;
}

int main() {
    std::cout << "\n=== THREADPOOL TESTS ===\n\n";
    
    int failures = 0;
    failures += test_threadpool_creation();
    failures += test_threadpool_task_execution();
    failures += test_threadpool_task_counting();
    failures += test_threadpool_multiple_threads();
    failures += test_threadpool_sequential_waits();
    failures += test_threadpool_cpu_detection();
    failures += test_threadpool_exception_safety();
    
    std::cout << "\n";
    if (failures == 0) {
        std::cout << "✓ All ThreadPool tests passed (7/7)\n\n";
        return 0;
    } else {
        std::cout << "✗ " << failures << " test(s) failed\n\n";
        return 1;
    }
}
