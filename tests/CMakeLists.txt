cmake_minimum_required(VERSION 3.20)

# Find Google Test (now using FetchContent)
# find_package(GTest REQUIRED)
include(GoogleTest)

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
    gtest_discover_tests(unit_tests)
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
    )
    gtest_discover_tests(property_tests)
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
    gtest_discover_tests(integration_tests)
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
    gtest_discover_tests(stress_tests)
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
    gtest_discover_tests(consensus_tests)
endif()
