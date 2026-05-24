#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <mutex>
#include <algorithm>

namespace hyperion::debug::performance {

// Performance entry types (Web API compatible)
enum class PerformanceEntryType : uint8_t {
    MARK,
    MEASURE,
    NAVIGATION,
    RESOURCE,
    PAINT,
    FRAME,
    LAYOUT,
    JS,
    FUNCTION,
    EVENT,
};

struct PerformanceEntry {
    std::string name;
    PerformanceEntryType type;
    uint64_t startTime;
    std::chrono::steady_clock::time_point timestamp;
    uint32_t duration;
    std::unordered_map<std::string, std::string> detail;
    std::string initiatorType;
};

// Performance resource timing (subset)
struct ResourceTimingEntry {
    std::string name;
    std::string initiatorType;
    uint64_t fetchStart;
    uint64_t domainLookupStart;
    uint64_t domainLookupEnd;
    uint64_t connectStart;
    uint64_t connectEnd;
    uint64_t secureConnectionStart;
    uint64_t requestStart;
    uint64_t responseStart;
    uint64_t responseEnd;
    uint64_t duration;
    std::string encodedBodySize;
    std::string decodedBodySize;
    std::string encodedTransferSize;
};

// Performance observer callback interface
class PerformanceObserver {
public:
    using Callback = std::function<void(const PerformanceEntry&)>;
    
    virtual void notify(const PerformanceEntry& entry) = 0;
    virtual ~PerformanceObserver() = default;
};

// Web Performance API compatible class
class JSPerformance {
public:
    // Mark entries
    void mark(const std::string& markName) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto entry = create_entry(markName, PerformanceEntryType::MARK, 0);
        marks_[markName] = entry;
        notify_observers(entry);
        entries_.push_back(entry);
    }
    
    // Measure existing mark to now or between two marks
    bool measure(const std::string& measureName, const std::string& startMark = "", 
              const std::string& endMark = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint64_t start_time;
        if (!startMark.empty() && marks_.find(startMark) != marks_.end()) {
            start_time = marks_[startMark].startTime;
        } else {
            start_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count());
        }
        
        uint64_t end_time;
        if (!endMark.empty() && marks_.find(endMark) != marks_.end()) {
            end_time = marks_[endMark].startTime;
        } else {
            end_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count());
        }
        
        auto duration = end_time - start_time;
        auto entry = create_entry(measureName, PerformanceEntryType::MEASURE, duration);
        entry.detail["startTime"] = std::to_string(start_time);
        entry.detail["duration"] = std::to_string(duration);
        
        measures_[measureName] = entry;
        entries_.push_back(entry);
        notify_observers(entry);
        
        return true;
    }
    
    // Get entries of given type
    std::vector<PerformanceEntry> get_entries(PerformanceEntryType type = PerformanceEntryType::MARK) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PerformanceEntry> result;
        
        for (const auto& entry : entries_) {
            if (entry.type == type || type == PerformanceEntryType::MARK) {
                result.push_back(entry);
            }
        }
        
        return result;
    }
    
    // Clear entries (clearResourceTimings alternative)
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
        marks_.clear();
        measures_.clear();
    }
    
    // Performance timeline work (generic)
    void time(const std::string& name) {
        time_start_[name] = std::chrono::steady_clock::now();
    }
    
    void timeEnd(const std::string& name) {
        auto start_it = time_start_.find(name);
        if (start_it != time_start_.end()) {
            auto start = start_it->second;
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            auto entry = create_entry(name, PerformanceEntryType::FUNCTION, static_cast<uint32_t>(duration));
            entries_.push_back(entry);
            time_start_.erase(start_it);
        }
    }
    
    // Resource timing API
    void add_resource(const ResourceTimingEntry& resource) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto duration = resource.responseEnd - resource.fetchStart;
        auto entry = create_entry(resource.name, PerformanceEntryType::RESOURCE, static_cast<uint32_t>(duration));
        entry.initiatorType = resource.initiatorType;
        entry.detail["fetchStart"] = std::to_string(resource.fetchStart);
        entry.detail["responseEnd"] = std::to_string(resource.responseEnd);
        entry.detail["duration"] = std::to_string(duration);
        entry.detail["encodedBodySize"] = resource.encodedBodySize;
        
        resources_.push_back(entry);
        entries_.push_back(entry);
        notify_observers(entry);
    }
    
    void get_resources(std::vector<ResourceTimingEntry>& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        out.clear();
        for (const auto& entry : resources_) {
            ResourceTimingEntry res;
            res.name = entry.name;
            res.initiatorType = entry.initiatorType;
            res.fetchStart = std::stoull(entry.detail.at("fetchStart"));
            res.responseEnd = std::stoull(entry.detail.at("responseEnd"));
            res.duration = std::stoul(entry.detail.at("duration"));
            res.encodedBodySize = entry.detail.at("encodedBodySize");
            out.push_back(res);
        }
    }
    
    // Use `performance.nodeTiming` equivalent
    void mark_node_style() {
        auto entry = create_entry("NodeTiming", PerformanceEntryType::FUNCTION, 0);
        entry.detail["node"] = "startup";
        entries_.push_back(entry);
    }
    
    // Frame timing (simplified)
    void mark_frame(const std::string& frame_type, uint32_t frame_id, uint32_t duration) {
        auto entry = create_entry(frame_type + "-" + frame_id, PerformanceEntryType::FRAME, duration);
        entry.detail["frame"] = std::to_string(frame_id);
        frames_.push_back(entry);
        entries_.push_back(entry);
        notify_observers(entry);
    }
    
    // Navigation timing
    void mark_navigation(const std::string& navigation_type, const std::string& url) {
        auto entry = create_entry("navigation-" + navigation_type, PerformanceEntryType::NAVIGATION, 0);
        entry.detail["type"] = navigation_type;
        entry.detail["url"] = url;
        navs_.push_back(entry);
        entries_.push_back(entry);
        notify_observers(entry);
    }
    
    // Layout performance
    void layout_duration(uint64_t time_elapsed, const std::string& description = {}) {
        auto entry = create_entry("Layout", PerformanceEntryType::LAYOUT, static_cast<uint32_t>(time_elapsed));
        if (!description.empty()) entry.detail["desc"] = description;
        entries_.push_back(entry);
        layout_times_++;
        total_layout_time_ += time_elapsed;
    }
    
    // JS function timing (critical paths)
    void js_function_timing(const std::string& func_name, uint64_t gc_count, uint64_t time_cost, 
                          const std::string& context = {}) {
        auto entry = create_entry(func_name, PerformanceEntryType::FUNCTION, static_cast<uint32_t>(time_cost));
        entry.detail["gc"] = std::to_string(gc_count);
        entry.detail["context"] = context;
        js_.push_back(entry);
        entries_.push_back(entry);
    }
    
    // Event timing
    void event_timing(const std::string& event_name, const std::string& target, uint64_t processing_time) {
        auto entry = create_entry("Event: " + event_name, PerformanceEntryType::EVENT, 
                               static_cast<uint32_t>(processing_time));
        entry.detail["target"] = target;
        events_.push_back(entry);
        entries_.push_back(entry);
    }
    
    // Calculate average measures
    double get_average_duration(PerformanceEntryType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> durations;
        
        for (const auto& entry : entries_) {
            if (entry.type == type) {
                durations.push_back(entry.duration);
            }
        }
        
        if (durations.empty()) return 0.0;
        uint32_t sum = 0;
        for (auto d : durations) sum += d;
        return static_cast<double>(sum) / durations.size();
    }
    
    // Get long tasks (simulate)
    std::vector<PerformanceEntry> get_long_tasks(uint64_t threshold_ms = 50) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PerformanceEntry> result;
        
        for (const auto& entry : entries_) {
            if (entry.duration >= threshold_ms) {
                result.push_back(entry);
            }
        }
        
        return result;
    }
    
    // Memory pressure simulation (simplified)
    void simulate_memory_pressure(uint64_t ratio_percent) {
        auto entry = create_entry("MemoryPressure", PerformanceEntryType::FUNCTION, static_cast<uint32_t>(ratio_percent));
        entry.detail["pressure"] = std::to_string(ratio_percent);
        entries_.push_back(entry);
    }
    
    // Connection timing
    void connection_timing(uint64_t dns_time, uint64_t tcp_time, uint64_t tls_time, 
                         bool secure, const std::string& host = {}) {
        std::string name = "Connection: " + (host.empty() ? "unknown" : host);
        auto entry = create_entry(name, PerformanceEntryType::RESOURCE, 
                               static_cast<uint32_t>(dns_time + tcp_time + tls_time));
        entry.detail["dns"] = std::to_string(dns_time);
        entry.detail["tcp"] = std::to_string(tcp_time);
        entry.detail["tls"] = std::to_string(tls_time);
        entry.detail["secure"] = secure ? "true" : "false";
        
        connections_.push_back(entry);
        entries_.push_back(entry);
    }

private:
    PerformanceEntry create_entry(const std::string& name, PerformanceEntryType type, uint32_t duration = 0) {
        PerformanceEntry entry;
        entry.name = name;
        entry.type = type;
        entry.startTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
        entry.timestamp = std::chrono::steady_clock::now();
        entry.duration = duration;
        
        return entry;
    }
    
    void notify_observers(const PerformanceEntry& entry) {
        for (const auto& observer : observers_) {
            observer->notify(entry);
        }
    }
    
    std::string allocation_type_name(PerformanceEntryType type) {
        switch (type) {
            case PerformanceEntryType::MARK: return "mark";
            case PerformanceEntryType::MEASURE: return "measure";
            case PerformanceEntryType::NAVIGATION: return "navigation";
            case PerformanceEntryType::RESOURCE: return "resource";
            case PerformanceEntryType::PAINT: return "paint";
            case PerformanceEntryType::FRAME: return "frameMutations";
            case PerformanceEntryType::LAYOUT: return "layouts";
            case PerformanceEntryType::JS: return "js";
            case PerformanceEntryType::FUNCTION: return "function";
            case PerformanceEntryType::EVENT: return "event";
            default: return "unknown";
        }
    }

    std::vector<PerformanceEntry> entries_;
    std::unordered_map<std::string, PerformanceEntry> marks_;
    std::unordered_map<std::string, PerformanceEntry> measures_;
    std::vector<PerformanceEntry> resources_;
    std::vector<PerformanceEntry> frames_;
    std::vector<PerformanceEntry> navs_;
    std::vector<PerformanceEntry> js_;
    std::vector<PerformanceEntry> events_;
    std::vector<PerformanceEntry> connections_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> time_start_;
    std::vector<std::shared_ptr<PerformanceObserver>> observers_;
    
    size_t layout_times_ = 0;
    uint64_t total_layout_time_ = 0;
    
    mutable std::mutex mutex_;
};

// Singleton wrapper for JS performance
class PerformanceGlobal {
public:
    static JSPerformance& instance() {
        static JSPerformance inst;
        return inst;
    }
    
    static void mark(const std::string& name) { instance().mark(name); }
    static bool measure(const std::string& name, const std::string& startMark = "", const std::string& endMark = "") {
        return instance().measure(name, startMark, endMark);
    }
    static std::vector<PerformanceEntry> get_entries() { return instance().get_entries(); }
    static void clear() { instance().clear(); }
    static void time(const std::string& name) { instance().time(name); }
    static void timeEnd(const std::string& name) { instance().timeEnd(name); }
    static void add_resource(const ResourceTimingEntry& res) { instance().add_resource(res); }
    static void layout_duration(uint64_t time, const std::string& desc = {}) { instance().layout_duration(time, desc); }
    static void js_function_timing(const std::string& func, uint64_t gc, uint64_t time, const std::string& ctx = {}) {
        instance().js_function_timing(func, gc, time, ctx);
    }
    static void event_timing(const std::string& event, const std::string& target, uint64_t time) {
        instance().event_timing(event, target, time);
    }
    static double get_average_duration(PerformanceEntryType type) { return instance().get_average_duration(type); }
};

} // namespace hyperion::debug::performance