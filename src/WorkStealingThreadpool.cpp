#include "WorkStealingThreadpool.h"

#include <chrono>
#include <iostream>
#include <random>

// Initialize thread-local storage
thread_local int WorkStealingThreadPool::worker_id = -1;

// WorkQueue method implementations
void WorkStealingThreadPool::WorkQueue::push(Task task) {
    std::lock_guard<std::mutex> lock(mutex);
    tasks.push_back(std::move(task));
}

bool WorkStealingThreadPool::WorkQueue::pop(Task& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (tasks.empty()) return false;
    task = std::move(tasks.back());
    tasks.pop_back();
    return true;
}

bool WorkStealingThreadPool::WorkQueue::steal(Task& task) {
    std::lock_guard<std::mutex> lock(mutex);
    if (tasks.empty()) return false;
    task = std::move(tasks.front());
    tasks.pop_front();
    return true;
}

bool WorkStealingThreadPool::WorkQueue::empty() {
    std::lock_guard<std::mutex> lock(mutex);
    return tasks.empty();
}

// WorkStealingThreadPool method implementations
void WorkStealingThreadPool::worker_thread(int id) {
    worker_id = id;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, work_queues.size() - 1);

    while (!shutdown.load()) {
        Task task;
        bool found_work = false;

        // Try to get work from own queue first
        if (work_queues[id]->pop(task)) {
            found_work = true;
        } else {
            // Try to steal work from other queues
            for (int attempts = 0; attempts < work_queues.size() * 2;
                 ++attempts) {
                int target = dis(gen);
                if (target != id && work_queues[target]->steal(task)) {
                    found_work = true;
                    break;
                }
            }
        }

        if (found_work) {
            active_workers.fetch_add(1);
            try {
                task();
            } catch (...) {
                // Swallow exceptions to prevent thread termination
            }
            active_workers.fetch_sub(1);
        } else {
            // No work found, wait for notification
            std::unique_lock<std::mutex> lock(work_mutex);
            work_available.wait_for(
                lock, std::chrono::milliseconds(10),
                [this] { return shutdown.load() || has_work(); });
        }
    }
}

bool WorkStealingThreadPool::has_work() const {
    for (const auto& queue : work_queues) {
        if (!queue->empty()) return true;
    }
    return false;
}

int WorkStealingThreadPool::get_worker_id() const {
    return worker_id >= 0 ? worker_id : 0;
}

WorkStealingThreadPool::WorkStealingThreadPool(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;

    work_queues.reserve(num_threads);
    workers.reserve(num_threads);

    // Create work queues
    for (size_t i = 0; i < num_threads; ++i) {
        work_queues.emplace_back(std::make_unique<WorkQueue>());
    }

    // Start worker threads
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(&WorkStealingThreadPool::worker_thread, this, i);
    }
}

WorkStealingThreadPool::~WorkStealingThreadPool() {
    shutdown.store(true);
    work_available.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void WorkStealingThreadPool::wait_for_tasks() {
    while (has_work() || active_workers.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

size_t WorkStealingThreadPool::size() const { return workers.size(); }

// Example usage and demonstration
// void example_usage() {
//     WorkStealingThreadPool pool(4);

//     std::cout << "Thread pool created with " << pool.size() << " threads\n";

//     // Submit tasks that return values
//     std::vector<std::future<int>> results;

//     for (int i = 0; i < 1000; ++i) {
//         results.push_back(pool.submit([i]() {
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//             std::cout << "Task " << i << " executed on thread "
//                       << std::this_thread::get_id() << std::endl;
//             return i * i;
//         }));
//     }

//     // Submit tasks without return values
//     for (int i = 0; i < 10; ++i) {
//         pool.post([i]() {
//             std::this_thread::sleep_for(std::chrono::milliseconds(50));
//             std::cout << "Fire-and-forget task " << i << " completed\n";
//         });
//     }

//     // Collect results
//     std::cout << "Results: ";
//     for (auto& result : results) {
//         std::cout << result.get() << " ";
//     }
//     std::cout << std::endl;

//     // Wait for all tasks to complete
//     pool.wait_for_tasks();
//     std::cout << "All tasks completed\n";
// }

// int main() {
//     example_usage();
//     return 0;
// }
