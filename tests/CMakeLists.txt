cmake_minimum_required(VERSION 3.20)

# Find Google Test (now using FetchContent)
# find_package(GTest REQUIRED)
include(GoogleTest)

# Allow slower test binaries to list tests without failing the build.
set(GTEST_DISCOVERY_TIMEOUT 60)

# Test utilities library
add_library(test_utils STATIC
    test_utils.cpp
)

target_link_libraries(test_utils
    PUBLIC
        sarafu_core
        GTest::gtest
)

target_include_directories(test_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Unit tests
file(GLOB_RECURSE UNIT_TEST_SOURCES "unit/*.cpp")
if(UNIT_TEST_SOURCES)
    add_executable(unit_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(unit_tests
        PRIVATE
            sarafu_core
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    gtest_discover_tests(unit_tests DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endif()

# Property-based tests
file(GLOB_RECURSE PROPERTY_TEST_SOURCES "property/*.cpp")
if(PROPERTY_TEST_SOURCES)
    add_executable(property_tests ${PROPERTY_TEST_SOURCES})
    target_link_libraries(property_tests
        PRIVATE
            sarafu_core
            test_utils
            GTest::gtest
            GTest::gtest_main
            rapidcheck
            rapidcheck_gtest
    )
    # Configure RapidCheck with 1000 iterations per test
    target_compile_definitions(property_tests PRIVATE RC_PARAMS="max_success=1000")
    gtest_discover_tests(property_tests DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endif()

# Integration tests
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "integration/*.cpp")
if(INTEGRATION_TEST_SOURCES)
    add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(integration_tests
        PRIVATE
            sarafu_core
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    gtest_discover_tests(integration_tests DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endif()

# Stress tests
file(GLOB_RECURSE STRESS_TEST_SOURCES "stress/*.cpp")
if(STRESS_TEST_SOURCES)
    add_executable(stress_tests ${STRESS_TEST_SOURCES})
    target_link_libraries(stress_tests
        PRIVATE
            sarafu_core
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    gtest_discover_tests(stress_tests DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endif()

# Consensus tests
file(GLOB_RECURSE CONSENSUS_TEST_SOURCES "consensus/*.cpp")
if(CONSENSUS_TEST_SOURCES)
    add_executable(consensus_tests ${CONSENSUS_TEST_SOURCES})
    target_link_libraries(consensus_tests
        PRIVATE
            sarafu_core
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    gtest_discover_tests(consensus_tests DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endif()

# State tests (governance and monetary policy)
file(GLOB_RECURSE STATE_TEST_SOURCES "state/*.cpp")
if(STATE_TEST_SOURCES)
    add_executable(state_tests ${STATE_TEST_SOURCES})
    target_link_libraries(state_tests
        PRIVATE
            sarafu_core
            test_utils
            GTest::gtest
            GTest::gtest_main
    )
    gtest_discover_tests(state_tests DISCOVERY_TIMEOUT ${GTEST_DISCOVERY_TIMEOUT})
endif()

# Fuzz tests (only build if ENABLE_FUZZING is ON)
if(ENABLE_FUZZING)
    message(STATUS "Building fuzz tests")
    
    # Find all fuzz test sources
    file(GLOB FUZZ_TEST_SOURCES "fuzz/*_fuzzer.cpp")
    
    # Create a separate executable for each fuzzer
    foreach(FUZZ_SOURCE ${FUZZ_TEST_SOURCES})
        get_filename_component(FUZZ_NAME ${FUZZ_SOURCE} NAME_WE)
        
        add_executable(${FUZZ_NAME} ${FUZZ_SOURCE})
        
        target_link_libraries(${FUZZ_NAME}
            PRIVATE
                sarafu_core
        )
        
        # Add to test suite
        add_test(NAME ${FUZZ_NAME}_smoke
                 COMMAND ${FUZZ_NAME} -runs=1000 -max_total_time=10)
        
        message(STATUS "Added fuzzer: ${FUZZ_NAME}")
    endforeach()
endif()
