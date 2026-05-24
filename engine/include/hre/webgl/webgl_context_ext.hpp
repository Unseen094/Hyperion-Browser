#pragma once

#include <hre/webgl/webgl_context.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <wrl/client.h>
#include <d3d11_4.h>

namespace hre::webgl {

// ---- WebGL 2.0 Extensions --------------------------------------------------

struct webgl_transform_feedback {
    uint32_t handle = 0;
    bool active = false;
    bool paused = false;
    gl_enum primitive_mode = gl_enum::TRIANGLES;
    std::vector<uint32_t> bound_buffers;
};

struct webgl_uniform_buffer {
    uint32_t handle = 0;
    size_t byte_length = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> d3d_buffer;
};

struct webgl_renderbuffer {
    uint32_t handle = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    gl_enum internal_format = gl_enum::DEPTH24_STENCIL8;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d_texture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
};

struct webgl_sampler_ext {
    uint32_t handle = 0;
    gl_enum min_filter = gl_enum::LINEAR;
    gl_enum mag_filter = gl_enum::LINEAR;
    gl_enum wrap_s = gl_enum::REPEAT;
    gl_enum wrap_t = gl_enum::REPEAT;
    gl_enum wrap_r = gl_enum::REPEAT;
    gl_enum compare_mode = static_cast<gl_enum>(0);
    gl_enum compare_func = static_cast<gl_enum>(0x0203);
};

struct webgl_query {
    uint32_t handle = 0;
    gl_enum target = static_cast<gl_enum>(0);
    bool available = false;
    uint64_t result = 0;
    Microsoft::WRL::ComPtr<ID3D11Query> d3d_query;
};

class webgl_context_ext {
public:
    webgl_context_ext(webgl_context* base);
    ~webgl_context_ext();

    // VAO (WebGL 2 native)
    void bind_vertex_array_ext(uint32_t vao);
    void delete_vertex_array_ext(uint32_t vao);

    // Transform feedback
    uint32_t create_transform_feedback();
    void delete_transform_feedback(uint32_t tf);
    void bind_transform_feedback(gl_enum target, uint32_t tf);
    void begin_transform_feedback_ext(gl_enum primitive_mode);
    void end_transform_feedback_ext();
    void pause_transform_feedback();
    void resume_transform_feedback();
    void transform_feedback_varyings(uint32_t program,
                                     const std::vector<std::string>& varyings,
                                     gl_enum buffer_mode);
    void bind_buffer_base_ext(gl_enum target, uint32_t index, uint32_t buffer);
    void bind_buffer_range_ext(gl_enum target, uint32_t index,
                               uint32_t buffer, size_t offset, size_t size);

    // Uniform buffers
    uint32_t create_uniform_buffer();
    void delete_uniform_buffer(uint32_t ub);
    void bind_uniform_buffer(gl_enum target, uint32_t ub);
    void uniform_buffer_data(uint32_t ub, const void* data, size_t size);
    void uniform_buffer_sub_data(uint32_t ub, size_t offset, const void* data, size_t size);

    // Uniform block
    void uniform_block_binding(uint32_t program, uint32_t block_index, uint32_t binding_point);
    uint32_t get_uniform_block_index(uint32_t program, const std::string& name);

    // Renderbuffer objects
    uint32_t create_renderbuffer();
    void delete_renderbuffer(uint32_t rb);
    void bind_renderbuffer(gl_enum target, uint32_t rb);
    void renderbuffer_storage(gl_enum target, gl_enum internal_format,
                              int width, int height);
    void framebuffer_renderbuffer(gl_enum target, gl_enum attachment,
                                  gl_enum rb_target, uint32_t rb);

    // Shader compilation with error handling
    bool compile_shader_ext(uint32_t shader);
    std::string get_shader_info_log_ext(uint32_t shader);

    // Texture unit management & mipmapping
    void active_texture_ext(gl_enum unit);
    void bind_texture_ext(gl_enum target, uint32_t texture);
    void tex_storage_2d_ext(gl_enum target, int levels, gl_enum internal_format,
                            int width, int height);
    void tex_storage_3d_ext(gl_enum target, int levels, gl_enum internal_format,
                            int width, int height, int depth);
    void generate_mipmap(gl_enum target);
    void tex_parameter_i_ext(gl_enum target, gl_enum pname, int param);

    // Draw indirect
    void draw_arrays_indirect(gl_enum mode, const void* indirect);
    void draw_elements_indirect(gl_enum mode, gl_enum type, const void* indirect);

    // Queries
    uint32_t create_query();
    void delete_query(uint32_t query);
    void begin_query(gl_enum target, uint32_t query);
    void end_query(gl_enum target);
    uint64_t get_query_result(uint32_t query);

    uint32_t next_handle() { return ++m_next_handle; }

private:
    webgl_context* m_base;
    std::map<uint32_t, webgl_transform_feedback> m_transform_feedbacks;
    std::map<uint32_t, webgl_uniform_buffer> m_uniform_buffers;
    std::map<uint32_t, webgl_renderbuffer> m_renderbuffers;
    std::map<uint32_t, webgl_sampler_ext> m_samplers;
    std::map<uint32_t, webgl_query> m_queries;

    uint32_t m_current_tf = 0;
    uint32_t m_current_ub = 0;
    uint32_t m_current_rb = 0;
    uint32_t m_active_texture_unit_ext = 0;
    uint32_t m_next_handle = 10000;
};

} // namespace hre::webgl
