#define NOMINMAX
#include "hre/dom/node.hpp"
#include "hre/dom/shadow_dom.hpp"
#include "hre/dom/range.hpp"
#include <stack>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>

namespace hre::dom {

// ===== EventTarget =====

void EventTarget::add_event_listener(const std::string& type, EventCallback callback, bool capture, bool once, bool passive) {
    listeners_.push_back({type, callback, capture, once, passive, false});
}

void EventTarget::remove_event_listener(const std::string& type, EventCallback callback, bool capture) {
    for (auto& l : listeners_) {
        if (l.type == type && l.capture == capture) {
            l.removed = true;
        }
    }
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
        [](const EventListenerEntry& e) { return e.removed; }), listeners_.end());
}

bool EventTarget::dispatch_to_listeners(const std::shared_ptr<Event>& event, const std::shared_ptr<EventTarget>& self) {
    for (auto& l : listeners_) {
        if (l.removed) continue;
        if (l.type == event->type()) {
            if (l.once) l.removed = true;
            event->set_current_target(self);
            l.callback(event);
            if (event->immediate_stopped()) break;
        }
    }
    return !event->default_prevented();
}

bool EventTarget::dispatch_event(const std::shared_ptr<Event>& event, const std::shared_ptr<EventTarget>& self) {
    if (!event || !self) return false;

    event->set_target(self);

    // Capture phase: walk from root to target
    std::vector<std::shared_ptr<EventTarget>> capture_path;
    std::shared_ptr<EventTarget> current = self;
    while (auto parent = current->parent_event_target()) {
        capture_path.push_back(parent);
        current = parent;
    }
    std::reverse(capture_path.begin(), capture_path.end());

    // CAPTURING phase
    event->set_event_phase(Event::CAPTURING_PHASE);
    for (auto& ct : capture_path) {
        event->set_current_target(ct);
        for (auto& l : listeners_) {
            if (l.removed) continue;
            if (l.type == event->type() && l.capture) {
                if (l.once) l.removed = true;
                l.callback(event);
                if (event->immediate_stopped()) return !event->default_prevented();
            }
        }
        if (event->propagation_stopped()) break;
    }

    if (event->propagation_stopped()) return !event->default_prevented();

    // AT_TARGET phase
    event->set_event_phase(Event::AT_TARGET);
    dispatch_to_listeners(event, self);
    if (event->immediate_stopped()) return !event->default_prevented();

    // BUBBLING phase
    if (event->bubbles() && !event->propagation_stopped()) {
        event->set_event_phase(Event::BUBBLING_PHASE);
        std::shared_ptr<EventTarget> bubble_target = self;
        while (auto parent = bubble_target->parent_event_target()) {
            if (event->propagation_stopped()) break;
            bubble_target = parent;
            event->set_current_target(bubble_target);
            if (auto node_target = std::dynamic_pointer_cast<Node>(bubble_target)) {
                for (auto& l : node_target->listeners_) {
                    if (l.removed) continue;
                    if (l.type == event->type() && !l.capture) {
                        if (l.once) l.removed = true;
                        l.callback(event);
                        if (event->immediate_stopped()) break;
                    }
                }
            }
        }
    }

    return !event->default_prevented();
}

// ===== Node =====

std::shared_ptr<Element> Node::parent_element() const {
    auto p = parent_.lock();
    if (p && p->node_type() == NodeType::Element) {
        return std::static_pointer_cast<Element>(p);
    }
    return nullptr;
}

std::shared_ptr<Node> Node::previous_sibling() const {
    return previous_sibling_;
}

std::shared_ptr<Node> Node::next_sibling() const {
    return next_sibling_;
}

std::shared_ptr<Node> Node::first_child() const {
    return children_.empty() ? nullptr : children_.front();
}

std::shared_ptr<Node> Node::last_child() const {
    return children_.empty() ? nullptr : children_.back();
}

std::shared_ptr<Node> Node::append_child(const std::shared_ptr<Node>& child) {
    if (!child) return nullptr;
    if (child->parent_node()) {
        child->parent_node()->remove_child(child);
    }
    child->parent_ = weak_from_this();
    child->connected_ = connected_;

    if (!children_.empty()) {
        child->previous_sibling_ = children_.back();
        children_.back()->next_sibling_ = child;
    }
    child->next_sibling_.reset();
    children_.push_back(child);

    MutationNotifier::notify_child_added(shared_from_this(), child);
    return child;
}

std::shared_ptr<Node> Node::insert_before(const std::shared_ptr<Node>& new_node, const std::shared_ptr<Node>& reference) {
    if (!new_node) return nullptr;
    if (reference && reference->parent_node() != shared_from_this()) return nullptr;

    if (new_node->parent_node()) {
        new_node->parent_node()->remove_child(new_node);
    }

    new_node->parent_ = weak_from_this();
    new_node->connected_ = connected_;

    if (!reference) {
        return append_child(new_node);
    }

    auto it = std::find(children_.begin(), children_.end(), reference);
    if (it == children_.end()) return append_child(new_node);

    new_node->previous_sibling_ = reference->previous_sibling_;
    new_node->next_sibling_ = reference;
    if (auto prev = reference->previous_sibling_) {
        prev->next_sibling_ = new_node;
    }
    reference->previous_sibling_ = new_node;

    children_.insert(it, new_node);
    MutationNotifier::notify_child_added(shared_from_this(), new_node);
    return new_node;
}

std::shared_ptr<Node> Node::remove_child(const std::shared_ptr<Node>& child) {
    if (!child) return nullptr;
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it == children_.end()) return nullptr;

    if (auto prev = child->previous_sibling_) {
        prev->next_sibling_ = child->next_sibling_;
    }
    if (auto next = child->next_sibling_) {
        next->previous_sibling_ = child->previous_sibling_;
    }

    children_.erase(it);
    child->parent_.reset();
    child->connected_ = false;
    child->previous_sibling_.reset();
    child->next_sibling_.reset();

    MutationNotifier::notify_child_removed(shared_from_this(), child);
    return child;
}

std::shared_ptr<Node> Node::replace_child(const std::shared_ptr<Node>& new_child, const std::shared_ptr<Node>& old_child) {
    if (!new_child || !old_child) return nullptr;
    auto it = std::find(children_.begin(), children_.end(), old_child);
    if (it == children_.end()) return nullptr;

    insert_before(new_child, old_child);
    remove_child(old_child);
    return old_child;
}

std::shared_ptr<Node> Node::clone_node(bool deep) {
    auto clone = std::make_shared<Node>(node_type_);
    clone->node_name_ = node_name_;
    clone->node_value_ = node_value_;

    if (deep) {
        for (auto& child : children_) {
            clone->append_child(child->clone_node(true));
        }
    }
    return clone;
}

std::shared_ptr<Document> Node::owner_document() const {
    auto current = std::const_pointer_cast<Node>(shared_from_this());
    while (current) {
        if (current->node_type() == NodeType::Document) {
            return std::static_pointer_cast<Document>(current);
        }
        current = current->parent_.lock();
    }
    return nullptr;
}

std::wstring Node::text_content() const {
    if (node_type_ == NodeType::Text || node_type_ == NodeType::Comment) {
        return node_value_;
    }
    std::wstring result;
    for (auto& child : children_) {
        result += child->text_content();
    }
    return result;
}

void Node::set_text_content(const std::wstring& text) {
    // Remove all children
    while (!children_.empty()) {
        remove_child(children_.back());
    }

    if (!text.empty()) {
        auto text_node = std::make_shared<Text>(text);
        append_child(text_node);
    }
}

unsigned Node::child_element_count() const {
    unsigned count = 0;
    for (auto& child : children_) {
        if (child->node_type() == NodeType::Element) ++count;
    }
    return count;
}

bool Node::contains(const std::shared_ptr<Node>& other) const {
    if (!other) return false;
    if (other.get() == this) return true;
    auto current = other;
    while (current) {
        if (current.get() == this) return true;
        current = current->parent_.lock();
    }
    return false;
}

unsigned short Node::compare_document_position(const std::shared_ptr<Node>& other) const {
    if (!other) return 0;
    if (this == other.get()) return 0;

    // Build ancestor chains
    std::vector<std::shared_ptr<Node>> this_chain, other_chain;
    auto c = std::const_pointer_cast<Node>(shared_from_this());
    while (c) { this_chain.push_back(c); c = c->parent_.lock(); }
    c = std::const_pointer_cast<Node>(other);
    while (c) { other_chain.push_back(c); c = c->parent_.lock(); }

    // Find divergence point
    size_t i = 0;
    while (i < this_chain.size() && i < other_chain.size() &&
           this_chain[this_chain.size() - 1 - i] == other_chain[other_chain.size() - 1 - i]) {
        ++i;
    }

    if (i == this_chain.size() || i == other_chain.size()) {
        // One is ancestor of the other
        if (i == this_chain.size()) return 10; // DOCUMENT_POSITION_CONTAINED_BY | DOCUMENT_POSITION_FOLLOWING
        return 20; // DOCUMENT_POSITION_CONTAINS | DOCUMENT_POSITION_PRECEDING
    }

    auto this_ancestor = this_chain[this_chain.size() - i];
    auto other_ancestor = other_chain[other_chain.size() - i];
    auto parent = this_ancestor->parent_.lock();

    if (parent) {
        for (auto& sib : parent->children_) {
            if (sib == this_ancestor) return 4; // DOCUMENT_POSITION_FOLLOWING
            if (sib == other_ancestor) return 2; // DOCUMENT_POSITION_PRECEDING
        }
    }

    return 0;
}

void Node::normalize() {
    std::vector<std::shared_ptr<Node>> new_children;
    std::shared_ptr<Text> last_text;

    for (auto& child : children_) {
        if (child->node_type() == NodeType::Text) {
            auto text = std::static_pointer_cast<Text>(child);
            if (last_text) {
                last_text->append_data(text->data());
                continue;
            }
            if (text->data().empty()) continue;
            last_text = text;
        } else {
            last_text.reset();
            child->normalize();
        }
        new_children.push_back(child);
    }

    children_ = new_children;
}

std::vector<std::shared_ptr<EventTarget>> Node::child_event_targets() {
    std::vector<std::shared_ptr<EventTarget>> targets;
    for (auto& child : children_) {
        targets.push_back(child);
    }
    return targets;
}

// ===== Element =====

Element::Element(const std::wstring& tag_name)
    : Node(NodeType::Element), tag_name_(tag_name) {
    node_name_ = tag_name;
}

std::wstring Element::class_name() const {
    auto it = attributes_.find(L"class");
    return it != attributes_.end() ? it->second : L"";
}

std::vector<std::wstring> Element::class_list() const {
    std::vector<std::wstring> classes;
    std::wstring cls = class_name();
    if (cls.empty()) return classes;

    std::wistringstream iss(cls);
    std::wstring token;
    while (iss >> token) {
        if (!token.empty()) classes.push_back(token);
    }
    return classes;
}

std::wstring Element::get_attribute(const std::wstring& name) const {
    auto it = attributes_.find(name);
    return it != attributes_.end() ? it->second : L"";
}

void Element::set_attribute(const std::wstring& name, const std::wstring& value) {
    std::wstring old_val;
    auto it = attributes_.find(name);
    if (it != attributes_.end()) old_val = it->second;
    attributes_[name] = value;

    // Update id and class
    update_id_and_class();

    MutationNotifier::notify_attribute_changed(shared_from_this(), name, old_val, value);
}

bool Element::has_attribute(const std::wstring& name) const {
    return attributes_.find(name) != attributes_.end();
}

void Element::remove_attribute(const std::wstring& name) {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        std::wstring old_val = it->second;
        attributes_.erase(it);
        update_id_and_class();
        MutationNotifier::notify_attribute_changed(shared_from_this(), name, old_val, L"");
    }
}

std::vector<std::wstring> Element::get_attribute_names() const {
    std::vector<std::wstring> names;
    for (auto& [name, _] : attributes_) names.push_back(name);
    return names;
}

void Element::class_list_add(const std::wstring& token) {
    auto classes = class_list();
    if (std::find(classes.begin(), classes.end(), token) != classes.end()) return;
    classes.push_back(token);
    std::wstring result;
    for (auto& c : classes) {
        if (!result.empty()) result += L" ";
        result += c;
    }
    set_attribute(L"class", result);
}

void Element::class_list_remove(const std::wstring& token) {
    auto classes = class_list();
    classes.erase(std::remove(classes.begin(), classes.end(), token), classes.end());
    std::wstring result;
    for (auto& c : classes) {
        if (!result.empty()) result += L" ";
        result += c;
    }
    set_attribute(L"class", result);
}

bool Element::class_list_contains(const std::wstring& token) const {
    auto classes = class_list();
    return std::find(classes.begin(), classes.end(), token) != classes.end();
}

void Element::class_list_toggle(const std::wstring& token) {
    if (class_list_contains(token)) class_list_remove(token);
    else class_list_add(token);
}

void Element::update_id_and_class() {
    // Nothing specific needed here; stored in attributes
}

// ===== Event Dispatch Helpers =====
// Forward declarations for querySelector - included via simple matching

static bool match_simple_selector(const std::wstring& selector, const std::shared_ptr<Element>& el) {
    if (selector.empty()) return false;
    if (!el) return false;

    // Parse complex selectors (simplified)
    // Handle: tag, .class, #id, [attr], [attr=val], tag.class, tag#id

    // Split by combinators (for now, simple selectors only)
    std::wstring sel = selector;
    sel = std::regex_replace(sel, std::wregex(L"\\s*,\\s*"), L","); // normalize commas

    size_t pos = 0;
    while (pos < sel.size()) {
        // Parse one simple selector chain
        std::wstring chain;
        size_t end = sel.find_first_of(L", \t>+~", pos);
        if (end == std::wstring::npos) {
            chain = sel.substr(pos);
            pos = sel.size();
        } else {
            chain = sel.substr(pos, end - pos);
            if (sel[end] == L',') ++end;
            pos = end;
        }

        if (chain.empty()) continue;

        // Now match chain against element
        size_t ci = 0;
        bool matches = true;

        // Check for tag name
        std::wstring tag_name;
        if (chain[0] != L'.' && chain[0] != L'#' && chain[0] != L'[' && chain[0] != L':') {
            while (ci < chain.size() && chain[ci] != L'.' && chain[ci] != L'#' && chain[ci] != L'[' && chain[ci] != L':') {
                tag_name += chain[ci++];
            }
        }

        if (!tag_name.empty()) {
            if (tag_name != L"*" && el->tag_name() != tag_name) matches = false;
        }

        while (ci < chain.size() && matches) {
            if (chain[ci] == L'.') {
                ++ci;
                std::wstring cls;
                while (ci < chain.size() && chain[ci] != L'.' && chain[ci] != L'#' && chain[ci] != L'[' && chain[ci] != L':') {
                    cls += chain[ci++];
                }
                if (!cls.empty() && !el->class_list_contains(cls)) matches = false;
            } else if (chain[ci] == L'#') {
                ++ci;
                std::wstring id_val;
                while (ci < chain.size() && chain[ci] != L'.' && chain[ci] != L'#' && chain[ci] != L'[' && chain[ci] != L':') {
                    id_val += chain[ci++];
                }
                if (el->get_attribute(L"id") != id_val) matches = false;
            } else if (chain[ci] == L'[') {
                ++ci;
                std::wstring attr, val;
                bool has_val = false;
                bool equals = false;

                while (ci < chain.size() && chain[ci] != L']' && chain[ci] != L'=' && chain[ci] != L'~' && chain[ci] != L'|' && chain[ci] != L'^' && chain[ci] != L'$' && chain[ci] != L'*') {
                    if (chain[ci] != L'"' && chain[ci] != L'\'') attr += chain[ci];
                    ++ci;
                }

                if (ci < chain.size() && (chain[ci] == L'=' || chain[ci] == L'~' || chain[ci] == L'|' || chain[ci] == L'^' || chain[ci] == L'$' || chain[ci] == L'*')) {
                    has_val = true;
                    if (chain[ci] == L'~') equals = true; // ~=
                    ++ci;
                    if (ci < chain.size() && chain[ci] == L'=') ++ci; // skip =
                    while (ci < chain.size() && chain[ci] != L']' && ci < chain.size()) {
                        if (chain[ci] != L'"' && chain[ci] != L'\'') val += chain[ci];
                        ++ci;
                    }
                }
                if (ci < chain.size() && chain[ci] == L']') ++ci;

                // Match attribute
                if (!has_val) {
                    if (!el->has_attribute(attr)) matches = false;
                } else {
                    auto actual = el->get_attribute(attr);
                    if (equals) {
                        // ~= : contains word
                        auto words = el->class_list();
                        if (std::find(words.begin(), words.end(), val) == words.end()) matches = false;
                    } else {
                        if (actual != val) matches = false;
                    }
                }
            } else if (chain[ci] == L':') {
                // Pseudo-classes - simplified
                ++ci;
                std::wstring pseudo;
                while (ci < chain.size() && chain[ci] != L'.' && chain[ci] != L'#' && chain[ci] != L'[' && chain[ci] != L':') {
                    if (chain[ci] != L'(' && chain[ci] != L')') pseudo += chain[ci];
                    ++ci;
                }
                if (pseudo == L"root") {
                    matches = !el->parent_node() || el->parent_node()->node_type() == NodeType::Document;
                } else if (pseudo == L"first-child") {
                    auto parent = el->parent_node();
                    matches = parent && !parent->child_nodes().empty() && parent->child_nodes().front() == el;
                } else if (pseudo == L"last-child") {
                    auto parent = el->parent_node();
                    matches = parent && !parent->child_nodes().empty() && parent->child_nodes().back() == el;
                } else if (pseudo == L"only-child") {
                    auto parent = el->parent_node();
                    matches = parent && parent->child_element_count() == 1;
                } else if (pseudo == L"empty") {
                    matches = el->child_nodes().empty() && el->text_content().empty();
                }
            } else {
                ++ci;
            }
        }

        if (matches) return true;
    }

    return false;
}

static void find_matches(const std::shared_ptr<Element>& root, const std::wstring& selector,
                          std::vector<std::shared_ptr<Element>>& results, bool descend_into_shadow = true) {
    if (!root) return;

    if (match_simple_selector(selector, root)) {
        results.push_back(root);
    }

    for (auto& child : root->child_nodes()) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            find_matches(el, selector, results, descend_into_shadow);
        }
    }

    // Search into shadow DOM
    if (descend_into_shadow && root->shadow_root_) {
        for (auto& child : root->shadow_root_->child_nodes()) {
            if (auto el = std::dynamic_pointer_cast<Element>(child)) {
                find_matches(el, selector, results, false);
            }
        }
    }
}

std::shared_ptr<Element> Element::query_selector(const std::wstring& selectors) {
    std::vector<std::shared_ptr<Element>> results;
    find_matches(std::static_pointer_cast<Element>(shared_from_this()), selectors, results);
    return results.empty() ? nullptr : results.front();
}

std::vector<std::shared_ptr<Element>> Element::query_selector_all(const std::wstring& selectors) {
    std::vector<std::shared_ptr<Element>> results;
    find_matches(std::static_pointer_cast<Element>(shared_from_this()), selectors, results);
    return results;
}

std::vector<std::shared_ptr<Element>> Element::get_elements_by_tag_name(const std::wstring& tag_name) {
    return query_selector_all(tag_name);
}

std::vector<std::shared_ptr<Element>> Element::get_elements_by_class_name(const std::wstring& class_name) {
    // Replace spaces with dots for class selector
    std::wstring selector;
    std::wistringstream iss(class_name);
    std::wstring token;
    while (iss >> token) {
        if (!selector.empty()) selector += L"";
        selector += L"." + token;
    }
    return query_selector_all(selector);
}

bool Element::matches(const std::wstring& selectors) const {
    return match_simple_selector(selectors, std::static_pointer_cast<Element>(std::const_pointer_cast<Node>(shared_from_this())));
}

std::shared_ptr<Element> Element::closest(const std::wstring& selectors) const {
    auto current = std::dynamic_pointer_cast<Element>(std::const_pointer_cast<Node>(shared_from_this()));
    while (current) {
        if (current->matches(selectors)) return current;
        current = std::dynamic_pointer_cast<Element>(current->parent_node());
    }
    return nullptr;
}

// ===== Inner HTML (simplified parsing) =====

std::wstring Element::inner_html() const {
    std::wstring result;
    for (auto& child : children_) {
        result += child->text_content();
    }
    return result;
}

void Element::set_inner_html(const std::wstring& html) {
    // Remove all children
    while (!children_.empty()) {
        remove_child(children_.back());
    }

    // Simplified: create a text node for now
    // A full parser would handle tags properly
    auto parsed = parse_html(html);
    for (auto& node : parsed) {
        append_child(node);
    }
}

std::wstring Element::outer_html() const {
    std::wstring result = L"<" + tag_name_;
    for (auto& [name, val] : attributes_) {
        result += L" " + name + L"=\"" + val + L"\"";
    }
    result += L">";
    result += inner_html();
    result += L"</" + tag_name_ + L">";
    return result;
}

void Element::set_outer_html(const std::wstring& html) {
    auto parent = parent_node();
    if (!parent) return;
    auto parsed = parse_html(html);
    for (auto& node : parsed) {
        parent->insert_before(node, shared_from_this());
    }
    parent->remove_child(shared_from_this());
}

void Element::insert_adjacent_html(const std::wstring& position, const std::wstring& html) {
    auto parsed = parse_html(html);
    if (position == L"beforebegin") {
        auto parent = parent_node();
        if (parent) {
            for (auto& node : parsed) parent->insert_before(node, shared_from_this());
        }
    } else if (position == L"afterbegin") {
        for (auto& node : parsed) insert_before(node, first_child());
    } else if (position == L"beforeend") {
        for (auto& node : parsed) append_child(node);
    } else if (position == L"afterend") {
        auto parent = parent_node();
        if (parent) {
            auto next = next_sibling();
            for (auto& node : parsed) parent->insert_before(node, next);
        }
    }
}

void Element::insert_adjacent_element(const std::wstring& position, const std::shared_ptr<Element>& element) {
    if (position == L"beforebegin") {
        auto parent = parent_node();
        if (parent) parent->insert_before(element, shared_from_this());
    } else if (position == L"afterbegin") {
        insert_before(element, first_child());
    } else if (position == L"beforeend") {
        append_child(element);
    } else if (position == L"afterend") {
        auto parent = parent_node();
        if (parent) {
            auto next = next_sibling();
            parent->insert_before(element, next);
        }
    }
}

void Element::insert_adjacent_text(const std::wstring& position, const std::wstring& text) {
    auto text_node = std::make_shared<Text>(text);
    insert_adjacent_element(position, std::make_shared<Element>(L""));
}

// ===== HTML Parser (minimal) =====

std::vector<std::shared_ptr<Node>> Element::parse_html(const std::wstring& html) {
    std::vector<std::shared_ptr<Node>> result;
    if (html.empty()) return result;

    // Check if it contains HTML tags
    if (html.find(L'<') == std::wstring::npos && html.find(L'&') == std::wstring::npos) {
        result.push_back(std::make_shared<Text>(html));
        return result;
    }

    // Very basic tag parser
    size_t i = 0;
    std::stack<std::shared_ptr<Element>> open_elements;
    std::shared_ptr<Element> current;

    while (i < html.size()) {
        if (html[i] == L'<') {
            if (i + 1 < html.size() && html[i + 1] == L'/') {
                // Closing tag
                size_t close = html.find(L'>', i);
                if (close == std::wstring::npos) break;
                if (!open_elements.empty()) {
                    open_elements.pop();
                    current = open_elements.empty() ? nullptr : open_elements.top();
                }
                i = close + 1;
            } else {
                // Opening tag
                size_t close = html.find(L'>', i);
                if (close == std::wstring::npos) break;

                std::wstring tag_content = html.substr(i + 1, close - i - 1);
                bool self_closing = tag_content.back() == L'/';

                // Parse tag name
                size_t name_end = tag_content.find_first_of(L" \t\n>");
                std::wstring tag_name = tag_content.substr(0, name_end);
                if (self_closing) tag_name.pop_back();

                // Parse attributes
                auto el = std::make_shared<Element>(tag_name);
                if (name_end != std::wstring::npos && !self_closing) {
                    std::wstring attrs_part = tag_content.substr(name_end);
                    // Simple attribute parsing
                    std::wregex attr_re(L"([\\w-]+)\\s*=\\s*\"([^\"]*)\"");
                    std::wsmatch match;
                    std::wstring::const_iterator search_start(attrs_part.cbegin());
                    while (std::regex_search(search_start, attrs_part.cend(), match, attr_re)) {
                        el->set_attribute(match[1].str(), match[2].str());
                        search_start = match.suffix().first;
                    }
                }

                if (!current) {
                    current = el;
                } else {
                    current->append_child(el);
                }

                if (!self_closing) {
                    open_elements.push(el);
                    current = el;
                }

                i = close + 1;
            }
        } else {
            // Text content
            size_t next_tag = html.find(L'<', i);
            std::wstring text = (next_tag == std::wstring::npos) ? html.substr(i) : html.substr(i, next_tag - i);
            if (!text.empty()) {
                auto text_node = std::make_shared<Text>(text);
                if (current) {
                    current->append_child(text_node);
                } else {
                    result.push_back(text_node);
                }
            }
            i = (next_tag == std::wstring::npos) ? html.size() : next_tag;
        }
    }

    if (current && result.empty()) {
        result.push_back(current);
    }

    return result;
}

// ===== Geometry =====

double Element::client_width() const {
    return content_box.width;
}
double Element::client_height() const {
    return content_box.height;
}
double Element::scroll_width() const {
    double sw = content_box.width;
    for (auto& child : children_) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            sw = std::max(sw, el->content_box.x + el->scroll_width());
        }
    }
    return sw;
}
double Element::scroll_height() const {
    double sh = content_box.height;
    for (auto& child : children_) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            sh = std::max(sh, el->content_box.y + el->scroll_height());
        }
    }
    return sh;
}

void Element::scroll_into_view(bool align_to_top) {
    // Simplified: scroll parent to make this element visible
    auto parent = parent_element();
    if (parent) {
        parent->set_scroll_top(content_box.y - (align_to_top ? 0 : parent->client_height() + content_box.height));
        parent->set_scroll_left(content_box.x);
    }
}

void Element::scroll(double x, double y) {
    // Simplified scroll
}

void Element::scroll_by(double x, double y) {
    // Simplified scroll
}

double Element::scroll_left() const { return scroll_left_; }
double Element::scroll_top() const { return scroll_top_; }
void Element::set_scroll_left(double val) { scroll_left_ = val; }
void Element::set_scroll_top(double val) { scroll_top_ = val; }

Element::DOMRect Element::get_bounding_client_rect() const {
    return {content_box.x, content_box.y, content_box.width, content_box.height};
}

std::vector<Element::DOMRect> Element::get_client_rects() const {
    return {{content_box.x, content_box.y, content_box.width, content_box.height}};
}

// ===== Shadow DOM =====

std::shared_ptr<ShadowRoot> Element::attach_shadow(ShadowRootMode mode) {
    if (shadow_root_) return shadow_root_;
    shadow_root_ = std::make_shared<ShadowRoot>(mode);
    shadow_root_->set_host(std::static_pointer_cast<Element>(shared_from_this()));
    return shadow_root_;
}

// ===== Focus =====

void Element::focus() {
    focused_ = true;
    auto evt = std::make_shared<Event>("focus", false, false);
    EventTarget::dispatch_event(evt, shared_from_this());
}

void Element::blur() {
    if (!focused_) return;
    focused_ = false;
    auto evt = std::make_shared<Event>("blur", false, false);
    EventTarget::dispatch_event(evt, shared_from_this());
}

// ===== Form helpers =====

std::wstring Element::value() const {
    if (tag_name_ == L"input" || tag_name_ == L"textarea") {
        return get_attribute(L"value");
    }
    if (tag_name_ == L"select") {
        for (auto& child : children_) {
            if (auto el = std::dynamic_pointer_cast<Element>(child)) {
                if (el->tag_name() == L"option" && el->has_attribute(L"selected")) {
                    return el->get_attribute(L"value");
                }
                // First option by default
                if (el->tag_name() == L"option") {
                    return el->get_attribute(L"value");
                }
            }
        }
    }
    return L"";
}

void Element::set_value(const std::wstring& val) {
    set_attribute(L"value", val);
}

bool Element::check_validity() const {
    return true; // Simplified: no validation errors
}

// ===== Dataset =====

std::wstring Element::dataset(const std::wstring& key) const {
    return get_attribute(L"data-" + key);
}

void Element::set_dataset(const std::wstring& key, const std::wstring& value) {
    set_attribute(L"data-" + key, value);
}

// ===== Document =====

std::shared_ptr<Element> Document::create_element(const std::wstring& tag_name) {
    return std::make_shared<Element>(tag_name);
}

std::shared_ptr<Text> Document::create_text_node(const std::wstring& data) {
    return std::make_shared<Text>(data);
}

std::shared_ptr<Comment> Document::create_comment(const std::wstring& data) {
    return std::make_shared<Comment>(data);
}

std::shared_ptr<DocumentFragment> Document::create_document_fragment() {
    return std::make_shared<DocumentFragment>();
}

std::shared_ptr<Element> Document::document_element() const {
    for (auto& child : children_) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            if (el->tag_name() == L"html") return el;
        }
    }
    return nullptr;
}

std::shared_ptr<Element> Document::get_element_by_id(const std::wstring& id) {
    auto root = document_element();
    return root ? root->query_selector(L"#" + id) : nullptr;
}

std::vector<std::shared_ptr<Element>> Document::get_elements_by_tag_name(const std::wstring& tag_name) {
    auto root = document_element();
    return root ? root->query_selector_all(tag_name) : std::vector<std::shared_ptr<Element>>();
}

std::vector<std::shared_ptr<Element>> Document::get_elements_by_class_name(const std::wstring& class_name) {
    auto root = document_element();
    return root ? root->get_elements_by_class_name(class_name) : std::vector<std::shared_ptr<Element>>();
}

std::shared_ptr<Element> Document::query_selector(const std::wstring& selectors) {
    auto root = document_element();
    return root ? root->query_selector(selectors) : nullptr;
}

std::vector<std::shared_ptr<Element>> Document::query_selector_all(const std::wstring& selectors) {
    auto root = document_element();
    return root ? root->query_selector_all(selectors) : std::vector<std::shared_ptr<Element>>();
}

std::shared_ptr<Element> Document::body() const {
    auto html = document_element();
    if (!html) return nullptr;
    for (auto& child : html->child_nodes()) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            if (el->tag_name() == L"body") return el;
        }
    }
    return nullptr;
}

std::shared_ptr<Element> Document::head() const {
    auto html = document_element();
    if (!html) return nullptr;
    for (auto& child : html->child_nodes()) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            if (el->tag_name() == L"head") return el;
        }
    }
    return nullptr;
}

// ===== Text =====

std::wstring Text::substring_data(unsigned offset, unsigned count) const {
    if (offset >= node_value_.size()) return L"";
    return node_value_.substr(offset, count);
}

void Text::append_data(const std::wstring& data) {
    std::wstring old = node_value_;
    node_value_ += data;
    MutationNotifier::notify_character_data_changed(shared_from_this(), old, node_value_);
}

void Text::insert_data(unsigned offset, const std::wstring& data) {
    std::wstring old = node_value_;
    node_value_.insert(offset, data);
    MutationNotifier::notify_character_data_changed(shared_from_this(), old, node_value_);
}

void Text::delete_data(unsigned offset, unsigned count) {
    std::wstring old = node_value_;
    if (offset < node_value_.size()) {
        node_value_.erase(offset, std::min(count, static_cast<unsigned>(node_value_.size() - offset)));
    }
    MutationNotifier::notify_character_data_changed(shared_from_this(), old, node_value_);
}

void Text::replace_data(unsigned offset, unsigned count, const std::wstring& data) {
    std::wstring old = node_value_;
    if (offset < node_value_.size()) {
        node_value_.replace(offset, std::min(count, static_cast<unsigned>(node_value_.size() - offset)), data);
    }
    MutationNotifier::notify_character_data_changed(shared_from_this(), old, node_value_);
}

std::shared_ptr<Text> Text::split_text(unsigned offset) {
    if (offset >= node_value_.size()) return nullptr;
    std::wstring new_data = node_value_.substr(offset);
    node_value_ = node_value_.substr(0, offset);

    auto new_text = std::make_shared<Text>(new_data);
    auto parent = parent_node();
    if (parent) {
        parent->insert_before(new_text, next_sibling());
    }
    return new_text;
}

// ===== MutationObserver =====

std::vector<std::weak_ptr<MutationObserver>> MutationNotifier::observers_;

MutationObserver::MutationObserver(Callback callback) : callback_(callback) {}

void MutationObserver::observe(const std::shared_ptr<Node>& target, const std::unordered_map<std::string, bool>& options) {
    ObserveTarget ot;
    ot.target = target;
    auto it = options.find("childList");
    if (it != options.end()) ot.child_list = it->second;
    it = options.find("attributes");
    if (it != options.end()) ot.attributes = it->second;
    it = options.find("characterData");
    if (it != options.end()) ot.character_data = it->second;
    it = options.find("subtree");
    if (it != options.end()) ot.subtree = it->second;
    it = options.find("attributeOldValue");
    if (it != options.end()) ot.attribute_old_value = it->second;
    it = options.find("characterDataOldValue");
    if (it != options.end()) ot.character_data_old_value = it->second;

    observe_targets_.push_back(ot);
    MutationNotifier::register_observer(shared_from_this());
}

void MutationObserver::disconnect() {
    observe_targets_.clear();
    records_.clear();
    MutationNotifier::unregister_observer(shared_from_this());
}

std::vector<MutationRecord> MutationObserver::take_records() {
    auto result = std::move(records_);
    records_.clear();
    return result;
}

void MutationObserver::queue_record(const MutationRecord& record) {
    records_.push_back(record);
}

void MutationObserver::deliver_records() {
    if (delivering_ || records_.empty()) return;
    delivering_ = true;
    auto records = records_;
    records_.clear();
    callback_(records, *this);
    delivering_ = false;
}

void MutationNotifier::register_observer(const std::shared_ptr<MutationObserver>& observer) {
    observers_.push_back(observer);
}

void MutationNotifier::unregister_observer(const std::shared_ptr<MutationObserver>& observer) {
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
        [&](const std::weak_ptr<MutationObserver>& wp) {
            return wp.expired() || wp.lock() == observer;
        }), observers_.end());
}

void MutationNotifier::deliver_all() {
    for (auto& wp : observers_) {
        if (auto obs = wp.lock()) {
            obs->deliver_records();
        }
    }
}

void MutationNotifier::notify_child_added(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child) {
    MutationRecord record;
    record.type = "childList";
    record.target = parent;
    record.added_nodes.push_back(child);
    record.previous_sibling = child->previous_sibling();
    record.next_sibling = child->next_sibling();

    for (auto& wp : observers_) {
        if (auto obs = wp.lock()) {
            for (auto& ot : obs->observe_targets_) {
                auto target = ot.target.lock();
                if (!target) continue;
                if (target == parent || (ot.subtree && target->contains(parent))) {
                    if (ot.child_list) {
                        obs->queue_record(record);
                    }
                }
            }
        }
    }
}

void MutationNotifier::notify_child_removed(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child) {
    MutationRecord record;
    record.type = "childList";
    record.target = parent;
    record.removed_nodes.push_back(child);

    for (auto& wp : observers_) {
        if (auto obs = wp.lock()) {
            for (auto& ot : obs->observe_targets_) {
                auto target = ot.target.lock();
                if (!target) continue;
                if (target == parent || (ot.subtree && target->contains(parent))) {
                    if (ot.child_list) {
                        obs->queue_record(record);
                    }
                }
            }
        }
    }
}

void MutationNotifier::notify_attribute_changed(const std::shared_ptr<Node>& target, const std::wstring& name,
                                                  const std::wstring& old_value, const std::wstring& new_value) {
    MutationRecord record;
    record.type = "attributes";
    record.target = target;
    record.attribute_name = name;
    record.old_value = old_value;
    record.new_value = new_value;

    for (auto& wp : observers_) {
        if (auto obs = wp.lock()) {
            for (auto& ot : obs->observe_targets_) {
                auto t = ot.target.lock();
                if (!t) continue;
                if (t == target || (ot.subtree && t->contains(target))) {
                    if (ot.attributes) {
                        if (!ot.attribute_filter.empty() &&
                            std::find(ot.attribute_filter.begin(), ot.attribute_filter.end(), name) == ot.attribute_filter.end()) {
                            continue;
                        }
                        if (!ot.attribute_old_value) record.old_value = L"";
                        obs->queue_record(record);
                    }
                }
            }
        }
    }
}

void MutationNotifier::notify_character_data_changed(const std::shared_ptr<Node>& target,
                                                       const std::wstring& old_value, const std::wstring& new_value) {
    MutationRecord record;
    record.type = "characterData";
    record.target = target;
    record.old_value = old_value;
    record.new_value = new_value;

    for (auto& wp : observers_) {
        if (auto obs = wp.lock()) {
            for (auto& ot : obs->observe_targets_) {
                auto t = ot.target.lock();
                if (!t) continue;
                if (t == target || (ot.subtree && t->contains(target))) {
                    if (ot.character_data) {
                        if (!ot.character_data_old_value) record.old_value = L"";
                        obs->queue_record(record);
                    }
                }
            }
        }
    }
}

// ===== Range =====
Range::Range() : collapsed_(true) {}

void Range::set_start(const std::shared_ptr<Node>& node, unsigned offset) {
    start_container_ = node;
    start_offset_ = offset;
    collapsed_ = (start_container_ == end_container_ && start_offset_ == end_offset_);
}

void Range::set_end(const std::shared_ptr<Node>& node, unsigned offset) {
    end_container_ = node;
    end_offset_ = offset;
    collapsed_ = (start_container_ == end_container_ && start_offset_ == end_offset_);
}

void Range::set_start_before(const std::shared_ptr<Node>& node) {
    if (auto parent = node->parent_node()) {
        set_start(parent, index_of(parent, node));
    }
}

void Range::set_start_after(const std::shared_ptr<Node>& node) {
    if (auto parent = node->parent_node()) {
        set_start(parent, index_of(parent, node) + 1);
    }
}

void Range::set_end_before(const std::shared_ptr<Node>& node) {
    if (auto parent = node->parent_node()) {
        set_end(parent, index_of(parent, node));
    }
}

void Range::set_end_after(const std::shared_ptr<Node>& node) {
    if (auto parent = node->parent_node()) {
        set_end(parent, index_of(parent, node) + 1);
    }
}

void Range::collapse(bool to_start) {
    if (to_start || collapsed_) {
        end_container_ = start_container_;
        end_offset_ = start_offset_;
    } else {
        start_container_ = end_container_;
        start_offset_ = end_offset_;
    }
    collapsed_ = true;
}

bool Range::is_collapsed() const { return collapsed_; }

std::shared_ptr<Node> Range::common_ancestor_container() const {
    return find_common_ancestor(start_container_, end_container_);
}

std::shared_ptr<Node> Range::find_common_ancestor(std::shared_ptr<Node> a, std::shared_ptr<Node> b) const {
    if (!a || !b) return nullptr;
    if (a == b) return a;

    std::vector<std::shared_ptr<Node>> a_ancestors;
    auto current = a;
    while (current) {
        a_ancestors.push_back(current);
        current = current->parent_node();
    }

    current = b;
    while (current) {
        if (std::find(a_ancestors.begin(), a_ancestors.end(), current) != a_ancestors.end()) {
            return current;
        }
        current = current->parent_node();
    }
    return nullptr;
}

unsigned Range::index_of(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child) const {
    unsigned idx = 0;
    for (auto& c : parent->child_nodes()) {
        if (c == child) return idx;
        ++idx;
    }
    return 0;
}

std::shared_ptr<DocumentFragment> Range::extract_contents() {
    auto frag = std::make_shared<DocumentFragment>();
    // Simplified: clone and delete
    clone_contents();
    delete_contents();
    return frag;
}

std::shared_ptr<DocumentFragment> Range::clone_contents() {
    auto frag = std::make_shared<DocumentFragment>();
    auto common = common_ancestor_container();
    if (!common) return frag;

    // Simplified: clone all children of common ancestor
    for (auto& child : common->child_nodes()) {
        frag->append_child(child->clone_node(true));
    }
    return frag;
}

void Range::delete_contents() {
    auto common = common_ancestor_container();
    if (!common) return;
    while (!common->child_nodes().empty()) {
        common->remove_child(common->child_nodes().back());
    }
}

void Range::insert_node(const std::shared_ptr<Node>& node) {
    if (!node || !end_container_) return;
    // Simplified: append to end container
    end_container_->append_child(node);
}

void Range::surround_contents(const std::shared_ptr<Node>& new_parent) {
    if (!new_parent) return;
    auto common = common_ancestor_container();
    if (!common) return;

    while (!common->child_nodes().empty()) {
        new_parent->append_child(common->child_nodes().front());
    }
    common->append_child(new_parent);
}

std::wstring Range::to_string() const {
    if (!start_container_ || !end_container_) return L"";
    // Simplified: return text content between start and end
    auto common = common_ancestor_container();
    return common ? common->text_content() : L"";
}

// ===== ShadowRoot =====
ShadowRoot::ShadowRoot(ShadowRootMode mode)
    : DocumentFragment(), mode_(mode) {}

std::shared_ptr<Element> ShadowRoot::get_element_by_id(const std::wstring& id) {
    return query_selector_all(L"#" + id).empty() ? nullptr : query_selector_all(L"#" + id).front();
}

std::vector<std::shared_ptr<Element>> ShadowRoot::query_selector_all(const std::wstring& selector) {
    std::vector<std::shared_ptr<Element>> results;
    for (auto& child : children_) {
        if (auto el = std::dynamic_pointer_cast<Element>(child)) {
            find_matches(el, selector, results, false);
        }
    }
    return results;
}

void ShadowRoot::assign_slotted(const std::shared_ptr<Element>& slot, const std::vector<std::shared_ptr<Node>>& nodes) {
    slot_assignments_[slot] = nodes;
}

std::vector<std::shared_ptr<Node>> ShadowRoot::get_assigned_nodes(const std::shared_ptr<Element>& slot) {
    auto it = slot_assignments_.find(slot);
    if (it != slot_assignments_.end()) return it->second;
    return {};
}

std::vector<std::shared_ptr<Element>> ShadowRoot::get_assigned_slots(const std::shared_ptr<Element>& node) {
    std::vector<std::shared_ptr<Element>> slots;
    for (auto& [slot, nodes] : slot_assignments_) {
        for (auto& n : nodes) {
            if (n == node) {
                slots.push_back(slot);
            }
        }
    }
    return slots;
}

} // namespace hre::dom
