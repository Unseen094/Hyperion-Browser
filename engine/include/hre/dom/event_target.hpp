#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace hre::dom {

enum class event_phase {
    NONE,
    CAPTURING,
    AT_TARGET,
    BUBBLING
};

struct event {
    std::wstring type;
    bool bubbles = true;
    bool cancelable = true;
    bool propagation_stopped = false;
    event_phase phase = event_phase::NONE;
    class node* target = nullptr;
    class node* current_target = nullptr;
};

class event_target {
public:
    using listener_func = std::function<void(event&)>;

    void add_event_listener(const std::wstring& type, listener_func listener) {
        m_listeners[type].push_back(listener);
    }

    void dispatch_event(event& e) {
        // 1. Capturing Phase (would require tree traversal)
        // 2. Target Phase
        e.phase = event_phase::AT_TARGET;
        e.current_target = reinterpret_cast<node*>(this);
        
        if (m_listeners.count(e.type)) {
            for (auto& listener : m_listeners[e.type]) {
                if (e.propagation_stopped) break;
                listener(e);
            }
        }

        // 3. Bubbling Phase
        if (e.bubbles && !e.propagation_stopped) {
            bubble_event(e);
        }
    }

protected:
    virtual void bubble_event(event& e) = 0;

private:
    std::map<std::wstring, std::vector<listener_func>> m_listeners;
};

} // namespace hre::dom
