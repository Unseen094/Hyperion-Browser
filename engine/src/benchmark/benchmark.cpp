#include <hre/benchmark/benchmark.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace hre::benchmark {

void benchmark_suite::register_benchmark(const std::string& name, benchmark_func func) {
    m_benchmarks.push_back({name, func});
}

void benchmark_suite::run() {
    for (const auto& entry : m_benchmarks) {
        run(entry.name);
    }
}

void benchmark_suite::run(const std::string& name) {
    auto it = std::find_if(m_benchmarks.begin(), m_benchmarks.end(),
        [&name](const benchmark_entry& e) { return e.name == name; });

    if (it != m_benchmarks.end()) {
        auto result = run_benchmark(name, it->func);
        m_results.push_back(result);
    }
}

benchmark_result benchmark_suite::run_benchmark(const std::string& name, benchmark_func func) {
    benchmark_result result;
    result.name = name;

    std::vector<double> times;

    for (size_t i = 0; i < m_config.warmup_iterations; ++i) {
        func();
        if (m_teardown) m_teardown();
    }

    size_t iterations = m_config.min_iterations;
    while (iterations < m_config.max_iterations) {
        times.clear();
        double total_time = 0.0;

        for (size_t i = 0; i < iterations; ++i) {
            double elapsed = 0.0;
            {
                scoped_timer timer(elapsed);
                func();
            }
            times.push_back(elapsed);
            total_time += elapsed;

            if (m_teardown) m_teardown();
        }

        result.avg_time_ms = total_time / iterations;

        if (total_time >= m_config.min_duration_ms) {
            break;
        }

        iterations *= 2;
    }

    std::sort(times.begin(), times.end());
    result.sample_count = times.size();

    result.min_time_ms = times.front();
    result.max_time_ms = times.back();
    result.avg_time_ms = 0.0;

    double sum = 0.0;
    for (double t : times) {
        sum += t;
    }
    result.avg_time_ms = sum / times.size();

    size_t mid = times.size() / 2;
    result.median_time_ms = (times.size() % 2 == 0) ? (times[mid - 1] + times[mid]) * 0.5 : times[mid];

    double sq_sum = 0.0;
    for (double t : times) {
        sq_sum += (t - result.avg_time_ms) * (t - result.avg_time_ms);
    }
    result.std_dev_ms = std::sqrt(sq_sum / times.size());

    return result;
}

const benchmark_result* benchmark_suite::get_result(const std::string& name) const {
    for (const auto& r : m_results) {
        if (r.name == name) return &r;
    }
    return nullptr;
}

void benchmark_suite::clear_results() {
    m_results.clear();
}

std::string benchmark_suite::generate_report(const std::vector<benchmark_result>& results) {
    std::ostringstream oss;
    oss << std::left << std::setw(30) << "Name"
        << std::right << std::setw(12) << "Min (ms)"
        << std::setw(12) << "Max (ms)"
        << std::setw(12) << "Avg (ms)"
        << std::setw(12) << "Median (ms)"
        << std::setw(12) << "StdDev (ms)"
        << std::setw(10) << "Samples"
        << "\n";

    oss << std::string(90, '-') << "\n";

    for (const auto& r : results) {
        oss << std::left << std::setw(30) << r.name
            << std::right << std::setw(12) << std::fixed << std::setprecision(3) << r.min_time_ms
            << std::setw(12) << r.max_time_ms
            << std::setw(12) << r.avg_time_ms
            << std::setw(12) << r.median_time_ms
            << std::setw(12) << r.std_dev_ms
            << std::setw(10) << r.sample_count
            << "\n";
    }

    return oss.str();
}

std::string benchmark_suite::generate_report() const {
    return generate_report(m_results);
}

void benchmark_suite::save_json(const std::string& filename) const {
    std::ofstream file(filename);
    file << "{\n\"benchmarks\": [\n";

    for (size_t i = 0; i < m_results.size(); ++i) {
        const auto& r = m_results[i];
        file << "  {\n";
        file << "    \"name\": \"" << r.name << "\",\n";
        file << "    \"min_time_ms\": " << r.min_time_ms << ",\n";
        file << "    \"max_time_ms\": " << r.max_time_ms << ",\n";
        file << "    \"avg_time_ms\": " << r.avg_time_ms << ",\n";
        file << "    \"median_time_ms\": " << r.median_time_ms << ",\n";
        file << "    \"std_dev_ms\": " << r.std_dev_ms << ",\n";
        file << "    \"sample_count\": " << r.sample_count << "\n";
        file << "  }" << (i < m_results.size() - 1 ? ",\n" : "\n");
    }

    file << "]\n}\n";
}

void benchmark_suite::save_csv(const std::string& filename) const {
    std::ofstream file(filename);
    file << "Name,Min (ms),Max (ms),Avg (ms),Median (ms),StdDev (ms),Samples\n";

    for (const auto& r : m_results) {
        file << "\"" << r.name << "\","
             << r.min_time_ms << ","
             << r.max_time_ms << ","
             << r.avg_time_ms << ","
             << r.median_time_ms << ","
             << r.std_dev_ms << ","
             << r.sample_count << "\n";
    }
}

scoped_timer::scoped_timer(double& result) : m_result(result) {
    m_start = std::chrono::high_resolution_clock::now();
}

scoped_timer::~scoped_timer() {
    if (!m_stopped) {
        stop();
    }
}

void scoped_timer::stop() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
    m_result = duration.count() / 1000.0;
    m_stopped = true;
}

double now_ms() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace hre::benchmark