#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

namespace hre::threading {

class thread_pool {
public:
    explicit thread_pool(size_t num_threads = 0);
    ~thread_pool();

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    size_t num_threads() const { return m_num_threads; }
    size_t num_pending_tasks() const;

    void wait_all();

    void set_num_threads(size_t num_threads);
    void shutdown();

    bool is_shutdown() const { return m_shutdown.load(); }

private:
    void worker_thread();

    size_t m_num_threads = 0;
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_shutdown{false};
    std::atomic<size_t> m_pending_tasks{0};
};

inline thread_pool::thread_pool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 2;
    }
    set_num_threads(num_threads);
}

inline thread_pool::~thread_pool() {
    shutdown();
}

template<typename F, typename... Args>
auto thread_pool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_shutdown.load()) {
            return result;
        }

        m_tasks.emplace([task]() { (*task)(); });
        ++m_pending_tasks;
    }

    m_condition.notify_one();
    return result;
}

inline size_t thread_pool::num_pending_tasks() const {
    return m_pending_tasks.load();
}

inline void thread_pool::wait_all() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this] {
        return m_tasks.empty() && m_pending_tasks.load() == 0;
    });
}

inline void thread_pool::set_num_threads(size_t num_threads) {
    if (m_shutdown.load()) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    m_shutdown.store(true);
    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();

    m_num_threads = num_threads;
    m_shutdown.store(false);

    for (size_t i = 0; i < m_num_threads; ++i) {
        m_workers.emplace_back(&thread_pool::worker_thread, this);
    }
}

inline void thread_pool::shutdown() {
    if (m_shutdown.load()) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown.store(true);
    }

    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

inline void thread_pool::worker_thread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] {
                return m_shutdown.load() || !m_tasks.empty();
            });

            if (m_shutdown.load() && m_tasks.empty()) {
                return;
            }

            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
        }

        if (task) {
            task();
            if (m_pending_tasks.load() > 0) {
                --m_pending_tasks;
            }
        }
    }
}

} // namespace hre::threading