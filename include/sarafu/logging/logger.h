#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace sarafu {
namespace logging {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    Logger();
    explicit Logger(const std::string& log_file_path);
    ~Logger();

    // Set minimum log level
    void set_level(LogLevel level);
    LogLevel get_level() const;

    // Logging methods
    void debug(const std::string& component, const std::string& message);
    void info(const std::string& component, const std::string& message);
    void warn(const std::string& component, const std::string& message);
    void error(const std::string& component, const std::string& message);

    // Generic log method
    void log(LogLevel level, const std::string& component, const std::string& message);

    // Get singleton instance
    static Logger& instance();

private:
    void write_log(LogLevel level, const std::string& component, const std::string& message);
    std::string format_timestamp() const;
    std::string level_to_string(LogLevel level) const;

    LogLevel min_level_;
    std::ofstream log_file_;
    std::mutex mutex_;
    bool use_file_;
};

// Convenience macros for logging
#define LOG_DEBUG(component, message) \
    sarafu::logging::Logger::instance().debug(component, message)

#define LOG_INFO(component, message) \
    sarafu::logging::Logger::instance().info(component, message)

#define LOG_WARN(component, message) \
    sarafu::logging::Logger::instance().warn(component, message)

#define LOG_ERROR(component, message) \
    sarafu::logging::Logger::instance().error(component, message)

} // namespace logging
} // namespace sarafu
