#pragma once

#include <hjs/core/value.hpp>
#include <map>
#include <string>
#include <memory>

namespace hjs::runtime {

class Environment {
public:
    Environment() : m_enclosing(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> enclosing) 
        : m_enclosing(std::move(enclosing)) {}

    void define(const std::wstring& name, Value value) {
        m_values[name] = std::move(value);
    }

    bool assign(const std::wstring& name, Value value) {
        if (m_values.count(name)) {
            m_values[name] = std::move(value);
            return true;
        }

        if (m_enclosing) {
            return m_enclosing->assign(name, std::move(value));
        }

        return false;
    }

    Value* get(const std::wstring& name) {
        if (m_values.count(name)) {
            return &m_values[name];
        }

        if (m_enclosing) {
            return m_enclosing->get(name);
        }

        return nullptr;
    }

private:
    std::shared_ptr<Environment> m_enclosing;
    std::map<std::wstring, Value> m_values;
};

} // namespace hjs::runtime
