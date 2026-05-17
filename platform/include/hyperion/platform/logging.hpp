#pragma once

#include <string>
#include <string_view>
#include <format>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <source_location>

namespace hyperion::platform {

enum class log_level {
    debug,
    info,
    warning,
    error,
    critical
};

class logger {
public:
    static logger& instance();

    template<typename... Args>
    void log(log_level level, std::string_view fmt, Args&&... args) {
        std::lock_guard lock(m_mutex);
        
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", now);
        
        std::string message = std::vformat(fmt, std::make_format_args(args...));
        std::string log_entry = std::format("[{}] [{}] {}\n", 
            timestamp, 
            to_string(level), 
            message);
            
        std::cout << log_entry;
        
        // Write to file
        if (m_log_file.is_open()) {
            m_log_file << log_entry;
            m_log_file.flush();
        }
    }

private:
    logger();
    ~logger();

    std::string_view to_string(log_level level) {
        switch (level) {
            case log_level::debug:    return "DEBUG";
            case log_level::info:     return "INFO";
            case log_level::warning:  return "WARN";
            case log_level::error:    return "ERROR";
            case log_level::critical: return "CRIT";
            default:                  return "UNKNOWN";
        }
    }

    std::mutex m_mutex;
    std::ofstream m_log_file;
};

} // namespace hyperion::platform

#define HYPERION_LOG_INFO(fmt, ...) \
    ::hyperion::platform::logger::instance().log(::hyperion::platform::log_level::info, fmt __VA_OPT__(,) __VA_ARGS__)

#define HYPERION_LOG_WARN(fmt, ...) \
    ::hyperion::platform::logger::instance().log(::hyperion::platform::log_level::warning, fmt __VA_OPT__(,) __VA_ARGS__)

#define HYPERION_LOG_ERROR(fmt, ...) \
    ::hyperion::platform::logger::instance().log(::hyperion::platform::log_level::error, fmt __VA_OPT__(,) __VA_ARGS__)

#define HYPERION_LOG_DEBUG(fmt, ...) \
    ::hyperion::platform::logger::instance().log(::hyperion::platform::log_level::debug, fmt __VA_OPT__(,) __VA_ARGS__)
