cmake_minimum_required(VERSION 3.22)
project(ScalingMergeSort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

# Aggressive compiler optimizations for HPC benchmarking
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")

include_directories(include)

add_executable(sort_benchmark
    src/main.cpp
    src/serial_sort.cpp
    src/omp_divide_sort.cpp
    src/omp_blelloch_sort.cpp
)

target_link_libraries(sort_benchmark PRIVATE OpenMP::OpenMP_CXX)
