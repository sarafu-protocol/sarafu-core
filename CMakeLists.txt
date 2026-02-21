cmake_minimum_required(VERSION 3.20)
project(sarafu-blockchain VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reproducible build options
option(REPRODUCIBLE_BUILD "Enable reproducible builds with deterministic output" OFF)

if(REPRODUCIBLE_BUILD)
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
find_package(Boost 1.70 REQUIRED COMPONENTS system thread filesystem)

# Option to use system packages or fetch content
option(USE_SYSTEM_PROTOBUF "Use system-installed Protobuf" OFF)
option(USE_SYSTEM_GRPC "Use system-installed gRPC" OFF)
option(USE_SYSTEM_ROCKSDB "Use system-installed RocksDB" OFF)
option(USE_SYSTEM_LIBSODIUM "Use system-installed libsodium" ON)

# Protobuf
if(USE_SYSTEM_PROTOBUF)
    find_package(Protobuf 3.21.0 REQUIRED)
else()
    message(STATUS "Using FetchContent for Protobuf")
endif()

# gRPC
if(USE_SYSTEM_GRPC)
    find_package(gRPC 1.50.0 CONFIG REQUIRED)
else()
    message(STATUS "Using FetchContent for gRPC")
endif()

# RocksDB
if(USE_SYSTEM_ROCKSDB)
    find_package(RocksDB 7.0.0 REQUIRED)
else()
    message(STATUS "Using FetchContent for RocksDB")
endif()

# libsodium
if(USE_SYSTEM_LIBSODIUM)
    pkg_check_modules(LIBSODIUM REQUIRED libsodium>=1.0.18)
else()
    message(STATUS "Using FetchContent for libsodium")
endif()

# Include FetchContent for dependencies
include(FetchContent)

# Protobuf (if not using system package)
if(NOT USE_SYSTEM_PROTOBUF)
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG v21.12
        GIT_SHALLOW TRUE
    )
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
endif()

# gRPC (if not using system package)
if(NOT USE_SYSTEM_GRPC)
    FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG v1.50.1
        GIT_SHALLOW TRUE
    )
    set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_CODEGEN ON CACHE BOOL "" FORCE)
endif()

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

# Make dependencies available
set(FETCHCONTENT_QUIET OFF)
if(NOT USE_SYSTEM_PROTOBUF)
    FetchContent_MakeAvailable(protobuf)
endif()
if(NOT USE_SYSTEM_GRPC)
    FetchContent_MakeAvailable(grpc)
endif()
if(NOT USE_SYSTEM_ROCKSDB)
    FetchContent_MakeAvailable(rocksdb)
endif()
if(NOT USE_SYSTEM_LIBSODIUM)
    FetchContent_MakeAvailable(libsodium)
endif()

FetchContent_MakeAvailable(blake3 googletest)

# Note: libp2p requires special handling and will be integrated separately

# Build blst manually since it uses a custom build system
FetchContent_GetProperties(blst)
if(NOT blst_POPULATED)
    FetchContent_Populate(blst)
    
    # Build blst as a static library
    set(BLST_BUILD_DIR ${CMAKE_BINARY_DIR}/blst-build)
    file(MAKE_DIRECTORY ${BLST_BUILD_DIR})
    
    # Build blst using its build script
    execute_process(
        COMMAND ${blst_SOURCE_DIR}/build.sh
        WORKING_DIRECTORY ${blst_SOURCE_DIR}
        RESULT_VARIABLE BLST_BUILD_RESULT
    )
    
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

# Generate Protocol Buffer files
if(USE_SYSTEM_PROTOBUF OR NOT USE_SYSTEM_PROTOBUF)
    file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")
    if(PROTO_FILES)
        if(USE_SYSTEM_PROTOBUF)
            protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
        else()
            # Use FetchContent protobuf
            set(PROTO_SRCS "")
            set(PROTO_HDRS "")
            foreach(PROTO_FILE ${PROTO_FILES})
                get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
                list(APPEND PROTO_SRCS "${CMAKE_BINARY_DIR}/generated/${PROTO_NAME}.pb.cc")
                list(APPEND PROTO_HDRS "${CMAKE_BINARY_DIR}/generated/${PROTO_NAME}.pb.h")
            endforeach()
        endif()
        
        # gRPC generation
        if(USE_SYSTEM_GRPC)
            # grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${CMAKE_BINARY_DIR}/generated ${PROTO_FILES})
        else()
            set(GRPC_SRCS "")
            set(GRPC_HDRS "")
        endif()
    else()
        message(STATUS "No .proto files found in ${CMAKE_SOURCE_DIR}/proto/")
        set(PROTO_SRCS "")
        set(PROTO_HDRS "")
        set(GRPC_SRCS "")
        set(GRPC_HDRS "")
    endif()
endif()

# Source files (exclude main.cpp, CLI files, and RPC files temporarily)
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
# Exclude CLI files from core library
file(GLOB_RECURSE CLI_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/*.cpp")
list(REMOVE_ITEM SOURCES ${CLI_SOURCES})
# Temporarily exclude RPC files until gRPC is properly integrated (except stub)
file(GLOB_RECURSE RPC_SOURCES "${CMAKE_SOURCE_DIR}/src/rpc/*.cpp")
list(REMOVE_ITEM RPC_SOURCES "${CMAKE_SOURCE_DIR}/src/rpc/rpc_server_stub.cpp")
list(REMOVE_ITEM SOURCES ${RPC_SOURCES})

# Main library
add_library(sarafu_core STATIC
    ${SOURCES}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# Set up library dependencies
set(SARAFU_CORE_LIBS
    Threads::Threads
    Boost::system
    Boost::thread
    blst
    blake3
)

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

# CLI tool
add_executable(sar
    ${CMAKE_SOURCE_DIR}/src/cli/main.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/keygen.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/genesis.cpp
)

target_link_libraries(sar
    PRIVATE
        sarafu_core
)

target_include_directories(sar
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(sar PRIVATE CLI_MODE=1)

# Testing
enable_testing()
add_subdirectory(tests)

# Examples
add_executable(network_demo
    ${CMAKE_SOURCE_DIR}/examples/network_demo.cpp
)

target_link_libraries(network_demo
    PRIVATE
        sarafu_core
)

# Installation
install(TARGETS sarafu-node sar DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
