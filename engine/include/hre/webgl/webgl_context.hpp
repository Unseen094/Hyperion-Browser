#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <wrl/client.h>
#include <d3d11_4.h>

namespace hre::webgl {

// ---- GL Constants ---------------------------------------------------------
// Undefine Windows macros that conflict with GL enum names
#ifdef NO_ERROR
#undef NO_ERROR
#endif

enum class gl_enum : uint32_t {
    ARRAY_BUFFER = 0x8892,
    ELEMENT_ARRAY_BUFFER = 0x8893,
    STATIC_DRAW = 0x88E4,
    DYNAMIC_DRAW = 0x88E8,
    STREAM_DRAW = 0x88E0,
    FLOAT = 0x1406,
    INT = 0x1404,
    UNSIGNED_BYTE = 0x1401,
    UNSIGNED_SHORT = 0x1403,
    TRIANGLES = 0x0004,
    TRIANGLE_STRIP = 0x0005,
    TRIANGLE_FAN = 0x0006,
    LINES = 0x0001,
    LINE_STRIP = 0x0003,
    POINTS = 0x0000,
    TEXTURE_2D = 0x0DE1,
    RGBA = 0x1908,
    RGBA8 = 0x8058,
    NEAREST = 0x2600,
    LINEAR = 0x2601,
    CLAMP_TO_EDGE = 0x812F,
    REPEAT = 0x2901,
    COLOR_BUFFER_BIT = 0x4000,
    DEPTH_BUFFER_BIT = 0x0100,
    STENCIL_BUFFER_BIT = 0x0400,
    FRAGMENT_SHADER = 0x8B30,
    VERTEX_SHADER = 0x8B31,
    COMPILE_STATUS = 0x8B81,
    LINK_STATUS = 0x8B82,
    NO_ERROR = 0,
    INVALID_ENUM = 0x0500,
    INVALID_VALUE = 0x0501,
    INVALID_OPERATION = 0x0502,
    OUT_OF_MEMORY = 0x0505,
    MAX_TEXTURE_SIZE = 0x0D33,
    MAX_VERTEX_ATTRIBS = 0x8869,
    MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D,
    MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0x8B4C,
    MAX_TEXTURE_IMAGE_UNITS = 0x8872,
    MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD,
    MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB,
    MAX_VARYING_VECTORS = 0x8DFC,

    // WebGL 2.0 specific
    R32F = 0x822E,
    RG32F = 0x8230,
    RGB32F = 0x8815,
    RGBA32F = 0x8814,
    R8 = 0x8229,
    RG8 = 0x822B,
    RED = 0x1903,
    RG = 0x8227,
    READ_BUFFER = 0x0C02,
    UNPACK_ROW_LENGTH = 0x0CF2,
    UNPACK_SKIP_ROWS = 0x0CF3,
    UNPACK_SKIP_PIXELS = 0x0CF4,
    PACK_ROW_LENGTH = 0x0D02,
    PACK_SKIP_ROWS = 0x0D03,
    PACK_SKIP_PIXELS = 0x0D04,
    TEXTURE_MIN_FILTER = 0x2801,
    TEXTURE_MAG_FILTER = 0x2800,
    TEXTURE_WRAP_S = 0x2802,
    TEXTURE_WRAP_T = 0x2803,
    TEXTURE_WRAP_R = 0x8072,
    TEXTURE_3D = 0x806F,
    TEXTURE_BINDING_3D = 0x806A,
    TEXTURE_COMPARE_MODE = 0x884C,
    TEXTURE_COMPARE_FUNC = 0x884D,
    COMPARE_REF_TO_TEXTURE = 0x884E,
    SAMPLER_2D = 0x8B5E,
    SAMPLER_3D = 0x8B5F,
    SAMPLER_CUBE = 0x8B60,
    SAMPLER_2D_SHADOW = 0x8B62,
    SAMPLER_2D_ARRAY = 0x8DC1,
    SAMPLER_2D_ARRAY_SHADOW = 0x8DC4,
    SAMPLER_CUBE_SHADOW = 0x8DC5,
    INT_2_10_10_10_REV = 0x8D9F,
    UNSIGNED_INT_2_10_10_10_REV = 0x8368,
    UNSIGNED_INT_10F_11F_11F_REV = 0x8C3B,
    UNSIGNED_INT_5_9_9_9_REV = 0x8C3E,
    FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD,
    HALF_FLOAT = 0x140B,
    DEPTH_COMPONENT24 = 0x81A6,
    DEPTH_COMPONENT32F = 0x8CAC,
    DEPTH24_STENCIL8 = 0x88F0,
    DEPTH32F_STENCIL8 = 0x8CAD,
    STENCIL_INDEX8 = 0x8D48,
    DRAW_FRAMEBUFFER = 0x8CA9,
    READ_FRAMEBUFFER = 0x8CA8,
    FRAMEBUFFER = 0x8D40,
    COLOR_ATTACHMENT0 = 0x8CE0,
    DEPTH_ATTACHMENT = 0x8D00,
    STENCIL_ATTACHMENT = 0x8D20,
    DEPTH_STENCIL_ATTACHMENT = 0x821A,
    FRAMEBUFFER_COMPLETE = 0x8CD5,
    FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6,
    FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7,
    FRAMEBUFFER_INCOMPLETE_DIMENSIONS = 0x8CD9,
    FRAMEBUFFER_UNSUPPORTED = 0x8CDD,
    VERTEX_ARRAY_BINDING = 0x85B5,
    TRANSFORM_FEEDBACK_BUFFER = 0x8C8E,
    TRANSFORM_FEEDBACK = 0x8E22,
    TRANSFORM_FEEDBACK_PAUSED = 0x8E23,
    TRANSFORM_FEEDBACK_ACTIVE = 0x8E24,
    TRANSFORM_FEEDBACK_BINDING = 0x8E25,
    TRANSFORM_FEEDBACK_BUFFER_BINDING = 0x8C8F,
    TRANSFORM_FEEDBACK_BUFFER_START = 0x8C84,
    TRANSFORM_FEEDBACK_BUFFER_SIZE = 0x8C85,
    TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 0x8C88,
    RASTERIZER_DISCARD = 0x8C89,
    MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS = 0x8C80,
    MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS = 0x8C8A,
    MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS = 0x8C8B,
    INTERLEAVED_ATTRIBS = 0x8C8C,
    SEPARATE_ATTRIBS = 0x8C8D,
    UNIFORM_BUFFER = 0x8A11,
    UNIFORM_BUFFER_BINDING = 0x8A28,
    UNIFORM_BUFFER_START = 0x8A29,
    UNIFORM_BUFFER_SIZE = 0x8A2A,
    MAX_UNIFORM_BUFFER_BINDINGS = 0x8A2F,
    MAX_UNIFORM_BLOCK_SIZE = 0x8A30,
    MAX_COMBINED_UNIFORM_BLOCKS = 0x8A2E,
    MAX_VERTEX_UNIFORM_BLOCKS = 0x8A2B,
    MAX_FRAGMENT_UNIFORM_BLOCKS = 0x8A2D,
    UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER = 0x8A44,
    UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER = 0x8A46,
    UNIFORM_BLOCK_DATA_SIZE = 0x8A40,
    UNIFORM_BLOCK_ACTIVE_UNIFORMS = 0x8A42,
    UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES = 0x8A43,
    UNIFORM_BLOCK_BINDING = 0x8A3F,
    DRAW_BUFFER0 = 0x8825,
    DRAW_BUFFER1 = 0x8826,
    DRAW_BUFFER2 = 0x8827,
    DRAW_BUFFER3 = 0x8828,
    MAX_DRAW_BUFFERS = 0x8824,
    MAX_COLOR_ATTACHMENTS = 0x8CDF,
    INVALID_FRAMEBUFFER_OPERATION = 0x0506,
    ENABLE_BLEND = 0x0BE2,
    ENABLE_DEPTH_TEST = 0x0B71,
    ENABLE_CULL_FACE = 0x0B44,
    ENABLE_SCISSOR_TEST = 0x0C11
};

// ---- GLSL ES 3.0 Shader ---------------------------------------------------

struct glsl_es_shader {
    uint32_t handle = 0;
    uint32_t type = 0; // GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
    std::string source;
    bool compiled = false;
    std::string compile_log;

    void set_source(const std::string& src) { source = src; }
    bool compile();
};

struct glsl_es_program {
    uint32_t handle = 0;
    std::shared_ptr<glsl_es_shader> vert_shader;
    std::shared_ptr<glsl_es_shader> frag_shader;
    bool linked = false;
    std::string link_log;

    void attach(const std::shared_ptr<glsl_es_shader>& shader);
    bool link();
    void use();

    int get_uniform_location(const std::string& name);
    int get_attrib_location(const std::string& name);

    void uniform_1i(int location, int v);
    void uniform_1f(int location, float v);
    void uniform_2f(int location, float v0, float v1);
    void uniform_3f(int location, float v0, float v1, float v2);
    void uniform_4f(int location, float v0, float v1, float v2, float v3);
    void uniform_matrix_4fv(int location, bool transpose, const float* value);
};

// ---- Buffer Objects -------------------------------------------------------

struct webgl_buffer {
    uint32_t handle = 0;
    gl_enum target = gl_enum::ARRAY_BUFFER;
    gl_enum usage = gl_enum::STATIC_DRAW;
    size_t byte_length = 0;
    bool is_uploaded = false;
};

struct webgl_vertex_array {
    uint32_t handle = 0;
    std::vector<std::pair<uint32_t, int>> attribs; // location, size
};

// ---- Texture --------------------------------------------------------------

struct webgl_texture {
    uint32_t handle = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    gl_enum internal_format = gl_enum::RGBA8;
    gl_enum min_filter = gl_enum::LINEAR;
    gl_enum mag_filter = gl_enum::LINEAR;
    gl_enum wrap_s = gl_enum::REPEAT;
    gl_enum wrap_t = gl_enum::REPEAT;
    bool is_power_of_2 = false;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
};

// ---- Framebuffer ----------------------------------------------------------

struct webgl_framebuffer {
    uint32_t handle = 0;
    std::shared_ptr<webgl_texture> color_attachment;
    std::shared_ptr<webgl_texture> depth_stencil_attachment;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_complete = false;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
};

// ---- WebGL State Machine --------------------------------------------------

struct webgl_state {
    gl_enum active_texture = gl_enum::TEXTURE_2D;
    gl_enum blend_src = static_cast<gl_enum>(0x0302);    // ONE
    gl_enum blend_dst = static_cast<gl_enum>(0x0303);    // ZERO
    gl_enum blend_equation = static_cast<gl_enum>(0x8006); // FUNC_ADD
    bool blend_enabled = false;
    bool depth_test_enabled = false;
    bool cull_face_enabled = false;
    bool scissor_test_enabled = false;
    bool stencil_test_enabled = false;
    gl_enum cull_face_mode = static_cast<gl_enum>(0x0405); // BACK
    float clear_color[4] = {0, 0, 0, 0};
    float clear_depth = 1.0f;
    int viewport_x = 0, viewport_y = 0, viewport_w = 0, viewport_h = 0;
    int scissor_x = 0, scissor_y = 0, scissor_w = 0, scissor_h = 0;
};

// ---- WebGL 2.0 Context ----------------------------------------------------

class webgl_context {
public:
    webgl_context();
    ~webgl_context();

    bool init(ID3D11Device5* device, ID3D11DeviceContext4* context);

    uint32_t create_buffer();
    void delete_buffer(uint32_t buffer);
    void bind_buffer(gl_enum target, uint32_t buffer);
    void buffer_data(gl_enum target, const void* data, size_t size, gl_enum usage);
    void buffer_sub_data(gl_enum target, size_t offset, const void* data, size_t size);

    uint32_t create_vertex_array();
    void delete_vertex_array(uint32_t vao);
    void bind_vertex_array(uint32_t vao);
    void vertex_attrib_pointer(uint32_t index, int size, gl_enum type, bool normalized, int stride, const void* offset);
    void enable_vertex_attrib_array(uint32_t index);
    void disable_vertex_attrib_array(uint32_t index);

    uint32_t create_texture();
    void delete_texture(uint32_t texture);
    void active_texture(gl_enum unit);
    void bind_texture(gl_enum target, uint32_t texture);
    void tex_image_2d(gl_enum target, int level, gl_enum internal_format, int width, int height, int border, gl_enum format, gl_enum type, const void* pixels);
    void tex_parameter_i(gl_enum target, gl_enum pname, int param);
    void tex_sub_image_2d(gl_enum target, int level, int xoffset, int yoffset, int width, int height, gl_enum format, gl_enum type, const void* pixels);

    // WebGL 2.0: 3D textures
    void tex_image_3d(gl_enum target, int level, gl_enum internal_format, int width, int height, int depth, int border, gl_enum format, gl_enum type, const void* pixels);
    void tex_sub_image_3d(gl_enum target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, gl_enum format, gl_enum type, const void* pixels);
    void tex_storage_2d(gl_enum target, int levels, gl_enum internal_format, int width, int height);
    void tex_storage_3d(gl_enum target, int levels, gl_enum internal_format, int width, int height, int depth);

    // WebGL 2.0: Sampler objects
    uint32_t create_sampler();
    void delete_sampler(uint32_t sampler);
    void bind_sampler(uint32_t unit, uint32_t sampler);
    void sampler_parameter_i(uint32_t sampler, gl_enum pname, int param);

    uint32_t create_framebuffer();
    void delete_framebuffer(uint32_t fb);
    void bind_framebuffer(gl_enum target, uint32_t fb);
    void framebuffer_texture_2d(gl_enum target, gl_enum attachment, gl_enum textarget, uint32_t texture, int level);
    gl_enum check_framebuffer_status(gl_enum target);

    // WebGL 2.0: Multiple render targets
    void draw_buffers(const std::vector<gl_enum>& bufs);

    uint32_t create_shader(uint32_t type);
    void shader_source(uint32_t shader, const std::string& source);
    bool compile_shader(uint32_t shader);
    std::string get_shader_info_log(uint32_t shader);

    uint32_t create_program();
    void attach_shader(uint32_t program, uint32_t shader);
    bool link_program(uint32_t program);
    std::string get_program_info_log(uint32_t program);
    void use_program(uint32_t program);
    void delete_program(uint32_t program);

    int get_uniform_location(uint32_t program, const std::string& name);
    int get_attrib_location(uint32_t program, const std::string& name);

    void uniform_1i(int location, int v);
    void uniform_1f(int location, float v);
    void uniform_2f(int location, float v0, float v1);
    void uniform_3f(int location, float v0, float v1, float v2);
    void uniform_4f(int location, float v0, float v1, float v2, float v3);
    void uniform_matrix_4fv(int location, bool transpose, const float* value);

    void draw_arrays(gl_enum mode, int first, int count);
    void draw_elements(gl_enum mode, int count, gl_enum type, const void* offset);

    // WebGL 2.0: Instanced rendering
    void draw_arrays_instanced(gl_enum mode, int first, int count, int instance_count);
    void draw_elements_instanced(gl_enum mode, int count, gl_enum type, const void* offset, int instance_count);
    void vertex_attrib_divisor(uint32_t index, uint32_t divisor);

    // WebGL 2.0: Transform feedback
    void begin_transform_feedback(gl_enum primitive_mode);
    void end_transform_feedback();
    void bind_buffer_base(gl_enum target, uint32_t index, uint32_t buffer);

    // State
    void enable(gl_enum cap);
    void disable(gl_enum cap);
    void blend_func(gl_enum sfactor, gl_enum dfactor);
    void blend_equation(gl_enum mode);
    void clear_color(float r, float g, float b, float a);
    void clear(gl_enum mask);
    void viewport(int x, int y, int width, int height);
    void scissor(int x, int y, int width, int height);

    gl_enum get_error();

    uint32_t next_handle() { return ++m_next_handle; }

private:
    struct d3d_state {
        Microsoft::WRL::ComPtr<ID3D11Device5> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> current_vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> current_ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> current_layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> current_ib;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> current_rasterizer;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> current_ds;
        Microsoft::WRL::ComPtr<ID3D11BlendState> current_blend;
    } m_d3d;

    std::map<uint32_t, webgl_buffer> m_buffers;
    std::map<uint32_t, webgl_vertex_array> m_vertex_arrays;
    std::map<uint32_t, webgl_texture> m_textures;
    std::map<uint32_t, webgl_framebuffer> m_framebuffers;

    uint32_t m_current_vao = 0;
    uint32_t m_current_program = 0;
    uint32_t m_current_array_buffer = 0;
    uint32_t m_current_element_buffer = 0;
    uint32_t m_current_framebuffer = 0;
    uint32_t m_active_texture_unit = 0;

    webgl_state m_state;
    uint32_t m_next_handle = 1;
    gl_enum m_last_error = gl_enum::NO_ERROR;
};

} // namespace hre::webgl
