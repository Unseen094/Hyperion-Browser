#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace hre::net::http2 {

class priority_scheduler {
public:
    priority_scheduler();
    ~priority_scheduler();

    void set_priority(uint32_t stream_id, uint32_t parent_id, int weight, bool exclusive);
    std::vector<uint32_t> schedule() const;
    void remove(uint32_t stream_id);

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace hre::net::http2
