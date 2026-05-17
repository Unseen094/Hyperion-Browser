#pragma once

#include <hre/dom/node.hpp>
#include <hjs/core/value.hpp>
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace hre::script {

class script_engine {
public:
    explicit script_engine(dom::document* doc);
    
    void execute(const std::wstring& script);
    void trigger_event(dom::node* target, const std::wstring& event_type);

    static dom::element* find_element_by_id(dom::node* root, const std::wstring& id);

    // Event listener registration
    void add_event_listener(dom::node* target, const std::wstring& event_type, hjs::Value listener);

private:
    dom::document* m_document;
    
    // Mapping: Node -> EventType -> List of JS Functions
    std::map<dom::node*, std::map<std::wstring, std::vector<hjs::Value>>> m_listeners;
};

} // namespace hre::script
