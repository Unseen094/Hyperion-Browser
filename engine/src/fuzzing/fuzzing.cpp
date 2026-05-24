#include <hre\fuzzing\fuzzing.hpp>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>

namespace hre::fuzzing {

fuzzing_input::fuzzing_input(size_t size) : m_size(size), m_capacity(size) {
    m_data = new uint8_t[size];
}

fuzzing_input::fuzzing_input(const uint8_t* data, size_t size) : m_size(size), m_capacity(size) {
    m_data = new uint8_t[size];
    std::memcpy(m_data, data, size);
}

fuzzing_input::~fuzzing_input() {
    delete[] m_data;
}

void fuzzing_input::resize(size_t new_size) {
    if (new_size > m_capacity) {
        uint8_t* new_data = new uint8_t[new_size];
        if (m_data) {
            std::memcpy(new_data, m_data, m_size);
            delete[] m_data;
        }
        m_data = new_data;
        m_capacity = new_size;
    }
    m_size = new_size;
}

mutation_engine::mutation_engine() {
    set_seed(static_cast<uint32_t>(std::time(nullptr)));
}

mutation_engine::mutation_engine(uint32_t seed) : m_seed(seed) {}

void mutation_engine::set_seed(uint32_t seed) {
    m_seed = seed;
}

uint32_t mutation_engine::next_random() {
    m_counter++;
    uint32_t val = m_seed;
    val ^= val << 13;
    val ^= val >> 17;
    val ^= val << 5;
    m_seed = val;
    return val;
}

size_t mutation_engine::random_size_t(size_t max) {
    return next_random() % max;
}

fuzzing_input mutation_engine::mutate(const fuzzing_input& input, size_t max_size) {
    switch (next_random() % 5) {
    case 0: return mutate_bit_flip(input, max_size);
    case 1: return mutate_byte_flip(input, max_size);
    case 2: return mutate_insert_bytes(input, max_size);
    case 3: return mutate_delete_bytes(input, max_size);
    default: return mutate_byte_flip(input, max_size);
    }
}

fuzzing_input mutation_engine::mutate_bit_flip(const fuzzing_input& input, size_t max_size) {
    const uint8_t* data = input.data();
    size_t size = input.size();
    fuzzing_input result(data, size);
    if (size > 0) {
        size_t idx = random_size_t(size);
        result.m_data[idx] ^= (1 << (next_random() % 8));
    }
    return result;
}

fuzzing_input mutation_engine::mutate_byte_flip(const fuzzing_input& input, size_t max_size) {
    const uint8_t* data = input.data();
    size_t size = input.size();
    fuzzing_input result(data, size);
    if (size > 0) {
        size_t idx = random_size_t(size);
        result.m_data[idx] = static_cast<uint8_t>(next_random());
    }
    return result;
}

fuzzing_input mutation_engine::mutate_insert_bytes(const fuzzing_input& input, size_t max_size) {
    const uint8_t* data = input.data();
    size_t size = input.size();
    size_t new_size = std::min(size + 4, max_size);
    fuzzing_input result(new_size);

    size_t idx = random_size_t(size + 1);
    for (size_t i = 0; i < idx; ++i) {
        result.m_data[i] = data[i];
    }
    for (size_t i = idx; i < idx + 4 && i < new_size; ++i) {
        result.m_data[i] = static_cast<uint8_t>(next_random());
    }
    for (size_t i = idx + 4; i < new_size; ++i) {
        result.m_data[i] = data[i - 4];
    }

    return result;
}

fuzzing_input mutation_engine::mutate_delete_bytes(const fuzzing_input& input, size_t max_size) {
    const uint8_t* data = input.data();
    size_t size = input.size();
    if (size <= 1) return fuzzing_input(data, size);

    size_t idx = random_size_t(size);
    size_t del_len = std::min(size_t(4), size - idx - 1);
    size_t new_size = size - del_len;

    fuzzing_input result(new_size);

    for (size_t i = 0; i < idx; ++i) {
        result.m_data[i] = data[i];
    }
    for (size_t i = idx; i < new_size; ++i) {
        result.m_data[i] = data[i + del_len];
    }

    return result;
}

corpus::corpus() = default;
corpus::~corpus() = default;

void corpus::add(const fuzzing_input& input) {
    m_inputs.push_back(input);
}

void corpus::add_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);

        m_inputs.emplace_back(data.data(), size);
    }
}

void corpus::minimize() {
}

fuzzing_runner::fuzzing_runner() : m_mutator(m_config.seed) {}
fuzzing_runner::~fuzzing_runner() = default;

void fuzzing_runner::set_config(const fuzz_config& config) {
    m_config = config;
    m_mutator.set_seed(config.seed);
}

void fuzzing_runner::add_test_function(test_function func) {
    m_test_functions.push_back(func);
}

fuzz_result fuzzing_runner::run(const std::wstring& target_name) {
    fuzz_result result;

    for (size_t i = 0; i < m_config.max_iterations && result.crashes == 0; ++i) {
        fuzzing_input input(256);
        for (size_t j = 0; j < 256; ++j) {
            input.mutable_data()[j] = static_cast<uint8_t>(m_mutator.next_random());
        }

        bool test_passed = true;
        for (const auto& func : m_test_functions) {
            if (!func(input.data(), input.size())) {
                test_passed = false;
                break;
            }
        }

        if (!test_passed) {
            result.crashes++;
        }

        result.iterations++;
    }

    result.success = (result.crashes == 0);
    return result;
}

fuzz_result fuzzing_runner::run_from_corpus(corpus& input_corpus) {
    fuzz_result result;

    for (const auto& input : input_corpus) {
        for (size_t i = 0; i < 100; ++i) {
            auto mutated = m_mutator.mutate(input, m_config.max_input_size);

            bool test_passed = true;
            for (const auto& func : m_test_functions) {
                if (!func(mutated.data(), mutated.size())) {
                    test_passed = false;
                    break;
                }
            }

            if (!test_passed) {
                result.crashes++;
            }

            result.iterations++;
        }
    }

    result.success = (result.crashes == 0);
    return result;
}

coverage_tracker::coverage_tracker() {
    m_hits.resize(m_map_size);
}

void coverage_tracker::reset() {
    std::fill(m_hits.begin(), m_hits.end(), false);
}

void coverage_tracker::record_hit(uint64_t pc) {
    if (!m_enabled) return;
    size_t idx = (pc * 0x9e3779b9) >> (64 - 16);
    idx = idx % m_map_size;
    m_hits[idx] = true;
}

double coverage_tracker::compute_coverage() {
    size_t count = 0;
    for (bool hit : m_hits) {
        if (hit) ++count;
    }
    return static_cast<double>(count) / m_map_size * 100.0;
}

crash_reporter::crash_reporter() = default;
crash_reporter::~crash_reporter() = default;

void crash_reporter::report_crash(const fuzzing_input& input, const std::string& reason) {
    crash_info info;
    info.input = input;
    info.reason = reason;
    info.timestamp = static_cast<uint64_t>(std::time(nullptr));
    m_crashes.push_back(info);
}

void crash_reporter::save_input(const fuzzing_input& input, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (file) {
        file.write(reinterpret_cast<const char*>(input.data()), input.size());
    }
}

} // namespace hre::fuzzing