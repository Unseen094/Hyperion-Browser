#include <hre/webgl/webgl_context_ext.hpp>
#include <algorithm>
#include <cstring>

namespace hre::webgl {

webgl_context_ext::webgl_context_ext(webgl_context* base) : m_base(base) {}
webgl_context_ext::~webgl_context_ext() = default;

// ---- VAO -------------------------------------------------------------------

void webgl_context_ext::bind_vertex_array_ext(uint32_t vao) {
    m_base->bind_vertex_array(vao);
}

void webgl_context_ext::delete_vertex_array_ext(uint32_t vao) {
    m_base->delete_vertex_array(vao);
}

// ---- Transform Feedback ----------------------------------------------------

uint32_t webgl_context_ext::create_transform_feedback() {
    uint32_t handle = next_handle();
    m_transform_feedbacks[handle] = {};
    m_transform_feedbacks[handle].handle = handle;
    return handle;
}

void webgl_context_ext::delete_transform_feedback(uint32_t tf) {
    m_transform_feedbacks.erase(tf);
}

void webgl_context_ext::bind_transform_feedback(gl_enum target, uint32_t tf) {
    if (target == gl_enum::TRANSFORM_FEEDBACK) {
        m_current_tf = tf;
    }
}

void webgl_context_ext::begin_transform_feedback_ext(gl_enum primitive_mode) {
    auto it = m_transform_feedbacks.find(m_current_tf);
    if (it != m_transform_feedbacks.end()) {
        it->second.active = true;
        it->second.primitive_mode = primitive_mode;
    }
}

void webgl_context_ext::end_transform_feedback_ext() {
    auto it = m_transform_feedbacks.find(m_current_tf);
    if (it != m_transform_feedbacks.end()) {
        it->second.active = false;
        it->second.paused = false;
    }
}

void webgl_context_ext::pause_transform_feedback() {
    auto it = m_transform_feedbacks.find(m_current_tf);
    if (it != m_transform_feedbacks.end()) {
        it->second.paused = true;
    }
}

void webgl_context_ext::resume_transform_feedback() {
    auto it = m_transform_feedbacks.find(m_current_tf);
    if (it != m_transform_feedbacks.end()) {
        it->second.paused = false;
    }
}

void webgl_context_ext::transform_feedback_varyings(
    uint32_t program, const std::vector<std::string>& varyings, gl_enum buffer_mode) {
    (void)program; (void)varyings; (void)buffer_mode;
}

void webgl_context_ext::bind_buffer_base_ext(gl_enum target, uint32_t index, uint32_t buffer) {
    if (target == gl_enum::TRANSFORM_FEEDBACK_BUFFER) {
        auto it = m_transform_feedbacks.find(m_current_tf);
        if (it != m_transform_feedbacks.end()) {
            if (index >= it->second.bound_buffers.size()) {
                it->second.bound_buffers.resize(index + 1, 0);
            }
            it->second.bound_buffers[index] = buffer;
        }
    } else if (target == gl_enum::UNIFORM_BUFFER) {
        auto ub_it = m_uniform_buffers.find(buffer);
        if (ub_it != m_uniform_buffers.end()) {
            ID3D11Buffer* buf[] = { ub_it->second.d3d_buffer.Get() };
            UINT first[] = { index };
            UINT counts[] = { 1 };
            // In a real impl we'd call m_base->d3d_context->CSSetConstantBuffers etc.
        }
    }
}

void webgl_context_ext::bind_buffer_range_ext(gl_enum target, uint32_t index,
                                               uint32_t buffer, size_t offset, size_t size) {
    (void)target; (void)index; (void)buffer; (void)offset; (void)size;
}

// ---- Uniform Buffers -------------------------------------------------------

uint32_t webgl_context_ext::create_uniform_buffer() {
    uint32_t handle = next_handle();
    m_uniform_buffers[handle] = {};
    m_uniform_buffers[handle].handle = handle;
    return handle;
}

void webgl_context_ext::delete_uniform_buffer(uint32_t ub) {
    m_uniform_buffers.erase(ub);
}

void webgl_context_ext::bind_uniform_buffer(gl_enum target, uint32_t ub) {
    if (target == gl_enum::UNIFORM_BUFFER) {
        m_current_ub = ub;
    }
}

void webgl_context_ext::uniform_buffer_data(uint32_t ub, const void* data, size_t size) {
    auto it = m_uniform_buffers.find(ub);
    if (it == m_uniform_buffers.end()) return;
    it->second.byte_length = size;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(size);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = data;

    ID3D11Device5* device = nullptr;
    (void)device;
    // Would use m_base's device
}

void webgl_context_ext::uniform_buffer_sub_data(uint32_t ub, size_t offset,
                                                  const void* data, size_t size) {
    (void)ub; (void)offset; (void)data; (void)size;
}

void webgl_context_ext::uniform_block_binding(uint32_t program, uint32_t block_index,
                                               uint32_t binding_point) {
    (void)program; (void)block_index; (void)binding_point;
}

uint32_t webgl_context_ext::get_uniform_block_index(uint32_t program, const std::string& name) {
    (void)program; (void)name;
    return 0;
}

// ---- Renderbuffer Objects --------------------------------------------------

uint32_t webgl_context_ext::create_renderbuffer() {
    uint32_t handle = next_handle();
    m_renderbuffers[handle] = {};
    m_renderbuffers[handle].handle = handle;
    return handle;
}

void webgl_context_ext::delete_renderbuffer(uint32_t rb) {
    m_renderbuffers.erase(rb);
}

void webgl_context_ext::bind_renderbuffer(gl_enum target, uint32_t rb) {
    if (target == static_cast<gl_enum>(0x8D41)) {
        m_current_rb = rb;
    }
}

void webgl_context_ext::renderbuffer_storage(gl_enum target, gl_enum internal_format,
                                              int width, int height) {
    auto it = m_renderbuffers.find(m_current_rb);
    if (it == m_renderbuffers.end()) return;
    it->second.width = static_cast<uint32_t>(width);
    it->second.height = static_cast<uint32_t>(height);
    it->second.internal_format = internal_format;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;

    if (internal_format == gl_enum::DEPTH24_STENCIL8 ||
        internal_format == gl_enum::DEPTH32F_STENCIL8) {
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    } else if (internal_format == gl_enum::DEPTH_COMPONENT24 ||
               internal_format == gl_enum::DEPTH_COMPONENT32F) {
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    } else {
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    }
    desc.Usage = D3D11_USAGE_DEFAULT;
}

void webgl_context_ext::framebuffer_renderbuffer(gl_enum target, gl_enum attachment,
                                                  gl_enum rb_target, uint32_t rb) {
    (void)target; (void)attachment; (void)rb_target;
    auto rb_it = m_renderbuffers.find(rb);
    if (rb_it == m_renderbuffers.end()) return;
    // Attach to currently bound framebuffer
}

// ---- Shader Compilation with Error Handling --------------------------------

bool webgl_context_ext::compile_shader_ext(uint32_t shader) {
    bool result = m_base->compile_shader(shader);
    if (!result) {
        std::string log = m_base->get_shader_info_log(shader);
        (void)log;
    }
    return result;
}

std::string webgl_context_ext::get_shader_info_log_ext(uint32_t shader) {
    return m_base->get_shader_info_log(shader);
}

// ---- Texture Unit Management & Mipmapping ----------------------------------

void webgl_context_ext::active_texture_ext(gl_enum unit) {
    m_active_texture_unit_ext = static_cast<uint32_t>(unit) -
                                static_cast<uint32_t>(gl_enum::TEXTURE_2D);
    m_base->active_texture(unit);
}

void webgl_context_ext::bind_texture_ext(gl_enum target, uint32_t texture) {
    m_base->bind_texture(target, texture);
}

void webgl_context_ext::tex_storage_2d_ext(gl_enum target, int levels,
                                            gl_enum internal_format,
                                            int width, int height) {
    m_base->tex_storage_2d(target, levels, internal_format, width, height);

    // Create full mip chain
    int w = width, h = height;
    for (int level = 1; level < levels; ++level) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        m_base->tex_image_2d(target, level, internal_format, w, h, 0,
                             gl_enum::RGBA, gl_enum::UNSIGNED_BYTE, nullptr);
    }
}

void webgl_context_ext::tex_storage_3d_ext(gl_enum target, int levels,
                                            gl_enum internal_format,
                                            int width, int height, int depth) {
    m_base->tex_storage_3d(target, levels, internal_format, width, height, depth);
}

void webgl_context_ext::generate_mipmap(gl_enum target) {
    (void)target;
}

void webgl_context_ext::tex_parameter_i_ext(gl_enum target, gl_enum pname, int param) {
    if (pname == gl_enum::TEXTURE_MIN_FILTER ||
        pname == gl_enum::TEXTURE_MAG_FILTER ||
        pname == gl_enum::TEXTURE_WRAP_S ||
        pname == gl_enum::TEXTURE_WRAP_T ||
        pname == gl_enum::TEXTURE_WRAP_R) {
        m_base->tex_parameter_i(target, pname, param);
    }
}

// ---- Draw Indirect ---------------------------------------------------------

void webgl_context_ext::draw_arrays_indirect(gl_enum mode, const void* indirect) {
    const uint32_t* args = static_cast<const uint32_t*>(indirect);
    m_base->draw_arrays_instanced(mode, static_cast<int>(args[1]),
                                   static_cast<int>(args[0]),
                                   static_cast<int>(args[2]));
}

void webgl_context_ext::draw_elements_indirect(gl_enum mode, gl_enum type, const void* indirect) {
    (void)mode; (void)type; (void)indirect;
}

// ---- Queries ---------------------------------------------------------------

uint32_t webgl_context_ext::create_query() {
    uint32_t handle = next_handle();
    m_queries[handle] = {};
    m_queries[handle].handle = handle;
    return handle;
}

void webgl_context_ext::delete_query(uint32_t query) {
    m_queries.erase(query);
}

void webgl_context_ext::begin_query(gl_enum target, uint32_t query) {
    auto it = m_queries.find(query);
    if (it == m_queries.end()) return;
    it->second.target = target;
    it->second.available = false;
    it->second.result = 0;

    D3D11_QUERY_DESC desc = {};
    desc.Query = D3D11_QUERY_OCCLUSION;
    if (target == static_cast<gl_enum>(0x8C2F)) {
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    }
    ID3D11Device5* device = nullptr;
    (void)device;
}

void webgl_context_ext::end_query(gl_enum target) {
    (void)target;
}

uint64_t webgl_context_ext::get_query_result(uint32_t query) {
    auto it = m_queries.find(query);
    if (it == m_queries.end()) return 0;
    it->second.available = true;
    return it->second.result;
}

} // namespace hre::webgl
