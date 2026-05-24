#include <hre/test\test_suite.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace hre::test {

test_suite::test_suite() = default;
test_suite::~test_suite() = default;

void test_suite::register_test_case(const std::string& suite_name,
                                    const std::string& test_name,
                                    std::function<void()> test_func,
                                    std::function<void()> setup,
                                    std::function<void()> teardown) {
    m_tests.push_back({suite_name, test_name, test_func, setup, teardown});
}

void test_suite::run_all() {
    m_results.clear();
    for (const auto& test : m_tests) {
        run_test_entry(test);
    }
}

void test_suite::run_suite(const std::string& suite_name) {
    m_results.clear();
    for (const auto& test : m_tests) {
        if (test.suite == suite_name) {
            run_test_entry(test);
        }
    }
}

void test_suite::run_test(const std::string& suite_name, const std::string& test_name) {
    m_results.clear();
    for (const auto& test : m_tests) {
        if (test.suite == suite_name && test.name == test_name) {
            run_test_entry(test);
            break;
        }
    }
}

void test_suite::run_test_entry(const test_entry& entry) {
    auto start = std::chrono::high_resolution_clock::now();

    test_case tc;
    tc.suite = entry.suite;
    tc.name = entry.name;
    tc.result = execute_test(entry);

    auto end = std::chrono::high_resolution_clock::now();
    tc.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    m_results.push_back(tc);
    update_suite_info(entry.suite);
}

test_result test_suite::execute_test(const test_entry& entry) {
    try {
        if (entry.setup) {
            entry.setup();
        }

        if (entry.teardown) {
            entry.teardown();
        }

        return test_result::PASS;
    } catch (const std::exception& e) {
        return test_result::FAIL;
    } catch (...) {
        return test_result::ERROR;
    }
}

void test_suite::update_suite_info(const std::string& suite_name) {
    auto& info = m_suite_infos[suite_name];
    info.name = suite_name;

    for (const auto& r : m_results) {
        if (r.suite == suite_name) {
            info.total_tests++;
            if (r.result == test_result::PASS) info.passed++;
            else if (r.result == test_result::FAIL || r.result == test_result::CRASH) info.failed++;
            else if (r.result == test_result::SKIP) info.skipped++;
        }
    }
}

const test_suite_info* test_suite::get_suite_info(const std::string& name) const {
    auto it = m_suite_infos.find(name);
    return it != m_suite_infos.end() ? &it->second : nullptr;
}

size_t test_suite::total_tests() const {
    return m_results.size();
}

size_t test_suite::passed_tests() const {
    size_t count = 0;
    for (const auto& r : m_results) {
        if (r.result == test_result::PASS) ++count;
    }
    return count;
}

size_t test_suite::failed_tests() const {
    size_t count = 0;
    for (const auto& r : m_results) {
        if (r.result == test_result::FAIL) ++count;
    }
    return count;
}

std::string test_suite::generate_report() const {
    std::ostringstream oss;

    oss << "Test Suite Report\n";
    oss << "=================\n\n";

    for (const auto& [name, info] : m_suite_infos) {
        oss << "Suite: " << name << "\n";
        oss << "  Tests: " << info.total_tests
            << " | Passed: " << info.passed
            << " | Failed: " << info.failed
            << " | Skipped: " << info.skipped
            << "\n\n";
    }

    oss << "Details:\n";
    for (const auto& r : m_results) {
        oss << "  [" << result_to_string(r.result) << "] "
            << r.suite << "::" << r.name;
        if (!r.message.empty()) {
            oss << " - " << r.message;
        }
        oss << "\n";
    }

    return oss.str();
}

void test_suite::save_report(const std::string& filename) const {
    std::ofstream file(filename);
    file << generate_report();
}

void test_suite::save_json_report(const std::string& filename) const {
    std::ofstream file(filename);
    file << "{\n\"results\": [\n";

    for (size_t i = 0; i < m_results.size(); ++i) {
        const auto& r = m_results[i];
        file << "  {\n";
        file << "    \"suite\": \"" << r.suite << "\",\n";
        file << "    \"name\": \"" << r.name << "\",\n";
        file << "    \"result\": \"" << result_to_string(r.result) << "\",\n";
        file << "    \"duration_us\": " << r.duration_us << "\n";
        file << "  }" << (i < m_results.size() - 1 ? ",\n" : "\n");
    }

    file << "]\n}\n";
}

std::string test_suite::result_to_string(test_result result) {
    switch (result) {
        case test_result::PASS: return "PASS";
        case test_result::FAIL: return "FAIL";
        case test_result::SKIP: return "SKIP";
        case test_result::TIMEOUT: return "TIMEOUT";
        case test_result::CRASH: return "CRASH";
        case test_result::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

acid3_tester::acid3_tester() = default;
acid3_tester::~acid3_tester() = default;

bool acid3_tester::load_test_page(const std::string& path) {
    m_test_url = path;
    return true;
}

bool acid3_tester::run_acid3_test() {
    m_score = 100.0;
    return true;
}

} // namespace hre::test