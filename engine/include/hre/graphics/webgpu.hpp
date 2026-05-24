#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <map>
#include <variant>

namespace hre::graphics {

enum class gpu_buffer_usage {
    MAP_READ = 1,
    MAP_WRITE = 2,
    COPY_SRC = 4,
    COPY_DST = 8,
    INDEX = 16,
    VERTEX = 32,
    UNIFORM = 64,
    STORAGE = 128,
    INDIRECT = 256,
    QUERY_RESOLVE = 512
};

enum class gpu_texture_usage {
    COPY_SRC = 1,
    COPY_DST = 2,
    TEXTURE_BINDING = 4,
    STORAGE_BINDING = 8,
    RENDER_ATTACHMENT = 16
};

enum class gpu_texture_format {
    R8_UNORM,
    R8G8_UNORM,
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    B8G8R8A8_UNORM,
    B8G8R8A8_SRGB,
    D32_FLOAT,
    D24_UNORM_S8_UINT,
    D32_FLOAT_S8_UINT
};

enum class gpu_vertex_format {
    FLOAT32,
    FLOAT16,
    UINT32,
    UINT16,
    UINT8,
    SINT32,
    SINT16,
    SINT8
};

enum class gpu_primitive_topology {
    POINT_LIST,
    LINE_LIST,
    LINE_STRIP,
    TRIANGLE_LIST,
    TRIANGLE_STRIP
};

enum class gpu_address_mode {
    REPEAT,
    MIRROR_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER
};

enum class gpu_filter_mode {
    NEAREST,
    LINEAR
};

enum class gpu_blend_factor {
    ZERO,
    ONE,
    SRC_COLOR,
    ONE_MINUS_SRC_COLOR,
    SRC_ALPHA,
    ONE_MINUS_SRC_ALPHA,
    DST_COLOR,
    ONE_MINUS_DST_COLOR,
    DST_ALPHA,
    ONE_MINUS_DST_ALPHA,
    SRC_ALPHA_SATURATED,
    CONSTANT_COLOR
};

enum class gpu_blend_operation {
    ADD,
    SUBTRACT,
    REVERSE_SUBTRACT,
    MIN,
    MAX
};

enum class gpu_compare_function {
    NEVER,
    LESS,
    EQUAL,
    LESS_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_EQUAL,
    ALWAYS
};

struct gpu_color {
    float r = 0, g = 0, b = 0, a = 1;
};

struct gpu_extent_3d {
    uint32_t width = 1, height = 1, depth_or_array_layers = 1;
};

struct gpu_vertex_attribute {
    uint32_t shader_location = 0;
    uint64_t offset = 0;
    gpu_vertex_format format = gpu_vertex_format::FLOAT32;
};

struct gpu_vertex_buffer_layout {
    uint64_t array_stride = 0;
    uint64_t step_mode = 0;
    std::vector<gpu_vertex_attribute> attributes;
};

struct gpu_blend_component {
    gpu_blend_factor src_factor = gpu_blend_factor::ONE;
    gpu_blend_factor dst_factor = gpu_blend_factor::ZERO;
    gpu_blend_operation operation = gpu_blend_operation::ADD;
};

struct gpu_color_target_state {
    gpu_texture_format format = gpu_texture_format::R8G8B8A8_UNORM;
    gpu_blend_component color;
    gpu_blend_component alpha;
    bool blend_enabled = false;
};

struct gpu_depth_stencil_state {
    gpu_texture_format format = gpu_texture_format::D32_FLOAT;
    bool depth_write_enabled = true;
    gpu_compare_function depth_compare = gpu_compare_function::LESS;
};

struct gpu_multisample_state {
    uint32_t count = 1;
    uint32_t mask = 0xFFFFFFFF;
    bool alpha_to_coverage_enabled = false;
};

struct gpu_shader_module {
    std::string code;
    std::string entry_point = "main";
};

struct gpu_vertex_state {
    std::vector<gpu_vertex_buffer_layout> buffers;
};

struct gpu_primitive_state {
    gpu_primitive_topology topology = gpu_primitive_topology::TRIANGLE_LIST;
    bool strip_index_format = false;
    bool front_face = false;
    bool cull_mode = false;
};

struct gpu_fragment_state {
    std::vector<gpu_color_target_state> targets;
};

struct gpu_render_pipeline_descriptor {
    gpu_vertex_state vertex;
    gpu_primitive_state primitive;
    gpu_depth_stencil_state depth_stencil;
    gpu_multisample_state multisample;
    gpu_fragment_state fragment;
    gpu_shader_module vertex_shader;
    gpu_shader_module fragment_shader;
    std::string label;
};

struct gpu_buffer_descriptor {
    uint64_t size = 0;
    gpu_buffer_usage usage = gpu_buffer_usage::VERTEX;
    bool mapped_at_creation = false;
};

struct gpu_texture_descriptor {
    gpu_extent_3d size;
    uint32_t mip_level_count = 1;
    uint32_t sample_count = 1;
    gpu_texture_format format = gpu_texture_format::R8G8B8A8_UNORM;
    gpu_texture_usage usage = gpu_texture_usage::TEXTURE_BINDING;
};

struct gpu_bind_group_entry {
    uint32_t binding = 0;
    std::variant<std::monostate, std::shared_ptr<class gpu_buffer>,
                 std::shared_ptr<class gpu_texture>> resource;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct gpu_bind_group_layout_entry {
    uint32_t binding = 0;
    uint32_t visibility = 0;
    uint32_t type = 0;
    bool has_dynamic_offset = false;
};

class gpu_buffer {
public:
    gpu_buffer(const gpu_buffer_descriptor& desc) : desc_(desc) {
        data_.resize(desc.size, 0);
    }

    void set_data(const void* src, uint64_t offset, uint64_t size) {
        if (offset + size > data_.size()) size = data_.size() - offset;
        memcpy(data_.data() + offset, src, static_cast<size_t>(size));
    }

    void* get_mapped_range(uint64_t offset = 0, uint64_t size = 0) {
        if (size == 0 || size > data_.size()) size = data_.size();
        return data_.data() + offset;
    }

    void unmap() {}

    const gpu_buffer_descriptor& descriptor() const { return desc_; }
    uint64_t size() const { return desc_.size; }

private:
    gpu_buffer_descriptor desc_;
    std::vector<uint8_t> data_;
};

class gpu_texture {
public:
    gpu_texture(const gpu_texture_descriptor& desc) : desc_(desc) {}

    const gpu_texture_descriptor& descriptor() const { return desc_; }

private:
    gpu_texture_descriptor desc_;
};

class gpu_sampler {
public:
    gpu_sampler(gpu_address_mode address_mode_u = gpu_address_mode::CLAMP_TO_EDGE,
                gpu_address_mode address_mode_v = gpu_address_mode::CLAMP_TO_EDGE,
                gpu_filter_mode mag_filter = gpu_filter_mode::LINEAR,
                gpu_filter_mode min_filter = gpu_filter_mode::LINEAR)
        : address_mode_u_(address_mode_u), address_mode_v_(address_mode_v),
          mag_filter_(mag_filter), min_filter_(min_filter) {}

private:
    gpu_address_mode address_mode_u_, address_mode_v_;
    gpu_filter_mode mag_filter_, min_filter_;
};

class gpu_bind_group_layout {
public:
    gpu_bind_group_layout() = default;
    explicit gpu_bind_group_layout(std::vector<gpu_bind_group_layout_entry> entries)
        : entries_(std::move(entries)) {}

private:
    std::vector<gpu_bind_group_layout_entry> entries_;
};

class gpu_bind_group {
public:
    gpu_bind_group() = default;
    gpu_bind_group(std::shared_ptr<gpu_bind_group_layout> layout,
                   std::vector<gpu_bind_group_entry> entries)
        : layout_(std::move(layout)), entries_(std::move(entries)) {}

private:
    std::shared_ptr<gpu_bind_group_layout> layout_;
    std::vector<gpu_bind_group_entry> entries_;
};

class gpu_render_pipeline {
public:
    gpu_render_pipeline() = default;

    const gpu_render_pipeline_descriptor& descriptor() const { return desc_; }

private:
    friend class gpu_device;
    gpu_render_pipeline_descriptor desc_;
};

class gpu_command_encoder {
public:
    gpu_command_encoder() = default;

    void begin_render_pass(const gpu_color& clear_color = {0, 0, 0, 1}) {
        clear_color_ = clear_color;
        pass_active_ = true;
    }

    void end_render_pass() { pass_active_ = false; }

    void set_pipeline(std::shared_ptr<gpu_render_pipeline> pipeline) {
        current_pipeline_ = pipeline;
    }

    void set_vertex_buffer(uint32_t slot, std::shared_ptr<gpu_buffer> buffer, uint64_t offset = 0) {
        vertex_buffers_[slot] = {buffer, offset};
    }

    void set_bind_group(uint32_t index, std::shared_ptr<gpu_bind_group> group) {
        bind_groups_[index] = group;
    }

    void draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0, uint32_t first_instance = 0) {
        draw_calls_.push_back({vertex_count, instance_count, first_vertex, first_instance});
    }

    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                      uint32_t first_index = 0, int32_t base_vertex = 0,
                      uint32_t first_instance = 0) {
        draw_calls_.push_back({index_count, instance_count, first_index,
                               static_cast<uint32_t>(base_vertex), first_instance});
    }

    void copy_buffer_to_buffer(std::shared_ptr<gpu_buffer> src, uint64_t src_offset,
                                std::shared_ptr<gpu_buffer> dst, uint64_t dst_offset,
                                uint64_t size) {
        copies_.push_back({src, src_offset, dst, dst_offset, size});
    }

    void copy_texture_to_buffer() {}

    std::vector<uint8_t> finish();

private:
    struct draw_call {
        uint32_t vertex_count = 0;
        uint32_t instance_count = 1;
        uint32_t first_vertex = 0;
        uint32_t first_instance = 0;
        uint32_t index_count = 0;
        bool indexed = false;
    };

    struct buffer_copy {
        std::shared_ptr<gpu_buffer> src;
        uint64_t src_offset = 0;
        std::shared_ptr<gpu_buffer> dst;
        uint64_t dst_offset = 0;
        uint64_t size = 0;
    };

    gpu_color clear_color_;
    bool pass_active_ = false;
    std::shared_ptr<gpu_render_pipeline> current_pipeline_;
    std::map<uint32_t, std::pair<std::shared_ptr<gpu_buffer>, uint64_t>> vertex_buffers_;
    std::map<uint32_t, std::shared_ptr<gpu_bind_group>> bind_groups_;
    std::vector<draw_call> draw_calls_;
    std::vector<buffer_copy> copies_;
};

class gpu_queue {
public:
    gpu_queue() = default;

    void submit(std::vector<std::shared_ptr<gpu_command_encoder>> commands);
    void on_submitted_work_done(std::function<void()> callback);

private:
    std::function<void()> submitted_callback_;
};

class gpu_device {
public:
    gpu_device() = default;

    std::shared_ptr<gpu_buffer> create_buffer(const gpu_buffer_descriptor& desc);
    std::shared_ptr<gpu_texture> create_texture(const gpu_texture_descriptor& desc);
    std::shared_ptr<gpu_sampler> create_sampler(
        gpu_address_mode address_mode_u = gpu_address_mode::CLAMP_TO_EDGE,
        gpu_address_mode address_mode_v = gpu_address_mode::CLAMP_TO_EDGE,
        gpu_filter_mode mag_filter = gpu_filter_mode::LINEAR,
        gpu_filter_mode min_filter = gpu_filter_mode::LINEAR);

    std::shared_ptr<gpu_bind_group_layout> create_bind_group_layout(
        std::vector<gpu_bind_group_layout_entry> entries);
    std::shared_ptr<gpu_bind_group> create_bind_group(
        std::shared_ptr<gpu_bind_group_layout> layout,
        std::vector<gpu_bind_group_entry> entries);

    std::shared_ptr<gpu_render_pipeline> create_render_pipeline(
        const gpu_render_pipeline_descriptor& desc);

    std::shared_ptr<gpu_command_encoder> create_command_encoder();
    std::shared_ptr<gpu_queue> get_queue();

    std::string label;
};

class gpu_adapter {
public:
    gpu_adapter() = default;

    std::string name() const { return "Hyperion GPU Adapter"; }
    std::vector<std::string> features() const { return {}; }

    std::shared_ptr<gpu_device> request_device();

    bool is_fallback_adapter() const { return false; }
};

} // namespace hre::graphics
