#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <cmath>
#include <sstream>
#include "thread_pool.h"

// 全局互斥锁，用于同步控制台输出
std::mutex cout_mutex;

// 辅助函数：打印分隔线
void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

// 测试1：基础任务提交（无返回值）
void test_basic_tasks() {
    print_separator("Test 1: Basic Task Submission");

    ThreadPool pool(4);
    std::cout << "Thread pool created with " << pool.get_thread_count() << " threads\n" << std::endl;

    for (int i = 0; i < 8; ++i) {
        pool.enqueue([i] {
            // 使用互斥锁保护输出，避免多线程输出混乱
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "  Task " << i << " executed by worker #"
                      << ThreadPool::get_worker_index() << std::endl;
        });
    }

    pool.wait_all();
    std::cout << "\nAll tasks completed. Total executed: " << pool.get_tasks_completed() << std::endl;
}

// 测试2：有返回值的任务
void test_tasks_with_return() {
    print_separator("Test 2: Tasks with Return Values");

    ThreadPool pool(4);
    std::vector<std::future<int>> results;

    for (int i = 0; i < 10; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                return i * i;
            })
        );
    }

    std::cout << "Results: ";
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << results[i].get();
        if (i < results.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
}

// 测试3：带参数的任务
void test_tasks_with_parameters() {
    print_separator("Test 3: Tasks with Parameters");

    ThreadPool pool(2);

    auto multiply = [](int a, int b) { return a * b; };
    auto add = [](int a, int b) { return a + b; };

    std::future<int> r1 = pool.enqueue(multiply, 7, 8);
    std::future<int> r2 = pool.enqueue(add, 100, 200);
    std::future<int> r3 = pool.enqueue([](int x) { return x * x * x; }, 5);

    std::cout << "7 * 8 = " << r1.get() << std::endl;
    std::cout << "100 + 200 = " << r2.get() << std::endl;
    std::cout << "5^3 = " << r3.get() << std::endl;
}

// 测试4：parallel_for 并行循环
void test_parallel_for() {
    print_separator("Test 4: Parallel For Loop");

    ThreadPool pool(4);
    const size_t N = 16;
    std::vector<int> data(N, 0);

    pool.parallel_for(0, N, [&](size_t i) {
        data[i] = static_cast<int>(i * i);
    });

    std::cout << "Squares computed in parallel:" << std::endl;
    for (size_t i = 0; i < N; ++i) {
        std::cout << std::setw(3) << i << "^2 = " << std::setw(4) << data[i];
        if ((i + 1) % 4 == 0) std::cout << std::endl;
        else std::cout << "  |  ";
    }
}

// 测试5：parallel_reduce 并行归约
void test_parallel_reduce() {
    print_separator("Test 5: Parallel Reduce");

    ThreadPool pool(4);
    const size_t N = 100;

    // 计算 1 + 2 + ... + N
    long long sum = pool.parallel_reduce<long long>(
        1, N + 1,
        0LL,
        [](size_t i) { return static_cast<long long>(i); },
        [](long long a, long long b) { return a + b; }
    );

    long long expected = static_cast<long long>(N) * (N + 1) / 2;
    std::cout << "Sum of 1 to " << N << " = " << sum << std::endl;
    std::cout << "Expected: " << expected << " -> "
              << (sum == expected ? "PASS" : "FAIL") << std::endl;

    // 计算 1 * 2 * 3 * ... * 10 (阶乘)
    long long factorial = pool.parallel_reduce<long long>(
        1, 11,
        1LL,
        [](size_t i) { return static_cast<long long>(i); },
        [](long long a, long long b) { return a * b; }
    );
    std::cout << "10! = " << factorial << " (expected: 3628800) -> "
              << (factorial == 3628800 ? "PASS" : "FAIL") << std::endl;
}

// 测试6：暂停和恢复
void test_pause_resume() {
    print_separator("Test 6: Pause and Resume");

    ThreadPool pool(2);
    std::atomic<int> counter{0};

    // 提交一些任务
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([&counter, i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ++counter;
        });
    }

    std::cout << "Tasks submitted: 10" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    pool.pause();
    std::cout << "Pool paused. Is paused: " << (pool.is_paused() ? "Yes" : "No") << std::endl;
    int count_when_paused = counter.load();
    std::cout << "Tasks completed when paused: " << count_when_paused << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After 100ms pause, completed: " << counter.load()
              << " (should be same as before)" << std::endl;

    pool.resume();
    std::cout << "Pool resumed. Is paused: " << (pool.is_paused() ? "Yes" : "No") << std::endl;

    pool.wait_all();
    std::cout << "All tasks completed: " << counter.load() << std::endl;
}

// 测试7：动态重设线程数
void test_reset() {
    print_separator("Test 7: Dynamic Thread Count Reset");

    ThreadPool pool(2);
    std::cout << "Initial thread count: " << pool.get_thread_count() << std::endl;

    // 提交一些任务
    for (int i = 0; i < 4; ++i) {
        pool.enqueue([i] {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "  Task " << i << " executed by worker #"
                      << ThreadPool::get_worker_index() << std::endl;
        });
    }
    pool.wait_all();

    // 重设为4个线程
    pool.reset(4);
    std::cout << "\nAfter reset, thread count: " << pool.get_thread_count() << std::endl;

    for (int i = 0; i < 4; ++i) {
        pool.enqueue([i] {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "  Task " << i << " on resized pool, worker #"
                      << ThreadPool::get_worker_index() << std::endl;
        });
    }
    pool.wait_all();
}

// 测试8：查询接口
void test_monitoring() {
    print_separator("Test 8: Monitoring Interface");

    ThreadPool pool(4);

    std::cout << "Thread count: " << pool.get_thread_count() << std::endl;
    std::cout << "Tasks queued (before): " << pool.get_tasks_queued() << std::endl;
    std::cout << "Tasks total (before): " << pool.get_tasks_total() << std::endl;
    std::cout << "Tasks completed (before): " << pool.get_tasks_completed() << std::endl;

    // 提交一些任务
    for (int i = 0; i < 20; ++i) {
        pool.enqueue([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }

    std::cout << "\nAfter submitting 20 tasks:" << std::endl;
    std::cout << "Tasks queued: " << pool.get_tasks_queued() << std::endl;
    std::cout << "Tasks total: " << pool.get_tasks_total() << std::endl;

    pool.wait_all();

    std::cout << "\nAfter wait_all():" << std::endl;
    std::cout << "Tasks queued: " << pool.get_tasks_queued() << std::endl;
    std::cout << "Tasks total: " << pool.get_tasks_total() << std::endl;
    std::cout << "Tasks completed: " << pool.get_tasks_completed() << std::endl;
}

// 测试9：性能基准测试
void test_performance() {
    print_separator("Test 9: Performance Benchmark");

    const size_t N = 1000000;
    std::vector<double> data(N);

    // 串行版本
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        data[i] = std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto serial_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Serial computation: " << serial_time << " ms" << std::endl;

    // 并行版本
    ThreadPool pool;
    std::cout << "Using " << pool.get_thread_count() << " threads" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    pool.parallel_for(0, N, [&data](size_t i) {
        data[i] = std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
    });
    end = std::chrono::high_resolution_clock::now();
    auto parallel_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Parallel computation: " << parallel_time << " ms" << std::endl;

    if (parallel_time > 0) {
        double speedup = static_cast<double>(serial_time) / parallel_time;
        std::cout << "Speedup: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
    }
}

// 测试10：异常处理
void test_exception_handling() {
    print_separator("Test 10: Exception Handling");

    ThreadPool pool(2);

    auto future = pool.enqueue([] {
        throw std::runtime_error("Test exception from task");
        return 42;
    });

    try {
        int result = future.get();
        std::cout << "Result: " << result << " (should not reach here)" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
        std::cout << "Exception handling works correctly!" << std::endl;
    }

    // 确保线程池仍然可用
    auto future2 = pool.enqueue([] { return 100; });
    std::cout << "Pool still works after exception: " << future2.get() << std::endl;
}

int main() {
    std::cout << "\n";
    std::cout << "  ╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "  ║     ADVANCED THREAD POOL - COMPREHENSIVE TEST SUITE   ║\n";
    std::cout << "  ╚═══════════════════════════════════════════════════════╝\n";

    test_basic_tasks();
    test_tasks_with_return();
    test_tasks_with_parameters();
    test_parallel_for();
    test_parallel_reduce();
    test_pause_resume();
    test_reset();
    test_monitoring();
    test_performance();
    test_exception_handling();

    print_separator("ALL TESTS COMPLETED SUCCESSFULLY");
    std::cout << "\nThread Pool Features Demonstrated:" << std::endl;
    std::cout << "  [x] Basic task submission" << std::endl;
    std::cout << "  [x] Tasks with return values (std::future)" << std::endl;
    std::cout << "  [x] Tasks with parameters" << std::endl;
    std::cout << "  [x] Parallel for loop" << std::endl;
    std::cout << "  [x] Parallel reduce" << std::endl;
    std::cout << "  [x] Pause and resume" << std::endl;
    std::cout << "  [x] Dynamic thread count reset" << std::endl;
    std::cout << "  [x] Monitoring interface" << std::endl;
    std::cout << "  [x] Performance benchmark" << std::endl;
    std::cout << "  [x] Exception handling" << std::endl;

    std::cout << "\n按任意键退出..." << std::endl;
    std::cin.get();

    return 0;
}