#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace hyperion::debug::devtools {

// Log level for console output
enum class LogLevel : uint8_t {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
};

// Console message type
enum class ConsoleType : uint8_t {
    LOG,
    INFO_MSG,
    WARNING_MSG,
    ERROR_MSG,
    ASSERT,
    TABLE,
    TRACE,
    GROUP,
    GROUP_END,
    TIME,
    TIME_END,
    COUNT,
    CLEAR,
};

// Console message structure
struct ConsoleMessage {
    ConsoleType type;
    LogLevel level;
    std::string text;
    std::string group;
    std::string label;
    std::chrono::milliseconds timestamp;
    uint32_t source_id;
    std::vector<std::string> table_rows;
    bool apply_styles;
};

// JavaScript debug APIs
class JsDebugAPI {
public:
    // console.log variants
    static void log(const std::string& message) {
        default_handler_(nullptr, LogLevel::INFO, ConsoleType::LOG, message, {});
    }
    
    static void info(const std::string& message) {
        default_handler_(nullptr, LogLevel::INFO, ConsoleType::INFO_MSG, message, {});
    }
    
    static void warn(const std::string& message) {
        default_handler_(nullptr, LogLevel::WARNING, ConsoleType::WARNING_MSG, message, {});
    }
    
    static void error(const std::string& message) {
        default_handler_(nullptr, LogLevel::ERROR, ConsoleType::ERROR_MSG, message, {});
    }
    
    static void trace(const std::string& label = {}) {
        default_handler_(nullptr, LogLevel::DEBUG, ConsoleType::TRACE, {}, {label});
    }
    
    static void table(const std::vector<std::string>& data) {
        default_handler_(nullptr, LogLevel::INFO, ConsoleType::TABLE, {}, data);
    }
    
    static void group(const std::string& label) {
        default_handler_(nullptr, LogLevel::INFO, ConsoleType::GROUP, {}, {label});
    }
    
    static void group_collapsed(const std::string& label) {
        // Treat same as group for simplicity
        default_handler_(nullptr, LogLevel::INFO, ConsoleType::GROUP, {}, {label});
    }
    
    static void group_end() {
        default_handler_(nullptr, LogLevel::INFO, ConsoleType::GROUP_END, {}, {});
    }
    
    // Timing functions
    static void time(const std::string& label = "default") {
        auto now = std::chrono::steady_clock::now();
        start_times_[label] = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        );
    }
    
    static void time_log(const std::string& label = "default") {
        auto it = start_times_.find(label);
        if (it != start_times_.end()) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ) - it->second;
            
            std::ostringstream oss;
            oss << label << ": " << ms.count() << "ms";
            default_handler_(nullptr, LogLevel::INFO, ConsoleType::TIME_END, oss.str(), {});
            start_times_.erase(it);
        }
    }
    
    // Assertion helper
    template<typename... Args>
    static void assert(bool condition, const std::string& message, Args... args) {
        if (!condition) {
            std::ostringstream oss;
            oss << "Assertion failed: " << message;
            error(oss.str());
        }
    }
    
    // Custom handler setup
    using MessageHandler = std::function<void(const void*, LogLevel, ConsoleType,
                                          const std::string&, const std::vector<std::string>&)>;
    
    static void set_global_handler(MessageHandler handler) {
        default_handler_ = handler;
    }

private:
    static MessageHandler default_handler_;
    static std::unordered_map<std::string, std::chrono::milliseconds> start_times_;
};

// Default no-op handler
MessageHandler JsDebugAPI::default_handler_ = [](const void* /*ctx*/, LogLevel level, 
                                               ConsoleType type, const std::string& text,
                                               const std::vector<std::string>& extras) {
    // Prevent infinite recursion
};
std::unordered_map<std::string, std::chrono::milliseconds> JsDebugAPI::start_times_;

// Performance timeline for trace collection
class PerformanceTimeline {
public:
    struct TimelineEntry {
        std::string name;
        std::string entry_type;
        uint64_t start_time;
        uint64_t duration;
        std::unordered_map<std::string, std::string> attributes;
    };
    
    void mark(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        TimelineEntry entry;
        entry.name = name;
        entry.entry_type = "mark";
        entry.start_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()
        ).count());
        entries_.push_back(entry);
    }
    
    void measure(const std::string& name, const std::string& start_marker, 
               const std::string& end_marker = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        TimelineEntry entry;
        entry.name = name;
        entry.entry_type = "measure";
        entry.start_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()
        ).count());
        
        // Find matching start marker
        uint64_t start = 0;
        for (const auto& e : entries_) {
            if (e.name == start_marker && e.entry_type == "mark") {
                start = e.start_time;
                break;
            }
        }
        
        entry.duration = entry.start_time - start;
        entry.attributes["startTime"] = std::to_string(start);
        entry.attributes["duration"] = std::to_string(entry.duration);
        
        entries_.push_back(entry);
    }
    
    std::vector<TimelineEntry> get_entries() {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }
    
    // Resource timing support
    void add_resource_timing(const std::string& request_id, uint64_t fetch_start,
                            uint64_t response_end, const std::string& initiator_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        TimelineEntry entry;
        entry.name = request_id;
        entry.entry_type = "resource";
        entry.start_time = fetch_start;
        entry.duration = response_end - fetch_start;
        entry.attributes["initiatorType"] = initiator_type;
        entries_.push_back(entry);
    }

private:
    std::vector<TimelineEntry> entries_;
    mutable std::mutex mutex_;
};

// Memory profiler for heap analysis
class MemoryProfiler {
public:
    struct HeapSnapshot {
        std::string name;
        std::unordered_map<std::string, uint64_t> counts;
        std::unordered_map<std::string, uint64_t> sizes;
        std::chrono::system_clock::time_point timestamp;
    };
    
    void take_snapshot(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        HeapSnapshot snapshot;
        snapshot.name = name;
        snapshot.timestamp = std::chrono::system_clock::now();
        
        // Simulate counting memory types
        snapshot.counts["String"] = 1024;
        snapshot.counts["Array"] = 512;
        snapshot.counts["Object"] = 256;
        
        snapshot.sizes["String"] = 65536;
        snapshot.sizes["Array"] = 131072;
        snapshot.sizes["Object"] = 98304;
        
        snapshots_[name] = snapshot;
    }
    
    void get_snapshot(const std::string& name, HeapSnapshot& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = snapshots_.find(name);
        if (it != snapshots_.end()) {
            out = it->second;
        }
    }
    
    std::vector<HeapSnapshot> get_all_snapshots() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<HeapSnapshot> result;
        for (const auto& [name, snapshot] : snapshots_) {
            result.push_back(snapshot);
        }
        return result;
    }

private:
    std::unordered_map<std::string, HeapSnapshot> snapshots_;
    mutable std::mutex mutex_;
};

// Debug bundler for combining features
class DevtoolsBundler {
public:
    static void initialize_defaults() {
        JsDebugAPI::set_global_handler(default_handler);
    }
    
    static void register_console_command(const std::string& command,
                                     std::function<void(const std::vector<std::string>&)> action) {
        console_commands_[command] = action;
    }
    
    static bool execute_console_command(const std::string& input) {
        std::istringstream iss(input);
        std::string command;
        iss >> command;
        
        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }
        
        if (console_commands_.find(command) != console_commands_.end()) {
            console_commands_[command](args);
            return true;
        }
        return false;
    }

private:
    static const MessageHandler default_handler;
    static std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> console_commands_;
};

const MessageHandler DevtoolsBundler::default_handler = [](const void* /*ctx*/, LogLevel level,
                                                        ConsoleType type, 
                                                        const std::string& text,
                                                        const std::vector<std::string>& extras) {
    const char* prefix = []{
        switch (level) {
            case LogLevel::DEBUG: return "%c[36mDEBUG%c[0m: ";
            case LogLevel::INFO: return "%c[32mINFO%c[0m: ";
            case LogLevel::WARNING: return "%c[33mWARNING%c[0m: ";
            case LogLevel::ERROR: return "%c[31mERROR%c[0m: ";
            case LogLevel::CRITICAL: return "%c[1;31mCRITICAL%c[0m: ";
            default: return "";
        }
    }();
    
    // Console standard output simulation
    printf(prefix, 0x1B, 0x1B);
    printf("%s\n", text.c_str());
    if (!extras.empty()) {
        for (const auto& line : extras) {
            printf("  %s\n", line.c_str());
        }
    }
};

std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>>
DevtoolsBundler::console_commands_;

} // namespace hyperion::debug::devtools