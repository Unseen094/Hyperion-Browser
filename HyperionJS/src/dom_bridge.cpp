#include <HyperionJS/include/hyperion_js/vm.hpp>
#include <hre/dom/node.hpp>
#include <hre/dom/element.hpp>
#include <hre/style/style_engine.hpp>

namespace hyperion::js {

// This class wraps a C++ DOM element for the JS engine
struct dom_element_wrapper : public object {
    hre::dom::element* element_ptr;
    
    dom_element_wrapper(hre::dom::element* el) : element_ptr(el) {}
};

void vm::init_dom_bridge(hre::dom::node* root) {
    auto document_obj = create_object();
    
    // document.getElementById implementation
    set_native_func(document_obj, "getElementById", [this, root](const std::vector<value>& args) {
        if (args.empty()) return value::null();
        std::string id = args[0].to_string();
        std::wstring id_w(id.begin(), id.end());
        
        // Find element in C++ DOM
        auto* found = root->find_by_id(id_w);
        if (found && found->type() == hre::dom::node_type::element) {
            auto* wrapper = m_gc.allocate<dom_element_wrapper>(static_cast<hre::dom::element*>(found));
            
            // Add style property to the element
            auto style_obj = create_object();
            set_native_func(style_obj, "setProperty", [wrapper](const std::vector<value>& s_args) {
                if (s_args.size() >= 2) {
                    std::string prop = s_args[0].to_string();
                    std::string val = s_args[1].to_string();
                    // Update HRE Style Engine directly
                    // hre::style::update_inline_style(wrapper->element_ptr, prop, val);
                }
                return value::undefined();
            });
            wrapper->properties["style"] = style_obj;
            
            return value::object_ptr(wrapper);
        }
        return value::null();
    });

    m_global_object.properties["document"] = document_obj;
}

} // namespace hyperion::js
