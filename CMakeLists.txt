cmake_minimum_required(VERSION 3.8)

project(concurrency)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory(googletest)
add_subdirectory(benchmark)

add_subdirectory(unit_tests)
add_subdirectory(benchmark_tests)
