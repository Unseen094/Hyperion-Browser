#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

namespace hre::dom {

class Event;
class EventListener;

class EventTarget {
public:
    virtual ~EventTarget() = default;

    void add_event_listener(const std::wstring& type, std::shared_ptr<EventListener> listener);
    void remove_event_listener(const std::wstring& type, std::shared_ptr<EventListener> listener);
    bool dispatch_event(std::shared_ptr<Event> event);

protected:
    std::map<std::wstring, std::vector<std::shared_ptr<EventListener>>> m_listeners;
};

class Event {
public:
    Event(const std::wstring& type, bool bubbles = true, bool cancelable = false)
        : type(type), bubbles(bubbles), cancelable(cancelable), stopped(false) {}
    virtual ~Event() = default;

    std::wstring type;
    bool bubbles;
    bool cancelable;
    bool stopped;

    void stop_propagation() { stopped = true; }
};

class MouseEvent : public Event {
public:
    MouseEvent(const std::wstring& type, int client_x, int client_y)
        : Event(type, true, false), client_x(client_x), client_y(client_y) {}
    int client_x;
    int client_y;
};

class KeyboardEvent : public Event {
public:
    KeyboardEvent(const std::wstring& type, const std::wstring& key, int code)
        : Event(type, true, false), key(key), code(code) {}
    std::wstring key;
    int code;
};

class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void handle_event(std::shared_ptr<Event> event) = 0;
};

} // namespace hre::dom
