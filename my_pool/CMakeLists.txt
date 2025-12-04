cmake_minimum_required(VERSION 3.10)
project(ThreadPool CXX)

# 设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(thread_pool_demo
    main.cpp
    thread_pool.cpp
)

# Windows 下需要链接线程库（Linux/Mac 需要 pthread）
if(WIN32)
    # Windows 不需要额外链接
else()
    find_package(Threads REQUIRED)
    target_link_libraries(thread_pool_demo Threads::Threads)
endif()

# 设置输出目录
set_target_properties(thread_pool_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

