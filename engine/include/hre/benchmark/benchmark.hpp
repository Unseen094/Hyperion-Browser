#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace hre::benchmark {

struct benchmark_result {
    std::string name;
    double min_time_ms = 0.0;
    double max_time_ms = 0.0;
    double avg_time_ms = 0.0;
    double median_time_ms = 0.0;
    double std_dev_ms = 0.0;
    size_t sample_count = 0;
    double throughput = 0.0;
    std::string unit = "ms";
};

struct benchmark_config {
    size_t warmup_iterations = 3;
    size_t min_iterations = 10;
    size_t max_iterations = 1000;
    double min_duration_ms = 100.0;
    double target_precision_ms = 0.1;
};

class benchmark_suite {
public:
    benchmark_suite() = default;

    using benchmark_func = std::function<void()>;
    using tearDown_func = std::function<void()>;

    void register_benchmark(const std::string& name, benchmark_func func);
    void set_tear_down(tearDown_func func) { m_teardown = func; }

    void set_config(const benchmark_config& config) { m_config = config; }
    const benchmark_config& config() const { return m_config; }

    void run();
    void run(const std::string& name);

    const std::vector<benchmark_result>& results() const { return m_results; }
    const benchmark_result* get_result(const std::string& name) const;

    void clear_results();

    static std::string generate_report(const std::vector<benchmark_result>& results);
    std::string generate_report() const;

    void save_json(const std::string& filename) const;
    void save_csv(const std::string& filename) const;

private:
    benchmark_result run_benchmark(const std::string& name, benchmark_func func);

    struct benchmark_entry {
        std::string name;
        benchmark_func func;
    };

    std::vector<benchmark_entry> m_benchmarks;
    std::vector<benchmark_result> m_results;
    tearDown_func m_teardown;
    benchmark_config m_config;
};

class scoped_timer {
public:
    using time_point = std::chrono::high_resolution_clock::time_point;

    explicit scoped_timer(double& result);
    ~scoped_timer();

    scoped_timer(const scoped_timer&) = delete;
    scoped_timer& operator=(const scoped_timer&) = delete;

    void stop();

private:
    double& m_result;
    time_point m_start;
    bool m_stopped = false;
};

double now_ms();

} // namespace hre::benchmark