cmake_minimum_required(VERSION 3.20)
project(sarafu-blockchain VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reproducible build options
option(REPRODUCIBLE_BUILD "Enable reproducible builds with deterministic output" OFF)

if(REPRODUCIBLE_BUILD)
    # Add compiler flags for reproducible builds
    add_compile_options(
        -Wdate-time                    # Warn about __DATE__ and __TIME__ macros
        -ffile-prefix-map=${CMAKE_SOURCE_DIR}=.  # Remove absolute paths
        -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=. # Remove absolute paths from macros
    )

    # Add linker flags for reproducible builds
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_link_options(-Wl,--build-id=none)  # Disable build ID
    endif()

    # Set SOURCE_DATE_EPOCH for deterministic timestamps
    if(NOT DEFINED ENV{SOURCE_DATE_EPOCH})
        execute_process(
            COMMAND git log -1 --format=%ct
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SOURCE_DATE_EPOCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT SOURCE_DATE_EPOCH)
            # Fallback to a fixed timestamp if not in git repo
            set(SOURCE_DATE_EPOCH "1609459200")  # 2021-01-01 00:00:00 UTC
        endif()
        set(ENV{SOURCE_DATE_EPOCH} ${SOURCE_DATE_EPOCH})
    endif()
    
    message(STATUS "Reproducible build enabled with SOURCE_DATE_EPOCH=$ENV{SOURCE_DATE_EPOCH}")
    
    
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Code coverage option
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    
    # Add coverage flags for GCC/Clang
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
        
        # Find gcov/llvm-cov
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            find_program(GCOV_PATH gcov)
            if(NOT GCOV_PATH)
                message(WARNING "gcov not found, coverage reports may not work")
            endif()
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            find_program(LLVM_COV_PATH llvm-cov)
            if(NOT LLVM_COV_PATH)
                message(WARNING "llvm-cov not found, coverage reports may not work")
            endif()
        endif()
        
        # Find lcov for HTML report generation
        find_program(LCOV_PATH lcov)
        find_program(GENHTML_PATH genhtml)
        
        if(LCOV_PATH AND GENHTML_PATH)
            message(STATUS "lcov and genhtml found, HTML coverage reports will be available")
        else()
            message(WARNING "lcov or genhtml not found, HTML coverage reports will not be available")
        endif()
    else()
        message(WARNING "Code coverage is only supported with GCC or Clang")
    endif()
endif()

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# Boost 1.73+ has header-only system library
find_package(Boost 1.70 REQUIRED COMPONENTS thread filesystem)
if(Boost_VERSION VERSION_LESS 1.73.0)
    find_package(Boost 1.70 REQUIRED COMPONENTS system)
endif()

# Option to use system packages or fetch content
option(USE_SYSTEM_PROTOBUF "Use system-installed Protobuf" ON)
option(USE_SYSTEM_GRPC "Use system-installed gRPC" ON)
option(USE_SYSTEM_ROCKSDB "Use system-installed RocksDB" ON)
option(USE_SYSTEM_LIBSODIUM "Use system-installed libsodium" ON)

# Protobuf and gRPC - handle together to avoid conflicts
if(USE_SYSTEM_GRPC)
    # gRPC will bring in Protobuf as a dependency
    find_package(gRPC 1.50.0 CONFIG REQUIRED)
    message(STATUS "Using system gRPC (includes Protobuf)")
elseif(USE_SYSTEM_PROTOBUF)
    find_package(Protobuf 3.21.0 REQUIRED)
    message(STATUS "Using system Protobuf: ${Protobuf_VERSION}")
else()
    message(STATUS "Using FetchContent for Protobuf and gRPC")
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG v21.12
        GIT_SHALLOW TRUE
    )
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG v1.50.1
        GIT_SHALLOW TRUE
    )
    set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_CODEGEN ON CACHE BOOL "" FORCE)
    set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
    set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)
    set(gRPC_CARES_PROVIDER "module" CACHE STRING "" FORCE)
    set(gRPC_SSL_PROVIDER "package" CACHE STRING "" FORCE)
    set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
    set(gRPC_RE2_PROVIDER "module" CACHE STRING "" FORCE)
endif()

# RocksDB
if(USE_SYSTEM_ROCKSDB)
    find_package(RocksDB REQUIRED)
    message(STATUS "Using system RocksDB")
else()
    message(STATUS "Using FetchContent for RocksDB")
    FetchContent_Declare(
        rocksdb
        GIT_REPOSITORY https://github.com/facebook/rocksdb.git
        GIT_TAG v7.10.2
        GIT_SHALLOW TRUE
    )
    set(WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
    set(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "" FORCE)
    set(ROCKSDB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(FAIL_ON_WARNINGS OFF CACHE BOOL "" FORCE)
endif()

# libsodium
if(USE_SYSTEM_LIBSODIUM)
    pkg_check_modules(LIBSODIUM REQUIRED libsodium>=1.0.18)
else()
    message(STATUS "Using FetchContent for libsodium")
endif()

# Include FetchContent for dependencies
include(FetchContent)

# Protobuf (if not using system package) - declaration moved to conditional block above
# gRPC (if not using system package) - declaration moved to conditional block above

# RocksDB (if not using system package)
if(NOT USE_SYSTEM_ROCKSDB)
    FetchContent_Declare(
        rocksdb
        GIT_REPOSITORY https://github.com/facebook/rocksdb.git
        GIT_TAG v7.10.2
        GIT_SHALLOW TRUE
    )
    set(WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
    set(WITH_BENCHMARK_TOOLS OFF CACHE BOOL "" FORCE)
    set(ROCKSDB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(FAIL_ON_WARNINGS OFF CACHE BOOL "" FORCE)
endif()

# libp2p (using cpp-libp2p)
FetchContent_Declare(
    libp2p
    GIT_REPOSITORY https://github.com/libp2p/cpp-libp2p.git
    GIT_TAG v0.1.0
    GIT_SHALLOW TRUE
)

# libsodium for Ed25519 (if not using system package)
if(NOT USE_SYSTEM_LIBSODIUM)
    FetchContent_Declare(
        libsodium
        GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
        GIT_TAG 1.0.19-RELEASE
        GIT_SHALLOW TRUE
    )
endif()

# blst for BLS12-381
FetchContent_Declare(
    blst
    GIT_REPOSITORY https://github.com/supranational/blst.git
    GIT_TAG v0.3.11
)

# Blake3
FetchContent_Declare(
    blake3
    GIT_REPOSITORY https://github.com/BLAKE3-team/BLAKE3.git
    GIT_TAG 1.5.0
    SOURCE_SUBDIR c
)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

# RapidCheck for property-based testing
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""  # Disable submodules to avoid cloning issues
)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
set(RC_ENABLE_GMOCK OFF CACHE BOOL "" FORCE)

# Google Benchmark for performance benchmarking
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
    GIT_SHALLOW TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

# Prometheus C++ client library for metrics
FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.2.4
    GIT_SHALLOW TRUE
)
set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)

# nlohmann/json for JSON parsing
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

# cpp-httplib for REST API HTTP server
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
    GIT_SHALLOW TRUE
)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_BROTLI OFF CACHE BOOL "" FORCE)

# Make dependencies available
set(FETCHCONTENT_QUIET OFF)
if(NOT USE_SYSTEM_GRPC AND NOT USE_SYSTEM_PROTOBUF)
    FetchContent_MakeAvailable(protobuf grpc)
endif()
if(NOT USE_SYSTEM_ROCKSDB)
    FetchContent_MakeAvailable(rocksdb)
endif()
if(NOT USE_SYSTEM_LIBSODIUM)
    FetchContent_MakeAvailable(libsodium)
endif()

FetchContent_MakeAvailable(blake3 googletest rapidcheck benchmark prometheus-cpp json httplib)

# Note: libp2p requires special handling and will be integrated separately

# Build blst manually since it uses a custom build system
FetchContent_GetProperties(blst)
if(NOT blst_POPULATED)
    FetchContent_Populate(blst)
    
    # Build blst as a static library
    set(BLST_BUILD_DIR ${CMAKE_BINARY_DIR}/blst-build)
    file(MAKE_DIRECTORY ${BLST_BUILD_DIR})
    
    # Build blst using its build script
    # Set SDK path for macOS
    if(APPLE)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "macOS SDK path: ${MACOS_SDK_PATH}")
        
        # Build blst with explicit SDK path
        execute_process(
            COMMAND /bin/sh -c "SDKROOT='${MACOS_SDK_PATH}' ${blst_SOURCE_DIR}/build.sh"
            WORKING_DIRECTORY ${blst_SOURCE_DIR}
            RESULT_VARIABLE BLST_BUILD_RESULT
        )
    else()
        execute_process(
            COMMAND ${blst_SOURCE_DIR}/build.sh
            WORKING_DIRECTORY ${blst_SOURCE_DIR}
            RESULT_VARIABLE BLST_BUILD_RESULT
        )
    endif()
    
    if(NOT BLST_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build blst library")
    endif()
    
    # Create imported library target
    add_library(blst STATIC IMPORTED)
    set_target_properties(blst PROPERTIES
        IMPORTED_LOCATION ${blst_SOURCE_DIR}/libblst.a
        INTERFACE_INCLUDE_DIRECTORIES ${blst_SOURCE_DIR}/bindings
    )
endif()

# Build blst as static library - commented out for now
# FetchContent_GetProperties(blst)
# if(NOT blst_POPULATED)
#     FetchContent_Populate(blst)
#     add_subdirectory(${blst_SOURCE_DIR} ${blst_BINARY_DIR})
# endif()

# Build libp2p - commented out for now
# FetchContent_GetProperties(libp2p)
# if(NOT libp2p_POPULATED)
#     FetchContent_Populate(libp2p)
#     add_subdirectory(${libp2p_SOURCE_DIR} ${libp2p_BINARY_DIR})
# endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)

# Generate Protocol Buffer + gRPC files
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")
set(PROTO_SRCS "")
set(PROTO_HDRS "")
set(GRPC_SRCS "")
set(GRPC_HDRS "")

if(PROTO_FILES)
    if(USE_SYSTEM_GRPC)
        find_package(Protobuf CONFIG REQUIRED)
    elseif(USE_SYSTEM_PROTOBUF)
        find_package(Protobuf REQUIRED)
    endif()

    if(TARGET protobuf::protoc)
        set(PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
    elseif(Protobuf_PROTOC_EXECUTABLE)
        set(PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE})
    endif()

    if(TARGET gRPC::grpc_cpp_plugin)
        set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    endif()

    if(NOT PROTOC_EXECUTABLE OR NOT GRPC_CPP_PLUGIN_EXECUTABLE)
        message(FATAL_ERROR "protoc or grpc_cpp_plugin not found; cannot generate gRPC sources")
    endif()

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        set(GENERATED_PROTO_SRC "${CMAKE_BINARY_DIR}/generated/${PROTO_NAME}.pb.cc")
        set(GENERATED_PROTO_HDR "${CMAKE_BINARY_DIR}/generated/${PROTO_NAME}.pb.h")
        set(GENERATED_GRPC_SRC "${CMAKE_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.cc")
        set(GENERATED_GRPC_HDR "${CMAKE_BINARY_DIR}/generated/${PROTO_NAME}.grpc.pb.h")

        add_custom_command(
            OUTPUT
                ${GENERATED_PROTO_SRC}
                ${GENERATED_PROTO_HDR}
                ${GENERATED_GRPC_SRC}
                ${GENERATED_GRPC_HDR}
            COMMAND ${PROTOC_EXECUTABLE}
            ARGS
                --grpc_out "${CMAKE_BINARY_DIR}/generated"
                --cpp_out "${CMAKE_BINARY_DIR}/generated"
                --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
                -I "${CMAKE_SOURCE_DIR}/proto"
                "${PROTO_FILE}"
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating gRPC sources for ${PROTO_NAME}"
        )

        list(APPEND PROTO_SRCS ${GENERATED_PROTO_SRC})
        list(APPEND PROTO_HDRS ${GENERATED_PROTO_HDR})
        list(APPEND GRPC_SRCS ${GENERATED_GRPC_SRC})
        list(APPEND GRPC_HDRS ${GENERATED_GRPC_HDR})
    endforeach()
else()
    message(STATUS "No .proto files found in ${CMAKE_SOURCE_DIR}/proto/")
endif()

# Source files (exclude main.cpp, CLI files, and RPC files temporarily)
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
# Exclude CLI files from core library
file(GLOB_RECURSE CLI_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/*.cpp")
list(REMOVE_ITEM SOURCES ${CLI_SOURCES})
# Use full RPC implementation; exclude stub
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/rpc/rpc_server_stub.cpp")

# Add governance and monetary policy sources
list(APPEND SOURCES
    "${CMAKE_SOURCE_DIR}/src/state/adaptive_inflation.cpp"
    "${CMAKE_SOURCE_DIR}/src/state/layered_governance.cpp"
    "${CMAKE_SOURCE_DIR}/src/state/staking_controls.cpp"
)

# Add paper token sources
list(APPEND SOURCES
    "${CMAKE_SOURCE_DIR}/src/state/paper_token.cpp"
    "${CMAKE_SOURCE_DIR}/src/state/paper_token_manager.cpp"
    "${CMAKE_SOURCE_DIR}/src/state/paper_token_validator.cpp"
    "${CMAKE_SOURCE_DIR}/src/state/paper_token_executor.cpp"
)

# Main library
add_library(sarafu_core STATIC
    ${SOURCES}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# Set up library dependencies
set(SARAFU_CORE_LIBS
    Threads::Threads
    Boost::thread
    Boost::filesystem
    blst
    blake3
)

# Add Boost::system only for older Boost versions
if(Boost_VERSION VERSION_LESS 1.73.0)
    list(APPEND SARAFU_CORE_LIBS Boost::system)
endif()

# Add Protobuf
if(USE_SYSTEM_PROTOBUF)
    list(APPEND SARAFU_CORE_LIBS protobuf::libprotobuf)
else()
    list(APPEND SARAFU_CORE_LIBS libprotobuf)
endif()

# Add gRPC
if(USE_SYSTEM_GRPC)
    list(APPEND SARAFU_CORE_LIBS gRPC::grpc++)
else()
    list(APPEND SARAFU_CORE_LIBS grpc++)
endif()

# Add RocksDB
if(USE_SYSTEM_ROCKSDB)
    list(APPEND SARAFU_CORE_LIBS RocksDB::rocksdb)
else()
    list(APPEND SARAFU_CORE_LIBS rocksdb)
endif()

# Add libsodium
if(USE_SYSTEM_LIBSODIUM)
    list(APPEND SARAFU_CORE_LIBS ${LIBSODIUM_LIBRARIES})
else()
    list(APPEND SARAFU_CORE_LIBS sodium)
endif()

# Add prometheus-cpp
list(APPEND SARAFU_CORE_LIBS prometheus-cpp::core prometheus-cpp::pull)

# Add nlohmann/json
list(APPEND SARAFU_CORE_LIBS nlohmann_json::nlohmann_json)

# Add cpp-httplib
list(APPEND SARAFU_CORE_LIBS httplib::httplib)

# Add OpenSSL for TLS (REST/WebSocket)
list(APPEND SARAFU_CORE_LIBS OpenSSL::SSL OpenSSL::Crypto)

target_link_libraries(sarafu_core
    PUBLIC
        ${SARAFU_CORE_LIBS}
)

if(USE_SYSTEM_LIBSODIUM)
    target_link_directories(sarafu_core
        PUBLIC
            ${LIBSODIUM_LIBRARY_DIRS}
    )
endif()

target_include_directories(sarafu_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/generated
        ${blake3_SOURCE_DIR}/c
        ${Boost_INCLUDE_DIRS}
)

if(USE_SYSTEM_LIBSODIUM)
    target_include_directories(sarafu_core
        PUBLIC
            ${LIBSODIUM_INCLUDE_DIRS}
    )
endif()

# Main executable (node)
add_executable(sarafu-node
    ${CMAKE_SOURCE_DIR}/src/main.cpp
)

target_link_libraries(sarafu-node
    PRIVATE
        sarafu_core
)

# CLI tool (commented out until keygen.cpp and genesis.cpp are implemented)
# add_executable(sar
#     ${CMAKE_SOURCE_DIR}/src/cli/main.cpp
#     ${CMAKE_SOURCE_DIR}/src/cli/keygen.cpp
#     ${CMAKE_SOURCE_DIR}/src/cli/genesis.cpp
# )
# 
# target_link_libraries(sar
#     PRIVATE
#         sarafu_core
# )
# 
# target_include_directories(sar
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
# )
# 
# target_compile_definitions(sar PRIVATE CLI_MODE=1)

# Fuzzing option
option(ENABLE_FUZZING "Enable fuzzing with libFuzzer" OFF)

if(ENABLE_FUZZING)
    message(STATUS "Fuzzing enabled with libFuzzer")
    
    # Check for Clang compiler (libFuzzer requires Clang)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Fuzzing requires Clang compiler. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
    
    # Check if this is Apple Clang (which doesn't include libFuzzer by default)
    if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        message(WARNING "Apple Clang detected. libFuzzer may not be available.")
        message(WARNING "To use fuzzing on macOS, install LLVM Clang:")
        message(WARNING "  brew install llvm")
        message(WARNING "  cmake -B build -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++")
        message(WARNING "Continuing with Apple Clang - build may fail if libFuzzer is not available.")
    endif()
    
    # Add fuzzing flags
    add_compile_options(-fsanitize=fuzzer,address,undefined)
    add_link_options(-fsanitize=fuzzer,address,undefined)
    
    message(STATUS "Fuzzing flags added: -fsanitize=fuzzer,address,undefined")
endif()

# Testing
enable_testing()
add_subdirectory(tests)

# Benchmarks
add_subdirectory(benchmarks)

# Examples (commented out until network_demo.cpp is implemented)
# add_executable(network_demo
#     ${CMAKE_SOURCE_DIR}/examples/network_demo.cpp
# )
# 
# target_link_libraries(network_demo
#     PRIVATE
#         sarafu_core
# )

# Installation
install(TARGETS sarafu-node DESTINATION bin)
# install(TARGETS sar DESTINATION bin)  # Commented out until CLI tool is implemented
install(DIRECTORY include/ DESTINATION include)
