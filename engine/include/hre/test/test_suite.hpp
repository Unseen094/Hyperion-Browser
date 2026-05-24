#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace hre::test {

enum class test_result {
    PASS,
    FAIL,
    SKIP,
    TIMEOUT,
    CRASH,
    ERROR
};

struct test_case {
    std::string name;
    std::string suite;
    std::string description;
    std::string expected;
    std::string actual;
    test_result result = test_result::PASS;
    uint64_t duration_us = 0;
    std::string message;
};

struct test_suite_info {
    std::string name;
    std::string path;
    size_t total_tests = 0;
    size_t passed = 0;
    size_t failed = 0;
    size_t skipped = 0;
    double duration_ms = 0;
};

class test_suite {
public:
    test_suite();
    ~test_suite();

    void register_test_case(const std::string& suite_name, const std::string& test_name,
                           std::function<void()> test_func,
                           std::function<void()> setup = nullptr,
                           std::function<void()> teardown = nullptr);

    void run_all();
    void run_suite(const std::string& suite_name);
    void run_test(const std::string& suite_name, const std::string& test_name);

    const std::vector<test_case>& results() const { return m_results; }
    const test_suite_info* get_suite_info(const std::string& name) const;

    size_t total_tests() const;
    size_t passed_tests() const;
    size_t failed_tests() const;

    std::string generate_report() const;
    void save_report(const std::string& filename) const;
    void save_json_report(const std::string& filename) const;

    void set_timeout(uint64_t timeout_ms) { m_timeout_ms = timeout_ms; }
    uint64_t timeout() const { return m_timeout_ms; }

    static std::string result_to_string(test_result result);

private:
    struct test_entry {
        std::string suite;
        std::string name;
        std::function<void()> test_func;
        std::function<void()> setup;
        std::function<void()> teardown;
    };

    void run_test_entry(const test_entry& entry);
    void update_suite_info(const std::string& suite_name);
    test_result execute_test(const test_entry& entry);

    std::vector<test_entry> m_tests;
    std::vector<test_case> m_results;
    std::map<std::string, test_suite_info> m_suite_infos;
    uint64_t m_timeout_ms = 30000;
};

class acid3_tester {
public:
    acid3_tester();
    ~acid3_tester();

    bool load_test_page(const std::string& path);
    bool run_acid3_test();

    double score() const { return m_score; }
    bool passed() const { return m_score >= 100.0; }

    const std::vector<test_case>& failures() const { return m_failures; }
    const std::string& test_url() const { return m_test_url; }

private:
    double m_score = 0.0;
    std::string m_test_url;
    std::vector<test_case> m_failures;
};

} // namespace hre::test