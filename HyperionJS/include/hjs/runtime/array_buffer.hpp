#pragma once

#include <hjs/core/value.hpp>
#include <hjs/runtime/object.hpp>
#include <vector>
#include <cstdint>

namespace hjs {

class ArrayBuffer : public JSObject {
public:
    ArrayBuffer() = default;
    explicit ArrayBuffer(size_t byte_length, bool resizable = false, size_t max_byte_length = 0);

    size_t byte_length() const { return m_data.size(); }
    bool is_resizable() const { return m_resizable; }
    size_t max_byte_length() const { return m_max_byte_length; }
    bool is_detached() const { return m_detached; }

    uint8_t* data() { return m_data.data(); }
    const uint8_t* data() const { return m_data.data(); }

    Value slice(size_t begin, size_t end) const;
    Value transfer(size_t new_length = 0);
    Value transfer_to_fixed_length(size_t new_length = 0);

    bool resize(size_t new_length);

    static Value create(Value, int argc, Value* args);
    static Value create_shared(Value, int argc, Value* args);

    static std::shared_ptr<ArrayBuffer> create_detached(size_t length);

private:
    std::vector<uint8_t> m_data;
    bool m_resizable = false;
    size_t m_max_byte_length = 0;
    bool m_detached = false;
};

class SharedArrayBuffer : public ArrayBuffer {
public:
    SharedArrayBuffer() = default;
    explicit SharedArrayBuffer(size_t byte_length);
};

} // namespace hjs
