#include <hre/webgl/webgl_context.hpp>
#include <algorithm>
#include <cstring>

namespace hre::webgl {

webgl_context::webgl_context() = default;
webgl_context::~webgl_context() = default;

bool webgl_context::init(ID3D11Device5* device, ID3D11DeviceContext4* context) {
    if (!device || !context) return false;
    m_d3d.device = device;
    m_d3d.context = context;
    return true;
}

uint32_t webgl_context::create_buffer() {
    uint32_t handle = next_handle();
    m_buffers[handle] = {};
    m_buffers[handle].handle = handle;
    return handle;
}

void webgl_context::delete_buffer(uint32_t buffer) {
    m_buffers.erase(buffer);
}

void webgl_context::bind_buffer(gl_enum target, uint32_t buffer) {
    if (target == gl_enum::ARRAY_BUFFER) {
        m_current_array_buffer = buffer;
    } else if (target == gl_enum::ELEMENT_ARRAY_BUFFER) {
        m_current_element_buffer = buffer;
    }
}

void webgl_context::buffer_data(gl_enum target, const void* data, size_t size, gl_enum usage) {
    uint32_t handle = (target == gl_enum::ARRAY_BUFFER) ? m_current_array_buffer : m_current_element_buffer;
    auto it = m_buffers.find(handle);
    if (it == m_buffers.end()) return;
    it->second.byte_length = size;
    it->second.usage = usage;
    it->second.is_uploaded = true;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(size);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = (target == gl_enum::ELEMENT_ARRAY_BUFFER) ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = data;

    Microsoft::WRL::ComPtr<ID3D11Buffer> d3d_buffer;
    if (SUCCEEDED(m_d3d.device->CreateBuffer(&desc, &init_data, &d3d_buffer))) {
        if (target == gl_enum::ELEMENT_ARRAY_BUFFER) {
            m_d3d.current_ib = d3d_buffer;
        }
    }
}

void webgl_context::buffer_sub_data(gl_enum target, size_t offset, const void* data, size_t size) {
    (void)target; (void)offset; (void)data; (void)size;
}

uint32_t webgl_context::create_vertex_array() {
    uint32_t handle = next_handle();
    m_vertex_arrays[handle] = {};
    m_vertex_arrays[handle].handle = handle;
    return handle;
}

void webgl_context::delete_vertex_array(uint32_t vao) {
    m_vertex_arrays.erase(vao);
}

void webgl_context::bind_vertex_array(uint32_t vao) {
    m_current_vao = vao;
}

void webgl_context::vertex_attrib_pointer(uint32_t index, int size, gl_enum type, bool normalized, int stride, const void* offset) {
    (void)type; (void)normalized; (void)stride; (void)offset;
    auto it = m_vertex_arrays.find(m_current_vao);
    if (it != m_vertex_arrays.end()) {
        it->second.attribs.push_back({index, size});
    }
}

void webgl_context::enable_vertex_attrib_array(uint32_t index) { (void)index; }
void webgl_context::disable_vertex_attrib_array(uint32_t index) { (void)index; }

uint32_t webgl_context::create_texture() {
    uint32_t handle = next_handle();
    m_textures[handle] = {};
    m_textures[handle].handle = handle;
    return handle;
}

void webgl_context::delete_texture(uint32_t texture) {
    m_textures.erase(texture);
}

void webgl_context::active_texture(gl_enum unit) {
    m_state.active_texture = unit;
    m_active_texture_unit = static_cast<uint32_t>(unit) - static_cast<uint32_t>(gl_enum::TEXTURE_2D);
}

void webgl_context::bind_texture(gl_enum target, uint32_t texture) {
    (void)target;
}

void webgl_context::tex_image_2d(gl_enum target, int level, gl_enum internal_format, int width, int height, int border, gl_enum format, gl_enum type, const void* pixels) {
    auto it = m_textures.find(/* todo: track current texture */ 0);
    if (it != m_textures.end()) {
        it->second.width = static_cast<uint32_t>(width);
        it->second.height = static_cast<uint32_t>(height);
        it->second.internal_format = internal_format;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = pixels;
    init_data.SysMemPitch = static_cast<UINT>(width * 4);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (SUCCEEDED(m_d3d.device->CreateTexture2D(&desc, pixels ? &init_data : nullptr, &tex))) {
        m_d3d.device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
    }
}

void webgl_context::tex_parameter_i(gl_enum target, gl_enum pname, int param) {
    (void)target; (void)pname; (void)param;
}

void webgl_context::tex_sub_image_2d(gl_enum target, int level, int xoffset, int yoffset, int width, int height, gl_enum format, gl_enum type, const void* pixels) {
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)width; (void)height; (void)format; (void)type; (void)pixels;
}

void webgl_context::tex_image_3d(gl_enum target, int level, gl_enum internal_format, int width, int height, int depth, int border, gl_enum format, gl_enum type, const void* pixels) {
    (void)target; (void)level; (void)internal_format; (void)width; (void)height; (void)depth; (void)border; (void)format; (void)type; (void)pixels;
}

void webgl_context::tex_sub_image_3d(gl_enum target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, gl_enum format, gl_enum type, const void* pixels) {
    (void)target; (void)level; (void)xoffset; (void)yoffset; (void)zoffset; (void)width; (void)height; (void)depth; (void)format; (void)type; (void)pixels;
}

void webgl_context::tex_storage_2d(gl_enum target, int levels, gl_enum internal_format, int width, int height) {
    (void)target; (void)levels; (void)internal_format; (void)width; (void)height;
}

void webgl_context::tex_storage_3d(gl_enum target, int levels, gl_enum internal_format, int width, int height, int depth) {
    (void)target; (void)levels; (void)internal_format; (void)width; (void)height; (void)depth;
}

uint32_t webgl_context::create_sampler() { return next_handle(); }
void webgl_context::delete_sampler(uint32_t sampler) { (void)sampler; }
void webgl_context::bind_sampler(uint32_t unit, uint32_t sampler) { (void)unit; (void)sampler; }
void webgl_context::sampler_parameter_i(uint32_t sampler, gl_enum pname, int param) { (void)sampler; (void)pname; (void)param; }

uint32_t webgl_context::create_framebuffer() {
    uint32_t handle = next_handle();
    m_framebuffers[handle] = {};
    m_framebuffers[handle].handle = handle;
    return handle;
}

void webgl_context::delete_framebuffer(uint32_t fb) {
    m_framebuffers.erase(fb);
}

void webgl_context::bind_framebuffer(gl_enum target, uint32_t fb) {
    if (target == gl_enum::FRAMEBUFFER || target == gl_enum::DRAW_FRAMEBUFFER || target == gl_enum::READ_FRAMEBUFFER) {
        m_current_framebuffer = fb;
    }
}

void webgl_context::framebuffer_texture_2d(gl_enum target, gl_enum attachment, gl_enum textarget, uint32_t texture, int level) {
    (void)target; (void)attachment; (void)textarget; (void)level;
    auto fb_it = m_framebuffers.find(m_current_framebuffer);
    auto tex_it = m_textures.find(texture);
    if (fb_it != m_framebuffers.end() && tex_it != m_textures.end()) {
        fb_it->second.color_attachment = std::make_shared<webgl_texture>(tex_it->second);
        fb_it->second.width = tex_it->second.width;
        fb_it->second.height = tex_it->second.height;
        fb_it->second.is_complete = true;
    }
}

gl_enum webgl_context::check_framebuffer_status(gl_enum target) {
    (void)target;
    return gl_enum::FRAMEBUFFER_COMPLETE;
}

void webgl_context::draw_buffers(const std::vector<gl_enum>& bufs) {
    (void)bufs;
}

uint32_t webgl_context::create_shader(uint32_t type) {
    uint32_t handle = next_handle();
    return handle;
}

void webgl_context::shader_source(uint32_t shader, const std::string& source) {
    (void)shader; (void)source;
}

bool webgl_context::compile_shader(uint32_t shader) {
    (void)shader;
    return true;
}

std::string webgl_context::get_shader_info_log(uint32_t shader) {
    (void)shader;
    return {};
}

uint32_t webgl_context::create_program() {
    return next_handle();
}

void webgl_context::attach_shader(uint32_t program, uint32_t shader) {
    (void)program; (void)shader;
}

bool webgl_context::link_program(uint32_t program) {
    (void)program;
    return true;
}

std::string webgl_context::get_program_info_log(uint32_t program) {
    (void)program;
    return {};
}

void webgl_context::use_program(uint32_t program) {
    m_current_program = program;
}

void webgl_context::delete_program(uint32_t program) {
    (void)program;
}

int webgl_context::get_uniform_location(uint32_t program, const std::string& name) {
    (void)program; (void)name;
    return 0;
}

int webgl_context::get_attrib_location(uint32_t program, const std::string& name) {
    (void)program; (void)name;
    return 0;
}

void webgl_context::uniform_1i(int location, int v) { (void)location; (void)v; }
void webgl_context::uniform_1f(int location, float v) { (void)location; (void)v; }
void webgl_context::uniform_2f(int location, float v0, float v1) { (void)location; (void)v0; (void)v1; }
void webgl_context::uniform_3f(int location, float v0, float v1, float v2) { (void)location; (void)v0; (void)v1; (void)v2; }
void webgl_context::uniform_4f(int location, float v0, float v1, float v2, float v3) { (void)location; (void)v0; (void)v1; (void)v2; (void)v3; }
void webgl_context::uniform_matrix_4fv(int location, bool transpose, const float* value) { (void)location; (void)transpose; (void)value; }

void webgl_context::draw_arrays(gl_enum mode, int first, int count) {
    m_d3d.context->Draw(count, static_cast<UINT>(first));
}

void webgl_context::draw_elements(gl_enum mode, int count, gl_enum type, const void* offset) {
    m_d3d.context->DrawIndexed(static_cast<UINT>(count), 0, 0);
}

void webgl_context::draw_arrays_instanced(gl_enum mode, int first, int count, int instance_count) {
    m_d3d.context->DrawInstanced(static_cast<UINT>(count), static_cast<UINT>(instance_count), static_cast<UINT>(first), 0);
}

void webgl_context::draw_elements_instanced(gl_enum mode, int count, gl_enum type, const void* offset, int instance_count) {
    m_d3d.context->DrawIndexedInstanced(static_cast<UINT>(count), static_cast<UINT>(instance_count), 0, 0, 0);
}

void webgl_context::vertex_attrib_divisor(uint32_t index, uint32_t divisor) {
    (void)index; (void)divisor;
}

void webgl_context::begin_transform_feedback(gl_enum primitive_mode) { (void)primitive_mode; }
void webgl_context::end_transform_feedback() {}
void webgl_context::bind_buffer_base(gl_enum target, uint32_t index, uint32_t buffer) { (void)target; (void)index; (void)buffer; }

void webgl_context::enable(gl_enum cap) {
    switch (cap) {
        case gl_enum::ENABLE_BLEND: m_state.blend_enabled = true; break;
        case gl_enum::ENABLE_DEPTH_TEST: m_state.depth_test_enabled = true; break;
        case gl_enum::ENABLE_CULL_FACE: m_state.cull_face_enabled = true; break;
        case gl_enum::ENABLE_SCISSOR_TEST: m_state.scissor_test_enabled = true; break;
        default: break;
    }
}

void webgl_context::disable(gl_enum cap) {
    switch (cap) {
        case gl_enum::ENABLE_BLEND: m_state.blend_enabled = false; break;
        case gl_enum::ENABLE_DEPTH_TEST: m_state.depth_test_enabled = false; break;
        case gl_enum::ENABLE_CULL_FACE: m_state.cull_face_enabled = false; break;
        case gl_enum::ENABLE_SCISSOR_TEST: m_state.scissor_test_enabled = false; break;
        default: break;
    }
}

void webgl_context::blend_func(gl_enum sfactor, gl_enum dfactor) {
    m_state.blend_src = sfactor;
    m_state.blend_dst = dfactor;
}

void webgl_context::blend_equation(gl_enum mode) {
    m_state.blend_equation = mode;
}

void webgl_context::clear_color(float r, float g, float b, float a) {
    m_state.clear_color[0] = r;
    m_state.clear_color[1] = g;
    m_state.clear_color[2] = b;
    m_state.clear_color[3] = a;
}

void webgl_context::clear(gl_enum mask) {
    float color[4] = { m_state.clear_color[0], m_state.clear_color[1], m_state.clear_color[2], m_state.clear_color[3] };
    if (static_cast<uint32_t>(mask) & static_cast<uint32_t>(gl_enum::COLOR_BUFFER_BIT)) {
        m_d3d.context->ClearRenderTargetView(nullptr, color);
    }
    if (static_cast<uint32_t>(mask) & static_cast<uint32_t>(gl_enum::DEPTH_BUFFER_BIT)) {
        m_d3d.context->ClearDepthStencilView(nullptr, D3D11_CLEAR_DEPTH, m_state.clear_depth, 0);
    }
}

void webgl_context::viewport(int x, int y, int width, int height) {
    m_state.viewport_x = x;
    m_state.viewport_y = y;
    m_state.viewport_w = width;
    m_state.viewport_h = height;
    D3D11_VIEWPORT vp = { static_cast<FLOAT>(x), static_cast<FLOAT>(y), static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0, 1 };
    m_d3d.context->RSSetViewports(1, &vp);
}

void webgl_context::scissor(int x, int y, int width, int height) {
    m_state.scissor_x = x;
    m_state.scissor_y = y;
    m_state.scissor_w = width;
    m_state.scissor_h = height;
}

gl_enum webgl_context::get_error() {
    auto err = m_last_error;
    m_last_error = gl_enum::NO_ERROR;
    return err;
}

} // namespace hre::webgl
