#ifndef THREAD_POOL_MASTER_THREAD_POOL_H
#define THREAD_POOL_MASTER_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <future>
#include <memory>
#include <atomic>
#include <type_traits>
#include <algorithm>

class ThreadPool
{
public:
    ThreadPool();
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // 等待所有任务完成
    void wait_all();

    // 高级控制：暂停 / 恢复 / 动态重设线程数
    void pause();
    void resume();
    bool is_paused() const;
    void reset(size_t new_thread_count);

    // 查询接口
    size_t get_thread_count() const;
    size_t get_tasks_queued() const;
    size_t get_tasks_total() const;
    size_t get_tasks_completed() const;

    // 模板版 enqueue，支持返回值和参数
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // 批量提交任务
    template<class F, class... Args>
    std::vector<std::future<std::invoke_result_t<F, Args...>>>
    enqueue_batch(size_t count, F&& f, Args&&... args);

    // 简化版 parallel_for
    template<class Func>
    void parallel_for(size_t first, size_t last, Func f);

    // 带块大小的 parallel_for
    template<class Func>
    void parallel_for(size_t first, size_t last, size_t block_size, Func f);

    // parallel_reduce：并行归约
    template<class T, class Func, class ReduceFunc>
    T parallel_reduce(size_t first, size_t last, T init, Func f, ReduceFunc reduce);

    // 当前执行任务所在的线程下标（在线程池工作线程中有效）
    static size_t get_worker_index();

    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;

private:
    void worker_loop();
    void start_workers(size_t num_threads);

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    bool paused;

    std::atomic<size_t> tasks_total{0};
    std::atomic<size_t> tasks_completed{0};
    std::mutex tasks_done_mutex;
    std::condition_variable tasks_done_cv;

    static thread_local size_t worker_index;
};

// 模板成员函数需要在头文件中实现
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    // 创建一个 packaged_task 来包装可调用对象
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // 不允许在停止的线程池中添加任务
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        ++tasks_total;

        // 将任务包装成 void() 形式，并在任务完成后递减计数
        tasks.emplace([this, task]() {
            (*task)();

            size_t remaining = --tasks_total;
            if (remaining == 0) {
                std::unique_lock<std::mutex> done_lock(tasks_done_mutex);
                tasks_done_cv.notify_all();
            }
        });
    }

    // 通知一个等待的线程
    condition.notify_one();
    return result;
}

template<class Func>
void ThreadPool::parallel_for(size_t first, size_t last, Func f)
{
    if (last <= first) {
        return;
    }

    size_t total = last - first;
    size_t thread_count = workers.size();

    if (thread_count == 0) {
        for (size_t i = first; i < last; ++i) {
            f(i);
        }
        return;
    }

    size_t block_size = (total + thread_count - 1) / thread_count;

    std::vector<std::future<void>> futures;
    size_t block_start = first;

    while (block_start < last) {
        size_t block_end = std::min(block_start + block_size, last);

        futures.emplace_back(
            enqueue([block_start, block_end, &f]() {
                for (size_t i = block_start; i < block_end; ++i) {
                    f(i);
                }
            })
        );

        block_start = block_end;
    }

    for (auto& fut : futures) {
        fut.get();
    }
}

// 带块大小的 parallel_for
template<class Func>
void ThreadPool::parallel_for(size_t first, size_t last, size_t block_size, Func f)
{
    if (last <= first) {
        return;
    }

    if (block_size == 0) {
        block_size = 1;
    }

    std::vector<std::future<void>> futures;
    size_t block_start = first;

    while (block_start < last) {
        size_t block_end = std::min(block_start + block_size, last);

        futures.emplace_back(
            enqueue([block_start, block_end, &f]() {
                for (size_t i = block_start; i < block_end; ++i) {
                    f(i);
                }
            })
        );

        block_start = block_end;
    }

    for (auto& fut : futures) {
        fut.get();
    }
}

// 批量提交任务
template<class F, class... Args>
std::vector<std::future<std::invoke_result_t<F, Args...>>>
ThreadPool::enqueue_batch(size_t count, F&& f, Args&&... args)
{
    using return_type = std::invoke_result_t<F, Args...>;
    std::vector<std::future<return_type>> results;
    results.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        results.emplace_back(enqueue(std::forward<F>(f), std::forward<Args>(args)...));
    }

    return results;
}

// parallel_reduce：并行归约
template<class T, class Func, class ReduceFunc>
T ThreadPool::parallel_reduce(size_t first, size_t last, T init, Func f, ReduceFunc reduce)
{
    if (last <= first) {
        return init;
    }

    size_t total = last - first;
    size_t thread_count = workers.size();

    if (thread_count == 0) {
        T result = init;
        for (size_t i = first; i < last; ++i) {
            result = reduce(result, f(i));
        }
        return result;
    }

    size_t block_size = (total + thread_count - 1) / thread_count;

    std::vector<std::future<T>> futures;
    size_t block_start = first;

    while (block_start < last) {
        size_t block_end = std::min(block_start + block_size, last);

        futures.emplace_back(
            enqueue([block_start, block_end, init, &f, &reduce]() {
                T local_result = init;
                for (size_t i = block_start; i < block_end; ++i) {
                    local_result = reduce(local_result, f(i));
                }
                return local_result;
            })
        );

        block_start = block_end;
    }

    T final_result = init;
    for (auto& fut : futures) {
        final_result = reduce(final_result, fut.get());
    }

    return final_result;
}

#endif // THREAD_POOL_MASTER_THREAD_POOL_H