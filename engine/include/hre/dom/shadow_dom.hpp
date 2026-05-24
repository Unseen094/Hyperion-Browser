#pragma once

#include "hre/dom/node.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace hre::dom {

class ShadowRoot : public DocumentFragment {
public:
    ShadowRoot(ShadowRootMode mode = ShadowRootMode::Open);

    ShadowRootMode mode() const { return mode_; }
    std::shared_ptr<Element> host() const { return host_; }
    void set_host(const std::shared_ptr<Element>& host) { host_ = host; }

    std::shared_ptr<Element> get_element_by_id(const std::wstring& id);
    std::vector<std::shared_ptr<Element>> query_selector_all(const std::wstring& selector);

    // Slot assignment
    void assign_slotted(const std::shared_ptr<Element>& slot, const std::vector<std::shared_ptr<Node>>& nodes);
    std::vector<std::shared_ptr<Node>> get_assigned_nodes(const std::shared_ptr<Element>& slot);
    std::vector<std::shared_ptr<Element>> get_assigned_slots(const std::shared_ptr<Element>& node);

private:
    ShadowRootMode mode_;
    std::shared_ptr<Element> host_;
    std::unordered_map<std::shared_ptr<Element>, std::vector<std::shared_ptr<Node>>> slot_assignments_;
};

// Slotable mixin via Node
inline bool is_slotted(const std::shared_ptr<Node>& node) {
    return node->assigned_slot_ != nullptr;
}

// Event retargeting for shadow DOM
inline std::shared_ptr<EventTarget> retarget_event(const std::shared_ptr<Event>& event, const std::shared_ptr<EventTarget>& root) {
    auto target = event->target();
    while (target && target != root) {
        if (auto el = std::dynamic_pointer_cast<Element>(target)) {
            auto shadow = el->shadow_root();
            if (shadow && shadow->mode() == ShadowRootMode::Closed) return nullptr;
        }
        target = target->parent_event_target();
    }
    return event->target();
}

} // namespace hre::dom
