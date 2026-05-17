#pragma once

#include <hjs/core/value.hpp>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace hjs::vm { class Chunk; }

namespace hjs {

class JSObject {
public:
    virtual ~JSObject() = default;

    void set_prototype(std::shared_ptr<JSObject> proto) {
        m_prototype = std::move(proto);
    }
    std::shared_ptr<JSObject> prototype() const { return m_prototype; }

    virtual void set_property(const std::wstring& name, Value value) {
        m_properties[name] = std::move(value);
    }

    virtual Value* get_property(const std::wstring& name) {
        if (m_properties.count(name)) {
            return &m_properties[name];
        }
        if (m_prototype) {
            return m_prototype->get_property(name);
        }
        return nullptr;
    }

    virtual bool delete_property(const std::wstring& name) {
        auto it = m_properties.find(name);
        if (it != m_properties.end()) {
            m_properties.erase(it);
            return true;
        }
        return false;
    }

private:
    std::map<std::wstring, Value> m_properties;
    std::shared_ptr<JSObject> m_prototype;
};

class JSBoundFunction : public JSObject {
public:
    JSBoundFunction(Value receiver, Value method)
        : m_receiver(std::move(receiver)), m_method(std::move(method)) {}

    Value receiver() const { return m_receiver; }
    Value method() const { return m_method; }

private:
    Value m_receiver;
    Value m_method;
};

class JSArray : public JSObject {
public:
    std::vector<Value> elements;

    Value* get_index(int index) {
        if (index >= 0 && index < (int)elements.size()) {
            return &elements[index];
        }
        return nullptr;
    }

    void set_index(int index, Value value) {
        if (index >= (int)elements.size()) {
            elements.resize(index + 1);
        }
        elements[index] = std::move(value);
        set_property(L"length", Value((double)elements.size()));
    }
};

class JSFunction : public JSObject {
public:
    JSFunction(std::wstring name, int arity, const hjs::vm::Chunk* chunk)
        : m_name(std::move(name)), m_arity(arity), m_chunk(chunk) {}

    const std::wstring& name() const { return m_name; }
    int arity() const { return m_arity; }
    const hjs::vm::Chunk* chunk() const { return m_chunk; }
    int upvalue_count() const { return m_upvalue_count; }
    void set_upvalue_count(int count) { m_upvalue_count = count; }

private:
    std::wstring m_name;
    int m_arity;
    int m_upvalue_count = 0;
    const hjs::vm::Chunk* m_chunk;
};

typedef Value (*NativeFn)(Value receiver, int arg_count, Value* args);

class NativeFunction : public JSObject {
public:
    NativeFunction(NativeFn function) : m_function(function) {}
    NativeFn function() const { return m_function; }

private:
    NativeFn m_function;
};

// ---- Closures & Upvalues -------------------------------------------------

class Upvalue : public JSObject {
public:
    Upvalue(Value* value) : m_value_ptr(value) {}
    
    Value* value() { return m_value_ptr; }
    const Value* value() const { return m_value_ptr; }
    
    void close() {
        m_closed = *m_value_ptr;
        m_value_ptr = &m_closed;
    }

private:
    Value* m_value_ptr;
    Value m_closed; // Storage when the stack frame is gone
};

class JSClosure : public JSObject {
public:
    JSClosure(std::shared_ptr<JSFunction> function) : m_function(function) {}
    
    std::shared_ptr<JSFunction> function() const { return m_function; }
    std::vector<std::shared_ptr<Upvalue>>& upvalues() { return m_upvalues; }
    const std::vector<std::shared_ptr<Upvalue>>& upvalues() const { return m_upvalues; }

private:
    std::shared_ptr<JSFunction> m_function;
    std::vector<std::shared_ptr<Upvalue>> m_upvalues;
};

} // namespace hjs
