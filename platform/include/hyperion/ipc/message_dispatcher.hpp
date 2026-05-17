#pragma once

#include "../../platform/include/hyperion/platform/ipc/pipe_channel.hpp"
#include <string>
#include <map>
#include <functional>

namespace hyperion {
namespace ipc {

class message_dispatcher {
public:
    using handler_func = std::function<void(const std::vector<uint8_t>&)>;

    void register_handler(platform::ipc::message_type type, handler_func handler) {
        m_handlers[type] = handler;
    }

    void dispatch(const platform::ipc::message& msg) {
        auto it = m_handlers.find(msg.header.type);
        if (it != m_handlers.end()) {
            it->second(msg.payload);
        }
    }

    void dispatch_raw(const platform::ipc::message& msg) {
        dispatch(msg);
    }

private:
    std::map<platform::ipc::message_type, handler_func> m_handlers;
};

} // namespace ipc
} // namespace hyperion
