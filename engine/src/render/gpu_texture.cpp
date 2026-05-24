#define NOMINMAX
#include <hre/render/gpu_texture.hpp>
#include <cmath>

namespace hre::render {

gpu_texture_manager::gpu_texture_manager() = default;

gpu_texture_manager::~gpu_texture_manager() {
    flush();
}

bool gpu_texture_manager::init(ID3D11Device5* device, ID2D1Device5* d2d_device) {
    if (!device || !d2d_device) {
        return false;
    }

    m_d3d_device = device;
    m_d2d_device = d2d_device;

    device->GetImmediateContext3(&m_d3d_context);

    HRESULT hr = d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &m_d2d_context);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

gpu_texture_info* gpu_texture_manager::create_texture(const std::wstring& id, uint32_t width, uint32_t height, bool render_target) {
    if (m_textures.find(id) != m_textures.end()) {
        return &m_textures[id];
    }

    if (!m_d3d_device) {
        return nullptr;
    }

    gpu_texture_info info;
    info.width = width;
    info.height = height;
    info.is_render_target = render_target;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (render_target) {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    HRESULT hr = m_d3d_device->CreateTexture2D(&desc, nullptr, &info.texture);
    if (FAILED(hr)) {
        return nullptr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;

    hr = m_d3d_device->CreateShaderResourceView(info.texture.Get(), &srv_desc, &info.shader_view);
    if (FAILED(hr)) {
        return nullptr;
    }

    if (!create_d2d_bitmap_for_texture(&info)) {
        return nullptr;
    }

    auto result = m_textures.emplace(id, std::move(info));
    return &result.first->second;
}

bool gpu_texture_manager::create_d2d_bitmap_for_texture(gpu_texture_info* info) {
    if (!info || !info->texture || !m_d2d_context) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = info->texture.As(&surface);
    if (FAILED(hr)) {
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 bitmap_props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f
    );

    hr = m_d2d_context->CreateBitmapFromDxgiSurface(surface.Get(), bitmap_props, &info->d2d_bitmap);
    return SUCCEEDED(hr);
}

gpu_texture_info* gpu_texture_manager::get_texture(const std::wstring& id) {
    auto it = m_textures.find(id);
    if (it != m_textures.end()) {
        return &it->second;
    }
    return nullptr;
}

void gpu_texture_manager::release_texture(const std::wstring& id) {
    m_textures.erase(id);
}

void gpu_texture_manager::mark_dirty(const std::wstring& id) {
    auto it = m_textures.find(id);
    if (it != m_textures.end()) {
        it->second.is_dirty = true;
    }
}

void gpu_texture_manager::clear_dirty_flags() {
    for (auto& [id, info] : m_textures) {
        info.is_dirty = false;
    }
}

bool gpu_texture_manager::upload_data(const std::wstring& id, const void* data, size_t size, uint32_t pitch) {
    auto it = m_textures.find(id);
    if (it == m_textures.end() || !m_d3d_context) {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = m_d3d_context->Map(it->second.texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        return false;
    }

    const uint8_t* src = static_cast<const uint8_t*>(data);
    uint8_t* dst = static_cast<uint8_t*>(mapped.pData);

    for (uint32_t y = 0; y < it->second.height; ++y) {
        std::memcpy(dst + y * mapped.RowPitch, src + y * pitch, std::min(pitch, mapped.RowPitch));
    }

    m_d3d_context->Unmap(it->second.texture.Get(), 0);
    it->second.is_dirty = true;

    return true;
}

void gpu_texture_manager::flush() {
    if (m_d3d_context) {
        m_d3d_context->Flush();
    }
    m_textures.clear();
}

gpu_transform::gpu_transform() : m_matrix(D2D1::Matrix3x2F::Identity()) {}

void gpu_transform::identity() {
    m_matrix = D2D1::Matrix3x2F::Identity();
}

void gpu_transform::translate(float tx, float ty) {
    m_matrix = D2D1::Matrix3x2F::Translation(tx, ty) * m_matrix;
}

void gpu_transform::scale(float sx, float sy) {
    m_matrix = D2D1::Matrix3x2F::Scale(sx, sy) * m_matrix;
}

void gpu_transform::rotate(float angle, float cx, float cy) {
    float rad = angle * 3.14159265358979f / 180.0f;
    auto rot = D2D1::Matrix3x2F::Rotation(rad, D2D1::Point2F(cx, cy));
    m_matrix = rot * m_matrix;
}

void gpu_transform::multiply(const D2D1::Matrix3x2F& matrix) {
    m_matrix = matrix * m_matrix;
}

D2D1::Matrix3x2F gpu_transform::inverse() const {
    D2D1::Matrix3x2F inv = m_matrix;
    if (D2D1InvertMatrix(&inv)) {
        return inv;
    }
    return D2D1::Matrix3x2F::Identity();
}

float gpu_transform::determinant() const {
    return m_matrix._31 * 0 + (m_matrix._11 * m_matrix._22 - m_matrix._12 * m_matrix._21);
}

bool gpu_transform::is_invertible() const {
    return std::abs(determinant()) > 1e-10f;
}

} // namespace hre::render