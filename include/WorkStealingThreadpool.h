#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class WorkStealingThreadPool {
   private:
    using Task = std::function<void()>;

    struct WorkQueue {
        std::deque<Task> tasks;
        std::mutex mutex;

        // Push task to back (used by owner thread)
        void push(Task task);

        // Pop task from back (used by owner thread)
        bool pop(Task& task);

        // Steal task from front (used by other threads)
        bool steal(Task& task);

        bool empty();
    };

    std::vector<std::unique_ptr<WorkQueue>> work_queues;
    std::vector<std::thread> workers;
    std::atomic<bool> shutdown{false};
    std::condition_variable work_available;
    std::mutex work_mutex;
    std::atomic<int> active_workers{0};

    // Thread-local storage for current worker ID
    static thread_local int worker_id;

    void worker_thread(int id);
    bool has_work() const;
    int get_worker_id() const;

   public:
    explicit WorkStealingThreadPool(
        size_t num_threads = std::thread::hardware_concurrency());

    ~WorkStealingThreadPool();

    // Submit a task and return a future
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();

        // Choose queue based on current thread or round-robin
        int queue_id = get_worker_id();
        if (queue_id >= work_queues.size()) {
            static std::atomic<int> counter{0};
            queue_id = counter.fetch_add(1) % work_queues.size();
        }

        work_queues[queue_id]->push([task]() { (*task)(); });

        // Notify waiting threads
        work_available.notify_one();

        return result;
    }

    // Submit a task without return value
    template <typename F, typename... Args>
    void post(F&& f, Args&&... args) {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        int queue_id = get_worker_id();
        if (queue_id >= work_queues.size()) {
            static std::atomic<int> counter{0};
            queue_id = counter.fetch_add(1) % work_queues.size();
        }

        work_queues[queue_id]->push([task]() { task(); });
        work_available.notify_one();
    }

    // Wait for all tasks to complete
    void wait_for_tasks();

    size_t size() const;
};