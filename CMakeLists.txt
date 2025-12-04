cmake_minimum_required(VERSION 4.0)
project(thread_pool_master)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)
include_directories(my_pool)

add_executable(thread_pool_master
        include/BS_thread_pool.hpp
        modules/BS.thread_pool.cppm
        my_pool/thread_pool.h
        tests/BS_thread_pool_test.cpp
        my_pool/thread_pool.cpp
        my_pool/main.cpp)
