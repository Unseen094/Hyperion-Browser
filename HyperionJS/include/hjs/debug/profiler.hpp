#pragma once

#include <hjs/core/value.hpp>
#include <string>
#include <unordered_map>
#include <chrono>

namespace hjs::debug {

struct ProfileEntry {
    std::wstring name;
    int call_count = 0;
    int64_t total_time_ns = 0;
    int64_t min_time_ns = 0;
    int64_t max_time_ns = 0;

    double avg_time_ms() const {
        return call_count > 0 ? (total_time_ns / 1e6) / call_count : 0.0;
    }
    double total_time_ms() const { return total_time_ns / 1e6; }
};

class Profiler {
public:
    Profiler() : m_enabled(false) {}

    void enable() { m_enabled = true; }
    void disable() { m_enabled = false; }
    bool is_enabled() const { return m_enabled; }

    void begin_function(const std::wstring& name);
    void end_function();

    void log_opcode(const std::string& opcode);

    void reset();
    std::string report() const;

    const std::unordered_map<std::wstring, ProfileEntry>& entries() const { return m_entries; }

    // Execution counters
    int total_instructions_executed() const { return m_instruction_count; }
    void increment_instructions() { if (m_enabled) m_instruction_count++; }

    int ic_hits() const { return m_ic_hit_count; }
    int ic_misses() const { return m_ic_miss_count; }
    void record_ic_hit() { if (m_enabled) m_ic_hit_count++; }
    void record_ic_miss() { if (m_enabled) m_ic_miss_count++; }

    static Profiler& instance();

private:
    bool m_enabled;
    int m_instruction_count = 0;
    int m_ic_hit_count = 0;
    int m_ic_miss_count = 0;

    std::unordered_map<std::wstring, ProfileEntry> m_entries;
    std::unordered_map<std::wstring, int64_t> m_running_timers;
    std::chrono::high_resolution_clock::time_point m_current_start;
    std::wstring m_current_function;
};

} // namespace hjs::debug
