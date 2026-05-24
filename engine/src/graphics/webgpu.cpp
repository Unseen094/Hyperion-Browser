#include <hre/graphics/webgpu.hpp>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace hre::graphics {

// ---- GPUAdapter ------------------------------------------------------------

std::shared_ptr<gpu_device> gpu_adapter::request_device() {
    auto device = std::make_shared<gpu_device>();
    return device;
}

// ---- GPUDevice -------------------------------------------------------------

std::shared_ptr<gpu_buffer> gpu_device::create_buffer(const gpu_buffer_descriptor& desc) {
    auto buf = std::make_shared<gpu_buffer>(desc);
    return buf;
}

std::shared_ptr<gpu_texture> gpu_device::create_texture(const gpu_texture_descriptor& desc) {
    auto tex = std::make_shared<gpu_texture>(desc);
    return tex;
}

std::shared_ptr<gpu_sampler> gpu_device::create_sampler(
    gpu_address_mode address_mode_u,
    gpu_address_mode address_mode_v,
    gpu_filter_mode mag_filter,
    gpu_filter_mode min_filter) {
    return std::make_shared<gpu_sampler>(address_mode_u, address_mode_v, mag_filter, min_filter);
}

std::shared_ptr<gpu_bind_group_layout> gpu_device::create_bind_group_layout(
    std::vector<gpu_bind_group_layout_entry> entries) {
    return std::make_shared<gpu_bind_group_layout>(std::move(entries));
}

std::shared_ptr<gpu_bind_group> gpu_device::create_bind_group(
    std::shared_ptr<gpu_bind_group_layout> layout,
    std::vector<gpu_bind_group_entry> entries) {
    return std::make_shared<gpu_bind_group>(std::move(layout), std::move(entries));
}

std::shared_ptr<gpu_render_pipeline> gpu_device::create_render_pipeline(
    const gpu_render_pipeline_descriptor& desc) {
    auto pipeline = std::make_shared<gpu_render_pipeline>();
    pipeline->desc_ = desc;
    return pipeline;
}

std::shared_ptr<gpu_command_encoder> gpu_device::create_command_encoder() {
    return std::make_shared<gpu_command_encoder>();
}

std::shared_ptr<gpu_queue> gpu_device::get_queue() {
    auto queue = std::make_shared<gpu_queue>();
    return queue;
}

// ---- GPUCommandEncoder -----------------------------------------------------

std::vector<uint8_t> gpu_command_encoder::finish() {
    std::vector<uint8_t> cmd_buffer;
    cmd_buffer.push_back(pass_active_ ? 0x01 : 0x00);
    return cmd_buffer;
}

// ---- GPUQueue --------------------------------------------------------------

void gpu_queue::submit(std::vector<std::shared_ptr<gpu_command_encoder>> commands) {
    for (auto& cmd : commands) {
        auto buf = cmd->finish();
    }
    if (submitted_callback_) submitted_callback_();
}

void gpu_queue::on_submitted_work_done(std::function<void()> callback) {
    submitted_callback_ = std::move(callback);
}

} // namespace hre::graphics
