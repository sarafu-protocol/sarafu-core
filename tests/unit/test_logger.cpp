#include <gtest/gtest.h>
#include "sarafu/logging/logger.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>

using namespace sarafu::logging;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file_ = "test_log.txt";
        // Clean up any existing test log file
        if (std::filesystem::exists(test_log_file_)) {
            std::filesystem::remove(test_log_file_);
        }
    }

    void TearDown() override {
        // Clean up test log file
        if (std::filesystem::exists(test_log_file_)) {
            std::filesystem::remove(test_log_file_);
        }
    }

    std::string test_log_file_;
};

TEST_F(LoggerTest, DefaultLogLevel) {
    Logger logger;
    EXPECT_EQ(logger.get_level(), LogLevel::INFO);
}

TEST_F(LoggerTest, SetLogLevel) {
    Logger logger;
    logger.set_level(LogLevel::DEBUG);
    EXPECT_EQ(logger.get_level(), LogLevel::DEBUG);
    
    logger.set_level(LogLevel::ERROR);
    EXPECT_EQ(logger.get_level(), LogLevel::ERROR);
}

TEST_F(LoggerTest, LogToStdout) {
    Logger logger;
    logger.set_level(LogLevel::DEBUG);
    
    // These should not throw
    EXPECT_NO_THROW(logger.debug("TestComponent", "Debug message"));
    EXPECT_NO_THROW(logger.info("TestComponent", "Info message"));
    EXPECT_NO_THROW(logger.warn("TestComponent", "Warning message"));
    EXPECT_NO_THROW(logger.error("TestComponent", "Error message"));
}

TEST_F(LoggerTest, LogToFile) {
    {
        Logger logger(test_log_file_);
        logger.set_level(LogLevel::INFO);
        logger.info("TestComponent", "Test message");
    }
    
    // Check that file was created and contains the message
    ASSERT_TRUE(std::filesystem::exists(test_log_file_));
    
    std::ifstream file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("Test message") != std::string::npos);
    EXPECT_TRUE(content.find("[INFO ]") != std::string::npos);
    EXPECT_TRUE(content.find("[TestComponent]") != std::string::npos);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    {
        Logger logger(test_log_file_);
        logger.set_level(LogLevel::WARN);
        
        logger.debug("TestComponent", "Debug message");
        logger.info("TestComponent", "Info message");
        logger.warn("TestComponent", "Warning message");
        logger.error("TestComponent", "Error message");
    }
    
    std::ifstream file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Debug and Info should be filtered out
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);
    
    // Warn and Error should be present
    EXPECT_TRUE(content.find("Warning message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message") != std::string::npos);
}

TEST_F(LoggerTest, ThreadSafety) {
    Logger logger;
    logger.set_level(LogLevel::INFO);
    
    // Create multiple threads logging simultaneously
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&logger, i]() {
            for (int j = 0; j < 100; j++) {
                logger.info("Thread" + std::to_string(i), 
                           "Message " + std::to_string(j));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we get here without crashing, thread safety is working
    SUCCEED();
}

TEST_F(LoggerTest, SingletonInstance) {
    Logger& logger1 = Logger::instance();
    Logger& logger2 = Logger::instance();
    
    // Should be the same instance
    EXPECT_EQ(&logger1, &logger2);
}
