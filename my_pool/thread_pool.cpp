#include "thread_pool.h"
thread_local size_t ThreadPool::worker_index = static_cast<size_t>(-1);



ThreadPool::ThreadPool() : stop(false), paused(false)
{
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) {
        num_threads = 4; // 默认值
    }
    start_workers(num_threads);
}

ThreadPool::ThreadPool(size_t num_threads) : stop(false), paused(false)
{
    if (num_threads == 0) {
        throw std::invalid_argument("线程数必须大于0");
    }
    start_workers(num_threads);
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();

    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::wait_all()
{
    std::unique_lock<std::mutex> lock(tasks_done_mutex);
    tasks_done_cv.wait(lock, [this]() {
        std::lock_guard<std::mutex> queue_lock(queue_mutex);
        return tasks_total == 0 && tasks.empty();
    });
}

void ThreadPool::pause()
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    paused = true;
}

void ThreadPool::resume()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        paused = false;
    }
    condition.notify_all();
}

bool ThreadPool::is_paused() const
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return paused;
}

size_t ThreadPool::get_thread_count() const
{
    return workers.size();
}

size_t ThreadPool::get_tasks_queued() const
{
    std::lock_guard<std::mutex> lock(queue_mutex);
    return tasks.size();
}

size_t ThreadPool::get_tasks_total() const
{
    return tasks_total.load();
}

size_t ThreadPool::get_tasks_completed() const
{
    return tasks_completed.load();
}


void ThreadPool::reset(size_t new_thread_count)
{
    if (new_thread_count == 0) {
        throw std::invalid_argument("new_thread_count must be > 0");
    }

    // 等待当前所有任务完成
    wait_all();

    // 停止现有工作线程
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (auto &t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers.clear();

    // 重置状态并创建新的工作线程
    tasks_total.store(0);
    tasks_completed.store(0);
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = false;
        paused = false;
    }
    start_workers(new_thread_count);
}


void ThreadPool::start_workers(size_t num_threads)
{
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this, i] {
            worker_index = i;
            worker_loop();
        });
    }
}

void ThreadPool::worker_loop()
{
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this]() {
                return stop || (!paused && !tasks.empty());
            });

            if (stop && tasks.empty()) {
                return;
            }

            if (paused && !stop) {
                // 再次进入 wait
                continue;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        task();
        ++tasks_completed;
    }
}

size_t ThreadPool::get_worker_index()
{
    return worker_index;
}