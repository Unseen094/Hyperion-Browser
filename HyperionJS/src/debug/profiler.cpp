#include <hjs/debug/profiler.hpp>
#include <sstream>
#include <iomanip>

namespace hjs::debug {

static Profiler s_instance;

Profiler& Profiler::instance() { return s_instance; }

void Profiler::begin_function(const std::wstring& name) {
    if (!m_enabled) return;
    m_current_function = name;
    m_current_start = std::chrono::high_resolution_clock::now();
}

void Profiler::end_function() {
    if (!m_enabled || m_current_function.empty()) return;
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_current_start).count();

    auto& entry = m_entries[m_current_function];
    entry.name = m_current_function;
    entry.call_count++;
    entry.total_time_ns += duration;
    if (entry.min_time_ns == 0 || duration < entry.min_time_ns) entry.min_time_ns = duration;
    if (duration > entry.max_time_ns) entry.max_time_ns = duration;

    m_current_function.clear();
}

void Profiler::log_opcode(const std::string&) {
    // Extensible: could track per-opcode frequency
}

void Profiler::reset() {
    m_entries.clear();
    m_running_timers.clear();
    m_instruction_count = 0;
    m_ic_hit_count = 0;
    m_ic_miss_count = 0;
}

std::string Profiler::report() const {
    std::ostringstream ss;
    ss << "\n=== Profiler Report ===\n";
    ss << "Total Instructions: " << m_instruction_count << "\n";
    ss << "IC Hits: " << m_ic_hit_count << " | Misses: " << m_ic_miss_count;
    if (m_ic_hit_count + m_ic_miss_count > 0) {
        ss << " | Hit Rate: " << std::fixed << std::setprecision(1)
           << (100.0 * m_ic_hit_count / (m_ic_hit_count + m_ic_miss_count)) << "%";
    }
    ss << "\n\nFunctions:\n";
    ss << std::left << std::setw(25) << "Name"
       << std::right << std::setw(10) << "Calls"
       << std::setw(15) << "Total(ms)"
       << std::setw(15) << "Avg(ms)"
       << std::setw(15) << "Min(ms)"
       << std::setw(15) << "Max(ms)" << "\n";
    ss << std::string(95, '-') << "\n";
    for (auto& [name, entry] : m_entries) {
        std::wstring wname(name.begin(), name.end());
        ss << std::left << std::setw(25) << std::string(wname.begin(), wname.end())
           << std::right << std::setw(10) << entry.call_count
           << std::setw(15) << std::fixed << std::setprecision(3) << entry.total_time_ms()
           << std::setw(15) << entry.avg_time_ms()
           << std::setw(15) << (entry.min_time_ns / 1e6)
           << std::setw(15) << (entry.max_time_ns / 1e6) << "\n";
    }
    ss << "========================\n";
    return ss.str();
}

} // namespace hjs::debug
