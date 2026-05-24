#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <variant>
#include <typeindex>

namespace hre::dom {

enum class ShadowRootMode { Open, Closed };

class Node;
class Element;
class Document;
class DocumentFragment;
class Text;
class Comment;
class Event;
class EventTarget;
class ShadowRoot;
class Attr;
class EventListener;

enum class NodeType : int {
    Element = 1,
    Attribute = 2,
    Text = 3,
    CDataSection = 4,
    EntityReference = 5,
    Entity = 6,
    ProcessingInstruction = 7,
    Comment = 8,
    Document = 9,
    DocumentType = 10,
    DocumentFragment = 11,
    Notation = 12
};

using EventCallback = std::function<void(const std::shared_ptr<Event>&)>;

struct EventListenerEntry {
    std::string type;
    EventCallback callback;
    bool capture = false;
    bool once = false;
    bool passive = false;
    bool removed = false;
};

class EventTarget {
public:
    EventTarget() = default;
    virtual ~EventTarget() = default;

    void add_event_listener(const std::string& type, EventCallback callback, bool capture = false, bool once = false, bool passive = false);
    void remove_event_listener(const std::string& type, EventCallback callback, bool capture = false);
    bool dispatch_event(const std::shared_ptr<Event>& event, const std::shared_ptr<EventTarget>& self);

    virtual std::shared_ptr<EventTarget> parent_event_target() { return nullptr; }
    virtual std::vector<std::shared_ptr<EventTarget>> child_event_targets() { return {}; }

protected:
    std::vector<EventListenerEntry> listeners_;
    bool dispatch_to_listeners(const std::shared_ptr<Event>& event, const std::shared_ptr<EventTarget>& self);
};

class Node : public EventTarget, public std::enable_shared_from_this<Node> {
public:
    Node(NodeType type) : node_type_(type) {}
    virtual ~Node() = default;

    // Properties
    NodeType node_type() const { return node_type_; }
    std::wstring node_name() const { return node_name_; }
    std::wstring node_value() const { return node_value_; }
    void set_node_value(const std::wstring& val) { node_value_ = val; }

    std::shared_ptr<Node> parent_node() const { return parent_.lock(); }
    std::shared_ptr<Element> parent_element() const;
    std::shared_ptr<Node> previous_sibling() const;
    std::shared_ptr<Node> next_sibling() const;

    const std::vector<std::shared_ptr<Node>>& child_nodes() const { return children_; }
    std::shared_ptr<Node> first_child() const;
    std::shared_ptr<Node> last_child() const;

    // Tree mutation
    virtual std::shared_ptr<Node> append_child(const std::shared_ptr<Node>& child);
    virtual std::shared_ptr<Node> insert_before(const std::shared_ptr<Node>& new_node, const std::shared_ptr<Node>& reference);
    virtual std::shared_ptr<Node> remove_child(const std::shared_ptr<Node>& child);
    virtual std::shared_ptr<Node> replace_child(const std::shared_ptr<Node>& new_child, const std::shared_ptr<Node>& old_child);
    std::shared_ptr<Node> clone_node(bool deep = false);

    // Document relationships
    std::shared_ptr<Document> owner_document() const;
    bool is_connected() const { return connected_; }

    // Text content
    std::wstring text_content() const;
    void set_text_content(const std::wstring& text);

    // Traversal
    bool has_child_nodes() const { return !children_.empty(); }
    unsigned child_element_count() const;
    bool contains(const std::shared_ptr<Node>& other) const;

    // Comparison
    unsigned short compare_document_position(const std::shared_ptr<Node>& other) const;
    bool is_equal_node(const std::shared_ptr<Node>& other) const;
    bool is_same_node(const std::shared_ptr<Node>& other) const { return this == other.get(); }

    // Normalize
    void normalize();

    // Event target override
    std::shared_ptr<EventTarget> parent_event_target() override {
        return std::static_pointer_cast<EventTarget>(parent_.lock());
    }

    std::vector<std::shared_ptr<EventTarget>> child_event_targets() override;

    // Internal
    std::weak_ptr<Node> parent_;
    std::vector<std::shared_ptr<Node>> children_;
    NodeType node_type_;
    std::wstring node_name_;
    std::wstring node_value_;
    bool connected_ = false;

    // Shadow DOM
    std::shared_ptr<ShadowRoot> shadow_root_;
    std::shared_ptr<Element> assigned_slot_;
    std::wstring slot_;

    std::shared_ptr<Node> previous_sibling_;
    std::shared_ptr<Node> next_sibling_;
};

class DocumentFragment : public Node {
public:
    DocumentFragment() : Node(NodeType::DocumentFragment) {
        node_name_ = L"#document-fragment";
    }
};

class Document : public Node {
public:
    Document() : Node(NodeType::Document) {
        node_name_ = L"#document";
    }

    std::shared_ptr<Element> create_element(const std::wstring& tag_name);
    std::shared_ptr<Text> create_text_node(const std::wstring& data);
    std::shared_ptr<Comment> create_comment(const std::wstring& data);
    std::shared_ptr<DocumentFragment> create_document_fragment();

    std::shared_ptr<Element> document_element() const;
    std::shared_ptr<Element> get_element_by_id(const std::wstring& id);
    std::vector<std::shared_ptr<Element>> get_elements_by_tag_name(const std::wstring& tag_name);
    std::vector<std::shared_ptr<Element>> get_elements_by_class_name(const std::wstring& class_name);
    std::shared_ptr<Element> query_selector(const std::wstring& selectors);
    std::vector<std::shared_ptr<Element>> query_selector_all(const std::wstring& selectors);

    // Body/head
    std::shared_ptr<Element> body() const;
    std::shared_ptr<Element> head() const;

    // Event target
    std::shared_ptr<EventTarget> parent_event_target() override { return nullptr; }
};

class Attr {
public:
    std::wstring name;
    std::wstring value;
    std::shared_ptr<Element> owner_element;
};

class Element : public Node {
public:
    Element(const std::wstring& tag_name);

    std::wstring tag_name() const { return tag_name_; }
    std::wstring local_name() const { return tag_name_; }
    std::wstring id() const { return get_attribute(L"id"); }
    std::wstring class_name() const;
    std::vector<std::wstring> class_list() const;

    // Attributes
    std::wstring get_attribute(const std::wstring& name) const;
    void set_attribute(const std::wstring& name, const std::wstring& value);
    bool has_attribute(const std::wstring& name) const;
    void remove_attribute(const std::wstring& name);
    std::vector<std::wstring> get_attribute_names() const;
    bool has_attributes() const { return !attributes_.empty(); }

    // Class list operations
    void class_list_add(const std::wstring& token);
    void class_list_remove(const std::wstring& token);
    bool class_list_contains(const std::wstring& token) const;
    void class_list_toggle(const std::wstring& token);

    // DOM queries
    std::shared_ptr<Element> query_selector(const std::wstring& selectors);
    std::vector<std::shared_ptr<Element>> query_selector_all(const std::wstring& selectors);
    std::vector<std::shared_ptr<Element>> get_elements_by_tag_name(const std::wstring& tag_name);
    std::vector<std::shared_ptr<Element>> get_elements_by_class_name(const std::wstring& class_name);

    // Matches & closest
    bool matches(const std::wstring& selectors) const;
    std::shared_ptr<Element> closest(const std::wstring& selectors) const;

    // Inner HTML
    std::wstring inner_html() const;
    void set_inner_html(const std::wstring& html);

    std::wstring outer_html() const;
    void set_outer_html(const std::wstring& html);

    // Insert adjacent
    void insert_adjacent_html(const std::wstring& position, const std::wstring& html);
    void insert_adjacent_element(const std::wstring& position, const std::shared_ptr<Element>& element);
    void insert_adjacent_text(const std::wstring& position, const std::wstring& text);

    // Dimensions
    double client_width() const;
    double client_height() const;
    double scroll_width() const;
    double scroll_height() const;

    // Scroll
    void scroll_into_view(bool align_to_top = true);
    void scroll(double x, double y);
    void scroll_by(double x, double y);
    double scroll_left() const;
    double scroll_top() const;
    void set_scroll_left(double val);
    void set_scroll_top(double val);

    // Geometry
    struct DOMRect {
        double x = 0, y = 0, width = 0, height = 0;
    };
    DOMRect get_bounding_client_rect() const;
    std::vector<DOMRect> get_client_rects() const;

    // Shadow DOM
    std::shared_ptr<ShadowRoot> attach_shadow(ShadowRootMode mode);
    std::shared_ptr<ShadowRoot> shadow_root() const { return shadow_root_; }

    // Form-associated
    std::wstring value() const;
    void set_value(const std::wstring& val);
    bool check_validity() const;

    // Focus
    void focus();
    void blur();
    bool is_focused() const { return focused_; }

    // Tab
    int tab_index() const { return tab_index_; }
    void set_tab_index(int idx) { tab_index_ = idx; }

    // Dataset
    std::wstring dataset(const std::wstring& key) const;
    void set_dataset(const std::wstring& key, const std::wstring& value);

    // Layout box (set by layout engine)
    struct ContentBox {
        double x = 0, y = 0, width = 0, height = 0;
    };
    ContentBox content_box;

    double scroll_left_ = 0;
    double scroll_top_ = 0;

private:
    std::wstring tag_name_;
    std::unordered_map<std::wstring, std::wstring> attributes_;
    bool focused_ = false;
    int tab_index_ = 0;

    // Simple HTML parser for innerHTML
    std::vector<std::shared_ptr<Node>> parse_html(const std::wstring& html);
    std::wstring serialize_html() const;

    void update_id_and_class();
};

class Text : public Node {
public:
    Text(const std::wstring& data) : Node(NodeType::Text) {
        node_name_ = L"#text";
        node_value_ = data;
    }

    std::wstring whole_text() const { return node_value_; }
    std::wstring data() const { return node_value_; }
    void set_data(const std::wstring& data) { node_value_ = data; }
    unsigned length() const { return static_cast<unsigned>(node_value_.length()); }
    std::wstring substring_data(unsigned offset, unsigned count) const;
    void append_data(const std::wstring& data);
    void insert_data(unsigned offset, const std::wstring& data);
    void delete_data(unsigned offset, unsigned count);
    void replace_data(unsigned offset, unsigned count, const std::wstring& data);
    std::shared_ptr<Text> split_text(unsigned offset);
};

class Comment : public Node {
public:
    Comment(const std::wstring& data) : Node(NodeType::Comment) {
        node_name_ = L"#comment";
        node_value_ = data;
    }
};

// Simplified event types
class Event : public std::enable_shared_from_this<Event> {
public:
    Event(const std::string& type, bool bubbles = true, bool cancelable = true)
        : type_(type), bubbles_(bubbles), cancelable_(cancelable) {}

    std::string type() const { return type_; }
    std::shared_ptr<EventTarget> target() const { return target_; }
    std::shared_ptr<EventTarget> current_target() const { return current_target_; }
    unsigned short event_phase() const { return event_phase_; }

    bool bubbles() const { return bubbles_; }
    bool cancelable() const { return cancelable_; }
    bool default_prevented() const { return default_prevented_; }

    void stop_propagation() { propagation_stopped_ = true; }
    void stop_immediate_propagation() { immediate_stopped_ = true; }
    void prevent_default() { if (cancelable_) default_prevented_ = true; }

    bool propagation_stopped() const { return propagation_stopped_; }
    bool immediate_stopped() const { return immediate_stopped_; }

    // Internal setters
    void set_target(const std::shared_ptr<EventTarget>& t) { target_ = t; }
    void set_current_target(const std::shared_ptr<EventTarget>& t) { current_target_ = t; }
    void set_event_phase(unsigned short phase) { event_phase_ = phase; }

    // Event phase constants
    static const unsigned short NONE = 0;
    static const unsigned short CAPTURING_PHASE = 1;
    static const unsigned short AT_TARGET = 2;
    static const unsigned short BUBBLING_PHASE = 3;

    // UIEvent specifics
    double detail() const { return detail_; }
    void set_detail(double d) { detail_ = d; }

    // MouseEvent sim
    double client_x() const { return client_x_; }
    double client_y() const { return client_y_; }
    double screen_x() const { return screen_x_; }
    double screen_y() const { return screen_y_; }
    void set_client_xy(double x, double y) { client_x_ = x; client_y_ = y; }
    void set_screen_xy(double x, double y) { screen_x_ = x; screen_y_ = y; }
    bool ctrl_key() const { return ctrl_key_; }
    bool shift_key() const { return shift_key_; }
    bool alt_key() const { return alt_key_; }
    bool meta_key() const { return meta_key_; }
    void set_modifiers(bool ctrl, bool shift, bool alt, bool meta) {
        ctrl_key_ = ctrl; shift_key_ = shift; alt_key_ = alt; meta_key_ = meta;
    }

    // FocusEvent sim
    std::shared_ptr<EventTarget> related_target() const { return related_target_; }
    void set_related_target(const std::shared_ptr<EventTarget>& rt) { related_target_ = rt; }

private:
    std::string type_;
    std::shared_ptr<EventTarget> target_;
    std::shared_ptr<EventTarget> current_target_;
    unsigned short event_phase_ = NONE;
    bool bubbles_ = true;
    bool cancelable_ = true;
    bool default_prevented_ = false;
    bool propagation_stopped_ = false;
    bool immediate_stopped_ = false;

    double detail_ = 0;
    double client_x_ = 0, client_y_ = 0;
    double screen_x_ = 0, screen_y_ = 0;
    bool ctrl_key_ = false, shift_key_ = false, alt_key_ = false, meta_key_ = false;
    std::shared_ptr<EventTarget> related_target_;
};

// MutationObserver
struct MutationRecord {
    std::string type; // "attributes", "childList", "characterData"
    std::shared_ptr<Node> target;
    std::vector<std::shared_ptr<Node>> added_nodes;
    std::vector<std::shared_ptr<Node>> removed_nodes;
    std::shared_ptr<Node> previous_sibling;
    std::shared_ptr<Node> next_sibling;
    std::wstring attribute_name;
    std::wstring attribute_namespace;
    std::wstring old_value;
    std::wstring new_value;
};

class MutationObserver : public std::enable_shared_from_this<MutationObserver> {
public:
    using Callback = std::function<void(const std::vector<MutationRecord>&, MutationObserver&)>;

    MutationObserver(Callback callback);

    void observe(const std::shared_ptr<Node>& target, const std::unordered_map<std::string, bool>& options);
    void disconnect();
    std::vector<MutationRecord> take_records();

    void queue_record(const MutationRecord& record);
    void deliver_records();

    struct ObserveTarget {
        std::weak_ptr<Node> target;
        bool child_list = false;
        bool attributes = false;
        bool character_data = false;
        bool subtree = false;
        bool attribute_old_value = false;
        bool character_data_old_value = false;
        std::vector<std::wstring> attribute_filter;
    };

private:
    Callback callback_;
    std::vector<MutationRecord> records_;
    std::vector<ObserveTarget> observe_targets_;
    bool delivering_ = false;

    friend class MutationNotifier;
};

// Mutation notifier helper
class MutationNotifier {
public:
    static void notify_child_added(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child);
    static void notify_child_removed(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child);
    static void notify_attribute_changed(const std::shared_ptr<Node>& target, const std::wstring& name,
                                          const std::wstring& old_value, const std::wstring& new_value);
    static void notify_character_data_changed(const std::shared_ptr<Node>& target,
                                               const std::wstring& old_value, const std::wstring& new_value);

    static void register_observer(const std::shared_ptr<MutationObserver>& observer);
    static void unregister_observer(const std::shared_ptr<MutationObserver>& observer);
    static void deliver_all();

private:
    static std::vector<std::weak_ptr<MutationObserver>> observers_;
};

// Form validation utilities
enum class ValidityStateFlag {
    ValueMissing, TypeMismatch, PatternMismatch, TooLong, TooShort,
    RangeUnderflow, RangeOverflow, StepMismatch, BadInput, CustomError,
    Valid
};

struct ValidityState {
    bool value_missing = false;
    bool type_mismatch = false;
    bool pattern_mismatch = false;
    bool too_long = false;
    bool too_short = false;
    bool range_underflow = false;
    bool range_overflow = false;
    bool step_mismatch = false;
    bool bad_input = false;
    bool custom_error = false;
    bool valid = true;

    operator bool() const { return valid; }
};

} // namespace hre::dom
