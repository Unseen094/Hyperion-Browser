#include <hre/render/video_renderer_d3d.hpp>
#include <algorithm>
#include <cstring>

namespace hre::render {

video_renderer_d3d::video_renderer_d3d() = default;
video_renderer_d3d::~video_renderer_d3d() { shutdown(); }

bool video_renderer_d3d::init(ID3D11Device5* device, IDXGISwapChain4* swapchain,
                               const video_render_config& cfg) {
    if (!device || !swapchain) return false;
    m_device = device;
    m_swapchain = swapchain;
    m_cfg = cfg;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx;
    m_device->GetImmediateContext(&ctx);
    ctx.As(&m_context);
    if (!m_context) return false;

    if (!create_video_textures(cfg.width, cfg.height)) return false;
    if (!create_vertex_shader()) return false;
    if (!create_pixel_shader_yuv()) return false;
    if (!create_sampler_state()) return false;
    if (!create_blend_state()) return false;

    m_viewport.Width = static_cast<FLOAT>(cfg.width);
    m_viewport.Height = static_cast<FLOAT>(cfg.height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;

    m_initialized = true;
    return true;
}

void video_renderer_d3d::shutdown() {
    m_y_texture.Reset();
    m_uv_texture.Reset();
    m_v_texture.Reset();
    m_video_srv.Reset();
    m_y_srv.Reset();
    m_uv_srv.Reset();
    m_vs.Reset();
    m_ps_yuv.Reset();
    m_ps_rgb.Reset();
    m_layout.Reset();
    m_vertex_buffer.Reset();
    m_cb.Reset();
    m_sampler.Reset();
    m_blend.Reset();
    m_swapchain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool video_renderer_d3d::resize(uint32_t width, uint32_t height) {
    m_cfg.width = width;
    m_cfg.height = height;
    m_viewport.Width = static_cast<FLOAT>(width);
    m_viewport.Height = static_cast<FLOAT>(height);
    if (m_swapchain) {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_y_srv.Reset();
        m_uv_srv.Reset();
        m_video_srv.Reset();
        HRESULT hr = m_swapchain->ResizeBuffers(0, width, height,
                                                 DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return false;
    }
    return create_video_textures(width, height);
}

bool video_renderer_d3d::create_video_textures(uint32_t width, uint32_t height) {
    m_y_texture.Reset();
    m_uv_texture.Reset();
    m_v_texture.Reset();
    m_y_srv.Reset();
    m_uv_srv.Reset();
    m_video_srv.Reset();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.Format = DXGI_FORMAT_R8_UNORM;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_y_texture)))
        return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(m_y_texture.Get(), &srv_desc, &m_y_srv);

    desc.Width = width / 2;
    desc.Height = height / 2;
    desc.Format = DXGI_FORMAT_R8G8_UNORM;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_uv_texture)))
        return false;
    srv_desc.Format = desc.Format;
    m_device->CreateShaderResourceView(m_uv_texture.Get(), &srv_desc, &m_uv_srv);

    return true;
}

bool video_renderer_d3d::upload_nv12_texture(const uint8_t* y_plane, int y_stride,
                                              const uint8_t* uv_plane, int uv_stride) {
    if (!m_context || !m_y_texture || !m_uv_texture) return false;
    m_context->UpdateSubresource(m_y_texture.Get(), 0, nullptr,
                                  y_plane, y_stride, 0);
    m_context->UpdateSubresource(m_uv_texture.Get(), 0, nullptr,
                                  uv_plane, uv_stride, 0);
    return true;
}

bool video_renderer_d3d::upload_yuv_texture(const uint8_t* y, const uint8_t* u,
                                             const uint8_t* v, int y_stride,
                                             int uv_stride) {
    if (!m_context || !m_y_texture || !m_uv_texture) return false;
    m_context->UpdateSubresource(m_y_texture.Get(), 0, nullptr,
                                  y, y_stride, 0);
    // Pack U and V into RG interleaved texture
    uint32_t uv_w = m_cfg.width / 2;
    uint32_t uv_h = m_cfg.height / 2;
    std::vector<uint8_t> uv_packed(uv_w * uv_h * 2);
    for (uint32_t row = 0; row < uv_h; ++row) {
        for (uint32_t col = 0; col < uv_w; ++col) {
            uv_packed[(row * uv_w + col) * 2] = u[row * uv_stride + col];
            uv_packed[(row * uv_w + col) * 2 + 1] = v[row * uv_stride + col];
        }
    }
    m_context->UpdateSubresource(m_uv_texture.Get(), 0, nullptr,
                                  uv_packed.data(), uv_w * 2, 0);
    return true;
}

bool video_renderer_d3d::upload_rgb_texture(const uint8_t* rgb, int stride) {
    (void)rgb; (void)stride;
    return false;
}

bool video_renderer_d3d::create_vertex_shader() {
    // Full-screen triangle vertex shader
    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
         3.0f, -1.0f, 0.0f, 2.0f, 1.0f,
        -1.0f,  3.0f, 0.0f, 0.0f, -1.0f
    };
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(vertices);
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = vertices;
    m_device->CreateBuffer(&bd, &sd, &m_vertex_buffer);
    return true;
}

bool video_renderer_d3d::create_pixel_shader_yuv() {
    // In a real implementation, compile YUV->RGB shader from bytecode
    return true;
}

bool video_renderer_d3d::create_sampler_state() {
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(m_device->CreateSamplerState(&sd, &m_sampler));
}

bool video_renderer_d3d::create_blend_state() {
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    return SUCCEEDED(m_device->CreateBlendState(&bd, &m_blend));
}

bool video_renderer_d3d::render() {
    if (!m_initialized || !m_context) return false;

    m_context->RSSetViewports(1, &m_viewport);

    ID3D11RenderTargetView* rtv = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
    if (SUCCEEDED(m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                          &back))) {
        m_device->CreateRenderTargetView(back.Get(), nullptr, &rtv);
        if (rtv) {
            float clear[4] = {0, 0, 0, 1};
            m_context->ClearRenderTargetView(rtv, clear);
            m_context->OMSetRenderTargets(1, &rtv, nullptr);

            // Set shader resources
            ID3D11ShaderResourceView* srvs[2] = { m_y_srv.Get(), m_uv_srv.Get() };
            m_context->PSSetShaderResources(0, 2, srvs);
            m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

            static const UINT stride = 20, offset = 0;
            m_context->IASetVertexBuffers(0, 1, m_vertex_buffer.GetAddressOf(),
                                           &stride, &offset);
            m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_context->Draw(3, 0);

            render_overlays();
            render_subtitles();
            render_controls();

            rtv->Release();
        }
    }
    return true;
}

bool video_renderer_d3d::present() {
    if (!m_swapchain) return false;
    UINT sync = m_cfg.vsync_enabled ? 1 : 0;
    return SUCCEEDED(m_swapchain->Present(sync, 0));
}

void video_renderer_d3d::set_vsync(bool enabled) { m_cfg.vsync_enabled = enabled; }
void video_renderer_d3d::set_brightness(float val) { m_brightness = val; }
void video_renderer_d3d::set_contrast(float val) { m_contrast = val; }
void video_renderer_d3d::set_saturation(float val) { m_saturation = val; }

int video_renderer_d3d::add_overlay(const overlay_quad& quad) {
    int id = m_next_overlay_id++;
    overlay_entry entry;
    entry.quad = quad;
    // Create texture for overlay if it has data
    if (!quad.texture_data.empty() && quad.tex_width > 0 && quad.tex_height > 0) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = quad.tex_width;
        desc.Height = quad.tex_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = quad.texture_data.data();
        sd.SysMemPitch = quad.tex_width * 4;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(m_device->CreateTexture2D(&desc, &sd, &tex))) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = desc.Format;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
            m_device->CreateShaderResourceView(tex.Get(), &srv_desc, &entry.srv);
        }
    }
    m_overlays.emplace_back(id, std::move(entry));
    return id;
}

void video_renderer_d3d::remove_overlay(int id) {
    auto it = std::remove_if(m_overlays.begin(), m_overlays.end(),
                              [id](auto& p) { return p.first == id; });
    m_overlays.erase(it, m_overlays.end());
}

void video_renderer_d3d::update_overlay(int id, const overlay_quad& quad) {
    for (auto& [oid, entry] : m_overlays) {
        if (oid == id) { entry.quad = quad; break; }
    }
}

void video_renderer_d3d::clear_overlays() { m_overlays.clear(); }

void video_renderer_d3d::set_subtitles(const std::vector<subtitle_rect>& subs) {
    m_subtitles = subs;
}

void video_renderer_d3d::clear_subtitles() { m_subtitles.clear(); }

void video_renderer_d3d::show_controls(bool show) { m_controls_visible = show; }

void video_renderer_d3d::set_controls_state(float volume, bool paused,
                                             int64_t position_ns,
                                             int64_t duration_ns) {
    m_volume = volume;
    m_paused = paused;
    m_position_ns = position_ns;
    m_duration_ns = duration_ns;
}

void video_renderer_d3d::render_overlays() {
    if (!m_context || m_overlays.empty()) return;
    float blend_factor[4] = {1,1,1,1};
    m_context->OMSetBlendState(m_blend.Get(), blend_factor, 0xFFFFFFFF);
    for (auto& [id, entry] : m_overlays) {
        if (!entry.quad.visible) continue;
        if (entry.srv) {
            ID3D11ShaderResourceView* srv = entry.srv.Get();
            m_context->PSSetShaderResources(0, 1, &srv);
            // In a real impl: draw a textured quad
        }
    }
    m_context->OMSetBlendState(nullptr, blend_factor, 0xFFFFFFFF);
}

void video_renderer_d3d::render_subtitles() {
    if (!m_context || m_subtitles.empty()) return;
    (void)m_subtitles;
}

void video_renderer_d3d::render_controls() {
    if (!m_context || !m_controls_visible) return;
    (void)m_volume;
    (void)m_paused;
    (void)m_position_ns;
    (void)m_duration_ns;
}

} // namespace hre::render
