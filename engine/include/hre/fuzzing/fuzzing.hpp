#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace hre::fuzzing {

struct fuzz_config {
    size_t max_input_size = 1024 * 1024;
    size_t max_iterations = 100000;
    uint32_t seed = 0;
    bool exit_on_crash = true;
    size_t timeout_ms = 1000;
};

struct fuzz_result {
    size_t iterations = 0;
    size_t crashes = 0;
    size_t timeouts = 0;
    size_t unique_inputs = 0;
    double coverage = 0.0;
    bool success = false;
};

class fuzzing_input {
public:
    fuzzing_input() = default;
    explicit fuzzing_input(size_t size);
    fuzzing_input(const uint8_t* data, size_t size);
    ~fuzzing_input();

    const uint8_t* data() const { return m_data; }
    uint8_t* mutable_data() { return m_data; }
    size_t size() const { return m_size; }

    void resize(size_t new_size);

    template<typename T>
    const T* as() const {
        return reinterpret_cast<const T*>(m_data);
    }

private:
    friend class mutation_engine;
    uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
};

class mutation_engine {
public:
    mutation_engine();
    explicit mutation_engine(uint32_t seed);

    void set_seed(uint32_t seed);
    uint32_t seed() const { return m_seed; }

    fuzzing_input mutate(const fuzzing_input& input, size_t max_size);
    fuzzing_input mutate_bit_flip(const fuzzing_input& input, size_t max_size);
    fuzzing_input mutate_byte_flip(const fuzzing_input& input, size_t max_size);
    fuzzing_input mutate_insert_bytes(const fuzzing_input& input, size_t max_size);
    fuzzing_input mutate_delete_bytes(const fuzzing_input& input, size_t max_size);

    uint32_t next_random();

private:
    uint32_t m_seed = 0;
    uint32_t m_counter = 0;

    size_t random_size_t(size_t max);
};

class corpus {
public:
    corpus();
    ~corpus();

    void add(const fuzzing_input& input);
    void add_file(const std::string& path);

    size_t size() const { return m_inputs.size(); }
    const fuzzing_input& operator[](size_t i) const { return m_inputs[i]; }
    auto begin() const { return m_inputs.begin(); }
    auto end() const { return m_inputs.end(); }

    void minimize();

private:
    std::vector<fuzzing_input> m_inputs;
};

class fuzzing_runner {
public:
    using test_function = std::function<bool(const uint8_t*, size_t)>;

    fuzzing_runner();
    ~fuzzing_runner();

    void set_config(const fuzz_config& config);
    const fuzz_config& config() const { return m_config; }

    void add_test_function(test_function func);

    fuzz_result run(const std::wstring& target_name);
    fuzz_result run_from_corpus(corpus& input_corpus);

    void set_coverage_callback(std::function<void(const uint8_t*, size_t)> cb) {
        m_coverage_callback = cb;
    }

private:
    fuzz_config m_config;
    std::vector<test_function> m_test_functions;
    std::function<void(const uint8_t*, size_t)> m_coverage_callback;
    mutation_engine m_mutator;
};

class coverage_tracker {
public:
    coverage_tracker();

    void reset();
    void record_hit(uint64_t pc);

    double compute_coverage();
    size_t num_unique_hits() const { return m_hits.size(); }

    void enable() { m_enabled = true; }
    void disable() { m_enabled = false; }
    bool is_enabled() const { return m_enabled; }

private:
    bool m_enabled = true;
    std::vector<bool> m_hits;
    size_t m_map_size = 64 * 1024;
};

class crash_reporter {
public:
    crash_reporter();
    ~crash_reporter();

    void report_crash(const fuzzing_input& input, const std::string& reason);
    void save_input(const fuzzing_input& input, const std::string& filename);

    size_t num_crashes() const { return m_crashes.size(); }

    struct crash_info {
        fuzzing_input input;
        std::string reason;
        uint64_t timestamp;
    };

    const std::vector<crash_info>& crashes() const { return m_crashes; }

private:
    std::vector<crash_info> m_crashes;
};

} // namespace hre::fuzzing