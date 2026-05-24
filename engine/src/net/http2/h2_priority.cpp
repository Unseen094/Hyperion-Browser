#include "hre/net/http2/h2_priority.hpp"
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <functional>

namespace hre::net::http2 {

struct priority_node {
    uint32_t stream_id;
    uint32_t parent_id;
    int weight;
    bool exclusive;
    std::vector<uint32_t> children;

    priority_node(uint32_t id) : stream_id(id), parent_id(0), weight(16), exclusive(false) {}
};

class priority_scheduler::impl {
public:
    std::unordered_map<uint32_t, priority_node> nodes_;
    uint32_t default_weight_ = 16;

    impl() { nodes_.emplace(0, 0); }

    void set_priority(uint32_t stream_id, uint32_t parent_id, int weight, bool exclusive);
    std::vector<uint32_t> schedule_order() const;
    void remove_stream(uint32_t stream_id);
};

void priority_scheduler::impl::set_priority(uint32_t id, uint32_t parent, int w, bool excl) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) it = nodes_.emplace(id, id).first;
    auto& n = it->second;
    n.parent_id = parent;
    n.weight = (std::max)(1, (std::min)(w, 256));
    n.exclusive = excl;

    auto pit = nodes_.find(parent);
    if (pit != nodes_.end() && excl) pit->second.children.clear();
    if (pit != nodes_.end()) pit->second.children.push_back(id);
}

std::vector<uint32_t> priority_scheduler::impl::schedule_order() const {
    std::vector<uint32_t> result;
    std::function<void(uint32_t)> dfs = [&](uint32_t pid) {
        auto it = nodes_.find(pid);
        if (it == nodes_.end()) return;

        std::vector<std::pair<uint32_t, int>> weighted;
        for (auto cid : it->second.children) {
            auto cit = nodes_.find(cid);
            if (cit != nodes_.end()) weighted.push_back({cid, cit->second.weight});
        }

        std::sort(weighted.begin(), weighted.end(),
            [](auto& a, auto& b) { return a.second > b.second; });

        for (auto& [cid, w] : weighted) {
            if (cid != 0) result.push_back(cid);
            dfs(cid);
        }
    };

    dfs(0);
    return result;
}

void priority_scheduler::impl::remove_stream(uint32_t id) {
    nodes_.erase(id);
}

priority_scheduler::priority_scheduler() : pimpl_(std::make_unique<impl>()) {}
priority_scheduler::~priority_scheduler() = default;

void priority_scheduler::set_priority(uint32_t s, uint32_t p, int w, bool e) {
    pimpl_->set_priority(s, p, w, e);
}

std::vector<uint32_t> priority_scheduler::schedule() const {
    return pimpl_->schedule_order();
}

void priority_scheduler::remove(uint32_t s) {
    pimpl_->remove_stream(s);
}

} // namespace hre::net::http2
