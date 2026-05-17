#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>

namespace hre::dom {

enum class node_type {
    element,
    text,
    document,
    document_fragment,
    comment
};

class node {
public:
    explicit node(node_type type) : m_type(type) {}
    virtual ~node() = default;

    node_type type() const { return m_type; }

    void append_child(std::unique_ptr<node> child) {
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }

    node* parent() const { return m_parent; }
    const std::vector<std::unique_ptr<node>>& children() const { return m_children; }

    // Take ownership of children (useful for fragments)
    std::vector<std::unique_ptr<node>> take_children() {
        auto old_children = std::move(m_children);
        m_children.clear();
        return old_children;
    }

private:
    node_type m_type;
    node* m_parent = nullptr;
    std::vector<std::unique_ptr<node>> m_children;
};

class text_node : public node {
public:
    explicit text_node(std::wstring text) 
        : node(node_type::text), m_text(std::move(text)) {}

    const std::wstring& text() const { return m_text; }
    void set_text(std::wstring text) { m_text = std::move(text); }

private:
    std::wstring m_text;
};

class element : public node {
public:
    explicit element(std::wstring tag_name) 
        : node(node_type::element), m_tag_name(std::move(tag_name)) {}

    const std::wstring& tag_name() const { return m_tag_name; }

    void set_id(std::wstring id) { m_id = std::move(id); }
    const std::wstring& id() const { return m_id; }

    void set_attribute(std::wstring name, std::wstring value) {
        if (name == L"class") {
            parse_class_list(value);
        }
        for (auto& attr : m_attributes) {
            if (attr.name == name) {
                attr.value = std::move(value);
                return;
            }
        }
        m_attributes.push_back({ std::move(name), std::move(value) });
    }

    std::wstring get_attribute(const std::wstring& name) const {
        for (const auto& attr : m_attributes) {
            if (attr.name == name) return attr.value;
        }
        return L"";
    }

    const std::vector<std::wstring>& class_list() const { return m_class_list; }

private:
    void parse_class_list(const std::wstring& class_str) {
        m_class_list.clear();
        size_t start = 0, end = 0;
        while ((end = class_str.find(L' ', start)) != std::wstring::npos) {
            if (end != start) {
                m_class_list.push_back(class_str.substr(start, end - start));
            }
            start = end + 1;
        }
        if (start < class_str.length()) {
            m_class_list.push_back(class_str.substr(start));
        }
    }

    struct attribute {
        std::wstring name;
        std::wstring value;
    };

    std::wstring m_tag_name;
    std::wstring m_id;
    std::vector<attribute> m_attributes;
    std::vector<std::wstring> m_class_list;
};

class document_fragment : public node {
public:
    document_fragment() : node(node_type::document_fragment) {}
};

class document : public node {
public:
    document() : node(node_type::document) {}

    void add_script(std::wstring source) { m_scripts.push_back(std::move(source)); }
    const std::vector<std::wstring>& scripts() const { return m_scripts; }

private:
    std::vector<std::wstring> m_scripts;
};

} // namespace hre::dom
