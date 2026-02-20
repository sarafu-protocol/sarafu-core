cmake_minimum_required(VERSION 3.20)
project(sarafu-blockchain VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread filesystem)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)
# find_package(Protobuf REQUIRED)
# find_package(gRPC CONFIG REQUIRED)
# find_package(RocksDB REQUIRED)

# Include FetchContent for dependencies
include(FetchContent)

# libp2p (using cpp-libp2p) - commented out for now
# FetchContent_Declare(
#     libp2p
#     GIT_REPOSITORY https://github.com/libp2p/cpp-libp2p.git
#     GIT_TAG v0.1.0
# )

# libsodium for Ed25519 - using system package via pkg-config
# FetchContent_Declare(
#     libsodium
#     GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
#     GIT_TAG 1.0.19-RELEASE
# )

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
)

# Make dependencies available
FetchContent_MakeAvailable(blake3 googletest)

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

# Generate Protocol Buffer files (commented out for now)
# file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")
# protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
# grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${CMAKE_BINARY_DIR}/generated ${PROTO_FILES})

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
    # ${PROTO_SRCS}
    # ${GRPC_SRCS}
)

target_link_libraries(sarafu_core
    PUBLIC
        Threads::Threads
        Boost::system
        Boost::thread
        # protobuf::libprotobuf
        # gRPC::grpc++
        # RocksDB::rocksdb
        ${LIBSODIUM_LIBRARIES}
        blst
        blake3
)

target_link_directories(sarafu_core
    PUBLIC
        ${LIBSODIUM_LIBRARY_DIRS}
)

target_include_directories(sarafu_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/generated
        ${blake3_SOURCE_DIR}/c
        ${LIBSODIUM_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

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
