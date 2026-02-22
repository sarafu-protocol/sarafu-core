# Benchmarks for Sarafu Blockchain
# Performance benchmarking using Google Benchmark

# Fetch Google Benchmark if not found
find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found, fetching from GitHub...")
    include(FetchContent)
    
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    
    # Disable benchmark tests
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(benchmark)
endif()

# Crypto Benchmarks
# Benchmarks for Blake3 hashing, Ed25519 signature verification,
# and BLS12-381 signature verification and aggregation
add_executable(crypto_benchmarks
    crypto_benchmarks.cpp
)

target_link_libraries(crypto_benchmarks
    PRIVATE
        sarafu_core
        benchmark::benchmark
        benchmark::benchmark_main
)

target_include_directories(crypto_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Transaction Benchmarks
# Benchmarks for transaction validation, serialization, and deserialization
add_executable(transaction_benchmarks
    transaction_benchmarks.cpp
)

target_link_libraries(transaction_benchmarks
    PRIVATE
        sarafu_core
        benchmark::benchmark
        benchmark::benchmark_main
)

target_include_directories(transaction_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Block Benchmarks
# Benchmarks for block validation, QC verification, state root calculation,
# and block serialization/deserialization
add_executable(block_benchmarks
    block_benchmarks.cpp
)

target_link_libraries(block_benchmarks
    PRIVATE
        sarafu_core
        benchmark::benchmark
        benchmark::benchmark_main
)

target_include_directories(block_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Database Benchmarks
# Benchmarks for RocksDB read/write latency, batch write throughput,
# and state query performance
add_executable(database_benchmarks
    database_benchmarks.cpp
)

target_link_libraries(database_benchmarks
    PRIVATE
        sarafu_core
        benchmark::benchmark
        benchmark::benchmark_main
)

target_include_directories(database_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Future benchmark executables will be added here:
# - network_benchmarks: Network message serialization

message(STATUS "Benchmarks configured: crypto_benchmarks, transaction_benchmarks, block_benchmarks, database_benchmarks")
