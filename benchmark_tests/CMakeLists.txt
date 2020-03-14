cmake_minimum_required(VERSION 3.8)

enable_testing()
include_directories(
    ../src
    ../googletest
    ../googletest/include
    ../benchmark
    ../benchmark/include
)

add_executable(
    hash_table_benchmark
    hash_table_benchmark.cpp
)

set_target_properties(hash_table_benchmark PROPERTIES COMPILE_FLAGS "-pthread -std=c++17 -O3")
target_link_libraries(hash_table_benchmark benchmark_main gtest_main)
