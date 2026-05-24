#pragma once

#include <hjs/core/value.hpp>
#include <hjs/runtime/object.hpp>
#include <functional>
#include <vector>

namespace hjs {

class Promise : public JSObject {
public:
    enum class State { Pending, Fulfilled, Rejected };

    Promise() : m_state(State::Pending) {}

    void resolve(Value value) {
        if (m_state != State::Pending) return;
        m_state = State::Fulfilled;
        m_result = std::move(value);
        run_callbacks();
    }

    void reject(Value value) {
        if (m_state != State::Pending) return;
        m_state = State::Rejected;
        m_result = std::move(value);
        run_callbacks();
    }

    void then(std::function<void(Value)> callback) {
        if (m_state == State::Fulfilled) {
            callback(m_result);
        } else if (m_state == State::Pending) {
            m_then_callbacks.push_back(std::move(callback));
        }
    }

    State state() const { return m_state; }
    const Value& result() const { return m_result; }

    void catch_error(std::function<void(Value)> callback) {
        if (m_state == State::Rejected) {
            callback(m_result);
        } else if (m_state == State::Pending) {
            m_catch_callbacks.push_back(std::move(callback));
        }
    }

private:
    State m_state;
    Value m_result;
    std::vector<std::function<void(Value)>> m_then_callbacks;
    std::vector<std::function<void(Value)>> m_catch_callbacks;

    void run_callbacks() {
        if (m_state == State::Fulfilled) {
            for (auto& cb : m_then_callbacks) {
                cb(m_result);
            }
        } else if (m_state == State::Rejected) {
            for (auto& cb : m_catch_callbacks) {
                cb(m_result);
            }
        }
    }
};

} // namespace hjs
