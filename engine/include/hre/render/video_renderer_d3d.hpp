#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <wrl/client.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>

namespace hre::render {

enum class video_output_format {
    NV12,
    YUV420,
    RGB32
};

struct video_render_config {
    uint32_t width = 0;
    uint32_t height = 0;
    bool vsync_enabled = true;
    bool fullscreen = false;
    HWND output_window = nullptr;
    video_output_format format = video_output_format::NV12;
    float display_aspect_ratio = 0.0f;
};

struct overlay_quad {
    float x, y, w, h;
    uint32_t color;
    float opacity;
    int z_order;
    bool visible;
    std::vector<uint8_t> texture_data; // RGBA
    int tex_width, tex_height;
};

struct subtitle_rect {
    float x, y, w, h;
    std::wstring text;
    uint32_t color;
    float font_size;
    int64_t start_time_ns;
    int64_t end_time_ns;
};

class video_renderer_d3d {
public:
    video_renderer_d3d();
    ~video_renderer_d3d();

    bool init(ID3D11Device5* device, IDXGISwapChain4* swapchain,
              const video_render_config& cfg);
    void shutdown();

    bool resize(uint32_t width, uint32_t height);

    // Texture upload
    bool upload_nv12_texture(const uint8_t* y_plane, int y_stride,
                              const uint8_t* uv_plane, int uv_stride);
    bool upload_yuv_texture(const uint8_t* y, const uint8_t* u,
                             const uint8_t* v, int y_stride,
                             int uv_stride);
    bool upload_rgb_texture(const uint8_t* rgb, int stride);

    // Presentation
    bool render();
    bool present();

    // Overlay compositing
    int add_overlay(const overlay_quad& quad);
    void remove_overlay(int id);
    void update_overlay(int id, const overlay_quad& quad);
    void clear_overlays();

    // Subtitles
    void set_subtitles(const std::vector<subtitle_rect>& subs);
    void clear_subtitles();

    // Controls overlay
    void show_controls(bool show);
    void set_controls_state(float volume, bool paused, int64_t position_ns,
                             int64_t duration_ns);

    // Configuration
    void set_vsync(bool enabled);
    void set_brightness(float val);
    void set_contrast(float val);
    void set_saturation(float val);

    ID3D11ShaderResourceView* video_srv() const { return m_video_srv.Get(); }

private:
    bool create_video_textures(uint32_t width, uint32_t height);
    bool create_vertex_shader();
    bool create_pixel_shader_yuv();
    bool create_sampler_state();
    bool create_blend_state();

    void render_overlays();
    void render_subtitles();
    void render_controls();

    Microsoft::WRL::ComPtr<ID3D11Device5> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;

    // Video textures
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_y_texture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_uv_texture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_v_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_video_srv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_y_srv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uv_srv;

    // Shaders
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps_yuv;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps_rgb;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertex_buffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cb;

    // Sampler & blend
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend;

    // Viewport
    D3D11_VIEWPORT m_viewport;

    // State
    video_render_config m_cfg;
    bool m_initialized = false;
    bool m_controls_visible = false;
    float m_brightness = 1.0f, m_contrast = 1.0f, m_saturation = 1.0f;
    float m_volume = 1.0f;
    bool m_paused = false;
    int64_t m_position_ns = 0, m_duration_ns = 0;

    // Overlays
    struct overlay_entry {
        overlay_quad quad;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };
    std::vector<std::pair<int, overlay_entry>> m_overlays;
    int m_next_overlay_id = 1;

    // Subtitles
    std::vector<subtitle_rect> m_subtitles;
};

} // namespace hre::render
