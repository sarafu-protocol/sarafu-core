#include "sarafu/logging/logger.h"
#include <iostream>

namespace sarafu {
namespace logging {

Logger::Logger() 
    : min_level_(LogLevel::INFO), use_file_(false) {
}

Logger::Logger(const std::string& log_file_path) 
    : min_level_(LogLevel::INFO), use_file_(true) {
    log_file_.open(log_file_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path << std::endl;
        use_file_ = false;
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

LogLevel Logger::get_level() const {
    return min_level_;
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::DEBUG, component, message);
}

void Logger::info(const std::string& component, const std::string& message) {
    log(LogLevel::INFO, component, message);
}

void Logger::warn(const std::string& component, const std::string& message) {
    log(LogLevel::WARN, component, message);
}

void Logger::error(const std::string& component, const std::string& message) {
    log(LogLevel::ERROR, component, message);
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    if (level < min_level_) {
        return;
    }
    write_log(level, component, message);
}

void Logger::write_log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = format_timestamp();
    std::string level_str = level_to_string(level);
    
    std::ostringstream log_entry;
    log_entry << timestamp << " [" << level_str << "] [" << component << "] " << message;
    
    // Always write to stdout
    std::cout << log_entry.str() << std::endl;
    
    // Write to file if enabled
    if (use_file_ && log_file_.is_open()) {
        log_file_ << log_entry.str() << std::endl;
        log_file_.flush();
    }
}

std::string Logger::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

} // namespace logging
} // namespace sarafu
