#pragma once

#include "hre/dom/node.hpp"
#include <memory>

namespace hre::dom {

class Range {
public:
    Range();

    void set_start(const std::shared_ptr<Node>& node, unsigned offset);
    void set_end(const std::shared_ptr<Node>& node, unsigned offset);
    void set_start_before(const std::shared_ptr<Node>& node);
    void set_start_after(const std::shared_ptr<Node>& node);
    void set_end_before(const std::shared_ptr<Node>& node);
    void set_end_after(const std::shared_ptr<Node>& node);

    void collapse(bool to_start = false);
    bool is_collapsed() const;
    unsigned start_offset() const { return start_offset_; }
    unsigned end_offset() const { return end_offset_; }
    std::shared_ptr<Node> start_container() const { return start_container_; }
    std::shared_ptr<Node> end_container() const { return end_container_; }

    std::shared_ptr<Node> common_ancestor_container() const;

    // Mutation
    std::shared_ptr<DocumentFragment> extract_contents();
    std::shared_ptr<DocumentFragment> clone_contents();
    void delete_contents();
    void insert_node(const std::shared_ptr<Node>& node);
    void surround_contents(const std::shared_ptr<Node>& new_parent);

    std::wstring to_string() const;

private:
    std::shared_ptr<Node> start_container_;
    unsigned start_offset_ = 0;
    std::shared_ptr<Node> end_container_;
    unsigned end_offset_ = 0;
    bool collapsed_ = true;

    std::shared_ptr<Node> find_common_ancestor(std::shared_ptr<Node> a, std::shared_ptr<Node> b) const;
    unsigned index_of(const std::shared_ptr<Node>& parent, const std::shared_ptr<Node>& child) const;
};

} // namespace hre::dom
