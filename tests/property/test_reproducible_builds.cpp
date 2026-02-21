#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <array>
#include <regex>

namespace fs = std::filesystem;

/**
 * Property-Based Test for Reproducible Builds
 * 
 * **Validates: Requirements 1.6, 24.6**
 * 
 * Property 1: Reproducible Builds
 * For any source tree and build configuration, building the same source twice
 * with the same toolchain SHALL produce byte-identical binaries.
 * 
 * This test validates that:
 * 1. Reproducible build configuration is properly set up
 * 2. CMake flags for reproducibility are correctly configured
 * 3. Compiler flags remove non-deterministic elements
 * 4. Build system enforces deterministic timestamps
 * 
 * Note: Full binary comparison requires a complete build, which is tested
 * separately in CI/CD pipelines. This test validates the configuration.
 */
class ReproducibleBuildsPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        
        // Get project root directory
        project_root_ = fs::current_path();
        while (!fs::exists(project_root_ / "CMakeLists.txt") && project_root_.has_parent_path()) {
            project_root_ = project_root_.parent_path();
        }
    }

    // Read file contents
    std::string read_file(const fs::path& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + file_path.string());
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Check if a string contains a pattern
    bool contains_pattern(const std::string& content, const std::string& pattern) {
        return content.find(pattern) != std::string::npos;
    }

    // Check if a regex pattern matches in the content
    bool matches_regex(const std::string& content, const std::string& pattern) {
        std::regex regex_pattern(pattern);
        return std::regex_search(content, regex_pattern);
    }

    std::mt19937 rng_;
    fs::path project_root_;
};

/**
 * Property: CMakeLists.txt has reproducible build option configured
 * 
 * The build system must have a REPRODUCIBLE_BUILD option that enables
 * deterministic builds.
 */
TEST_F(ReproducibleBuildsPropertyTest, CMakeHasReproducibleBuildOption) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    ASSERT_TRUE(fs::exists(cmake_file))
        << "CMakeLists.txt not found at " << cmake_file;
    
    std::string content = read_file(cmake_file);
    
    // Check for REPRODUCIBLE_BUILD option
    EXPECT_TRUE(contains_pattern(content, "REPRODUCIBLE_BUILD"))
        << "CMakeLists.txt does not define REPRODUCIBLE_BUILD option";
    
    // Check for option declaration
    EXPECT_TRUE(matches_regex(content, R"(option\s*\(\s*REPRODUCIBLE_BUILD)"))
        << "REPRODUCIBLE_BUILD is not properly declared as an option";
}

/**
 * Property: SOURCE_DATE_EPOCH is configured for reproducible builds
 * 
 * When REPRODUCIBLE_BUILD is enabled, SOURCE_DATE_EPOCH must be set
 * to ensure deterministic timestamps.
 */
TEST_F(ReproducibleBuildsPropertyTest, SourceDateEpochIsConfigured) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for SOURCE_DATE_EPOCH handling
    EXPECT_TRUE(contains_pattern(content, "SOURCE_DATE_EPOCH"))
        << "CMakeLists.txt does not handle SOURCE_DATE_EPOCH";
    
    // Check that it's set when REPRODUCIBLE_BUILD is ON
    EXPECT_TRUE(matches_regex(content, R"(if\s*\(\s*REPRODUCIBLE_BUILD\s*\))"))
        << "REPRODUCIBLE_BUILD conditional not found";
}

/**
 * Property: Compiler flags remove non-deterministic elements
 * 
 * For reproducible builds, compiler flags must remove timestamps,
 * absolute paths, and other non-deterministic elements.
 */
TEST_F(ReproducibleBuildsPropertyTest, CompilerFlagsRemoveNonDeterministicElements) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for -Wdate-time flag (warns about __DATE__ and __TIME__ macros)
    EXPECT_TRUE(contains_pattern(content, "-Wdate-time"))
        << "Missing -Wdate-time flag to warn about timestamp macros";
    
    // Check for -ffile-prefix-map flag (removes absolute paths)
    EXPECT_TRUE(contains_pattern(content, "-ffile-prefix-map") || 
                contains_pattern(content, "ffile-prefix-map"))
        << "Missing -ffile-prefix-map flag to remove absolute paths";
    
    // Check for -fmacro-prefix-map flag (removes absolute paths from macros)
    EXPECT_TRUE(contains_pattern(content, "-fmacro-prefix-map") ||
                contains_pattern(content, "fmacro-prefix-map"))
        << "Missing -fmacro-prefix-map flag to remove absolute paths from macros";
}

/**
 * Property: Linker flags disable non-deterministic build IDs
 * 
 * Build IDs can vary between builds, so they must be disabled
 * for reproducible builds.
 */
TEST_F(ReproducibleBuildsPropertyTest, LinkerFlagsDisableBuildID) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for --build-id=none flag
    EXPECT_TRUE(contains_pattern(content, "--build-id=none") ||
                contains_pattern(content, "build-id=none"))
        << "Missing --build-id=none linker flag to disable build IDs";
}

/**
 * Property: Reproducible build flags are applied conditionally
 * 
 * The reproducible build flags should only be applied when
 * REPRODUCIBLE_BUILD option is enabled.
 */
TEST_F(ReproducibleBuildsPropertyTest, ReproducibleFlagsAreConditional) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Find the REPRODUCIBLE_BUILD conditional block
    size_t if_pos = content.find("if(REPRODUCIBLE_BUILD)");
    ASSERT_NE(if_pos, std::string::npos)
        << "REPRODUCIBLE_BUILD conditional block not found";
    
    // Find the corresponding endif
    size_t endif_pos = content.find("endif()", if_pos);
    ASSERT_NE(endif_pos, std::string::npos)
        << "REPRODUCIBLE_BUILD conditional block not properly closed";
    
    // Extract the conditional block
    std::string conditional_block = content.substr(if_pos, endif_pos - if_pos);
    
    // Verify that compiler flags are within the conditional
    EXPECT_TRUE(contains_pattern(conditional_block, "add_compile_options") ||
                contains_pattern(conditional_block, "CMAKE_CXX_FLAGS"))
        << "Compiler flags not set within REPRODUCIBLE_BUILD conditional";
}

/**
 * Property: Git timestamp is used as fallback for SOURCE_DATE_EPOCH
 * 
 * If SOURCE_DATE_EPOCH is not set, the build system should use
 * the git commit timestamp as a fallback.
 */
TEST_F(ReproducibleBuildsPropertyTest, GitTimestampFallbackExists) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for git log command to get commit timestamp
    EXPECT_TRUE(contains_pattern(content, "git log") &&
                contains_pattern(content, "%ct"))
        << "Missing git log command to get commit timestamp as fallback";
}

/**
 * Property: Fixed timestamp fallback exists for non-git builds
 * 
 * If not in a git repository, a fixed timestamp should be used
 * as a final fallback.
 */
TEST_F(ReproducibleBuildsPropertyTest, FixedTimestampFallbackExists) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for a fixed timestamp fallback (should be a Unix timestamp)
    EXPECT_TRUE(matches_regex(content, R"(\d{10})"))
        << "Missing fixed timestamp fallback for non-git builds";
}

/**
 * Property: Reproducible build configuration is documented
 * 
 * The build system should log when reproducible builds are enabled
 * and what SOURCE_DATE_EPOCH value is being used.
 */
TEST_F(ReproducibleBuildsPropertyTest, ReproducibleBuildIsLogged) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for message() call that logs reproducible build status
    EXPECT_TRUE(contains_pattern(content, "message") &&
                contains_pattern(content, "Reproducible build"))
        << "Missing log message for reproducible build status";
    
    // Check that SOURCE_DATE_EPOCH is included in the message
    EXPECT_TRUE(matches_regex(content, R"(message\s*\([^)]*SOURCE_DATE_EPOCH)"))
        << "Log message does not include SOURCE_DATE_EPOCH value";
}

/**
 * Property: Compiler version independence
 * 
 * The reproducible build configuration should work with both
 * GCC and Clang compilers.
 */
TEST_F(ReproducibleBuildsPropertyTest, CompilerVersionIndependence) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check for compiler ID check (GNU or Clang)
    EXPECT_TRUE(contains_pattern(content, "CMAKE_CXX_COMPILER_ID") &&
                (contains_pattern(content, "GNU") || contains_pattern(content, "Clang")))
        << "Missing compiler ID check for GCC/Clang compatibility";
}

/**
 * Property: Reproducible builds work with different build types
 * 
 * The reproducible build flags should be compatible with both
 * Debug and Release build types.
 */
TEST_F(ReproducibleBuildsPropertyTest, CompatibleWithBuildTypes) {
    fs::path cmake_file = project_root_ / "CMakeLists.txt";
    std::string content = read_file(cmake_file);
    
    // Check that reproducible build flags are added via add_compile_options
    // or CMAKE_CXX_FLAGS (which apply to all build types)
    EXPECT_TRUE(contains_pattern(content, "add_compile_options") ||
                contains_pattern(content, "CMAKE_CXX_FLAGS"))
        << "Reproducible build flags not applied in a build-type-independent way";
    
    // Verify that flags are not only added to specific build types
    size_t if_reproducible = content.find("if(REPRODUCIBLE_BUILD)");
    if (if_reproducible != std::string::npos) {
        size_t endif_pos = content.find("endif()", if_reproducible);
        std::string block = content.substr(if_reproducible, endif_pos - if_reproducible);
        
        // Should not have CMAKE_CXX_FLAGS_DEBUG or CMAKE_CXX_FLAGS_RELEASE only
        EXPECT_FALSE(contains_pattern(block, "CMAKE_CXX_FLAGS_DEBUG") &&
                     !contains_pattern(block, "CMAKE_CXX_FLAGS_RELEASE"))
            << "Reproducible build flags only applied to Debug build";
        
        EXPECT_FALSE(contains_pattern(block, "CMAKE_CXX_FLAGS_RELEASE") &&
                     !contains_pattern(block, "CMAKE_CXX_FLAGS_DEBUG"))
            << "Reproducible build flags only applied to Release build";
    }
}
