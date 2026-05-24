#pragma once

#include <hre/dom/node.hpp>
#include <hjs/core/value.hpp>
#include <hjs/vm/vm.hpp>
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace hre::script {

class JSEventListener : public dom::EventListener {
public:
    JSEventListener(hjs::Value callback) : m_callback(std::move(callback)) {}

    void handle_event(std::shared_ptr<dom::Event> event) override;

private:
    hjs::Value m_callback;
};

struct ClickTarget {
    dom::element* element;
    std::wstring onclick_code;
    double x, y, width, height;
};

class script_engine {
public:
    explicit script_engine(dom::document* doc);
    ~script_engine();

    void execute(const std::wstring& script);
    void execute_scripts();
    void execute_chunk(hjs::vm::Chunk& chunk);
    void trigger_event(dom::node* target, const std::wstring& event_type);
    void trigger_keyboard_event(dom::node* target, const std::wstring& event_type,
                                const std::wstring& key, const std::wstring& code,
                                bool ctrl, bool alt, bool shift);

    static dom::element* find_element_by_id(dom::node* root, const std::wstring& id);

    void add_event_listener(dom::node* target, const std::wstring& event_type, hjs::Value listener);

    void wire_event_handlers();
    void set_click_targets(const std::vector<ClickTarget>& targets);
    dom::element* hit_test_click(int x, int y, float scroll_top, float y_offset);

private:
    void ensure_vm();
    dom::document* m_document;
    std::unique_ptr<hjs::vm::VM> m_vm;
    bool m_vm_initialized = false;
    std::map<dom::node*, std::map<std::wstring, std::vector<hjs::Value>>> m_listeners;
    std::vector<ClickTarget> m_click_targets;
};

} // namespace hre::script
