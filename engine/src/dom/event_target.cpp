#include "hre/dom/event_target.hpp"
#include <algorithm>
#include <iostream>

namespace hre::dom {

void EventTarget::add_event_listener(const std::wstring& type, std::shared_ptr<EventListener> listener) {
    m_listeners[type].push_back(listener);
}

void EventTarget::remove_event_listener(const std::wstring& type, std::shared_ptr<EventListener> listener) {
    auto& listeners = m_listeners[type];
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}

bool EventTarget::dispatch_event(std::shared_ptr<Event> event) {
    auto it = m_listeners.find(event->type);
    if (it == m_listeners.end()) return true;

    for (auto& listener : it->second) {
        if (event->stopped) break;
        try {
            listener->handle_event(event);
        } catch (const std::exception& e) {
            std::wcerr << L"Error in event listener: " << e.what() << std::endl;
        }
    }
    return !event->stopped;
}

} // namespace hre::dom
