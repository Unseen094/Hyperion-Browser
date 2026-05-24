#include <hjs/runtime/array_buffer.hpp>
#include <cstring>
#include <algorithm>

namespace hjs {

ArrayBuffer::ArrayBuffer(size_t byte_length, bool resizable, size_t max_byte_length)
    : m_data(byte_length, 0)
    , m_resizable(resizable)
    , m_max_byte_length(max_byte_length > 0 ? max_byte_length : byte_length)
    , m_detached(false)
{
    set_property(L"byteLength", Value(static_cast<double>(byte_length)));
}

Value ArrayBuffer::slice(size_t begin, size_t end) const {
    if (m_detached) return Value();
    auto buf = std::make_shared<ArrayBuffer>(end - begin);
    std::memcpy(buf->data(), m_data.data() + begin, end - begin);
    return Value(buf);
}

Value ArrayBuffer::transfer(size_t new_length) {
    if (m_detached) return Value();
    if (new_length == 0) new_length = m_data.size();

    auto new_buf = std::make_shared<ArrayBuffer>(new_length, m_resizable, m_max_byte_length);
    size_t copy_len = std::min(m_data.size(), new_length);
    std::memcpy(new_buf->data(), m_data.data(), copy_len);

    m_detached = true;
    m_data.clear();
    set_property(L"byteLength", Value(static_cast<double>(0)));

    return Value(std::move(new_buf));
}

Value ArrayBuffer::transfer_to_fixed_length(size_t new_length) {
    if (m_detached) return Value();
    if (new_length == 0) new_length = m_data.size();

    auto new_buf = std::make_shared<ArrayBuffer>(new_length, false, 0);
    size_t copy_len = std::min(m_data.size(), new_length);
    std::memcpy(new_buf->data(), m_data.data(), copy_len);

    m_detached = true;
    m_data.clear();
    set_property(L"byteLength", Value(static_cast<double>(0)));

    return Value(std::move(new_buf));
}

bool ArrayBuffer::resize(size_t new_length) {
    if (m_detached || !m_resizable) return false;
    if (new_length > m_max_byte_length) return false;
    m_data.resize(new_length);
    set_property(L"byteLength", Value(static_cast<double>(new_length)));
    return true;
}

Value ArrayBuffer::create(Value receiver, int argc, Value* args) {
    size_t length = 0;
    if (argc > 0 && args[0].is_number()) {
        length = static_cast<size_t>(args[0].as_number());
    }

    bool resizable = false;
    size_t max_len = 0;
    if (argc > 1 && args[1].is_object()) {
        auto options = args[1].as_object();
        Value* max_val = options->get_property(L"maxByteLength");
        if (max_val && max_val->is_number()) {
            max_len = static_cast<size_t>(max_val->as_number());
            resizable = true;
        }
    }

    auto buf = std::make_shared<ArrayBuffer>(length, resizable, max_len);
    return Value(buf);
}

Value ArrayBuffer::create_shared(Value receiver, int argc, Value* args) {
    size_t length = 0;
    if (argc > 0 && args[0].is_number()) {
        length = static_cast<size_t>(args[0].as_number());
    }
    auto buf = std::make_shared<SharedArrayBuffer>(length);
    return Value(buf);
}

SharedArrayBuffer::SharedArrayBuffer(size_t byte_length)
    : ArrayBuffer(byte_length, false, 0)
{
}

} // namespace hjs
