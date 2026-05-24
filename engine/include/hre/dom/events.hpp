#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <algorithm>

namespace hre::dom {

// Forward declarations
class EventTarget;
class Event;
class EventListener;

// Base Event Class
class Event {
public:
    Event(const std::wstring& type, bool bubbles = true, bool cancelable = false)
        : type(type), bubbles(bubbles), cancelable(cancelable), stopped(false), default_prevented(false) {}
    virtual ~Event() = default;

    std::wstring type;
    bool bubbles;
    bool cancelable;
    bool stopped;
    bool default_prevented;

    void stop_propagation() { stopped = true; }
    void prevent_default() { default_prevented = true; }
};

// Mouse Event
class MouseEvent : public Event {
public:
    MouseEvent(const std::wstring& type, int client_x = 0, int client_y = 0, int button = 0)
        : Event(type, true, false), client_x(client_x), client_y(client_y), button(button) {}
    
    int client_x;
    int client_y;
    int button;
};

// Keyboard Event
class KeyboardEvent : public Event {
public:
    KeyboardEvent(const std::wstring& type, const std::wstring& key, int code = 0, bool shift = false, bool ctrl = false, bool alt = false)
        : Event(type, true, false), key(key), code(code), shift_key(shift), ctrl_key(ctrl), alt_key(alt) {}
    
    std::wstring key;
    int code;
    bool shift_key;
    bool ctrl_key;
    bool alt_key;
};

// Event Listener Interface
class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void handle_event(std::shared_ptr<Event> event) = 0;
};

// JS Event Listener Wrapper
class JSEventListener : public EventListener {
public:
    using CallbackType = std::function<void(std::shared_ptr<Event>)>;
    
    JSEventListener(CallbackType callback) : callback(callback) {}
    
    void handle_event(std::shared_ptr<Event> event) override {
        if (callback) {
            callback(event);
        }
    }

private:
    CallbackType callback;
};

// EventTarget Mixin
class EventTarget {
public:
    virtual ~EventTarget() = default;

    void add_event_listener(const std::wstring& type, std::shared_ptr<EventListener> listener) {
        m_listeners[type].push_back(listener);
    }

    void remove_event_listener(const std::wstring& type, std::shared_ptr<EventListener> listener) {
        auto& list = m_listeners[type];
        list.erase(std::remove(list.begin(), list.end(), listener), list.end());
    }

    bool dispatch_event(std::shared_ptr<Event> event) {
        auto it = m_listeners.find(event->type);
        if (it == m_listeners.end()) return true;

        for (auto& listener : it->second) {
            if (event->stopped) break;
            try {
                listener->handle_event(event);
            } catch (...) {
                // Ignore listener errors
            }
        }
        return !event->stopped;
    }

protected:
    std::map<std::wstring, std::vector<std::shared_ptr<EventListener>>> m_listeners;
};

} // namespace hre::dom
