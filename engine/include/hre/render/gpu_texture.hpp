#pragma once

#include <wrl/client.h>
#include <d3d11_4.h>
#include <d2d1_3.h>
#include <string>
#include <map>
#include <vector>

namespace hre::render {

struct gpu_texture_info {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_view;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_bitmap;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_render_target = false;
    bool is_dirty = true;
};

class gpu_texture_manager {
public:
    gpu_texture_manager();
    ~gpu_texture_manager();

    bool init(ID3D11Device5* device, ID2D1Device5* d2d_device);

    gpu_texture_info* create_texture(const std::wstring& id, uint32_t width, uint32_t height, bool render_target = false);
    gpu_texture_info* get_texture(const std::wstring& id);
    void release_texture(const std::wstring& id);
    void mark_dirty(const std::wstring& id);
    void clear_dirty_flags();

    bool upload_data(const std::wstring& id, const void* data, size_t size, uint32_t pitch);

    ID3D11Device5* device() const { return m_d3d_device.Get(); }
    ID2D1Device5* d2d_device() const { return m_d2d_device.Get(); }
    ID2D1DeviceContext5* d2d_context() const { return m_d2d_context.Get(); }

    void flush();

private:
    bool create_d2d_bitmap_for_texture(gpu_texture_info* info);

    Microsoft::WRL::ComPtr<ID3D11Device5> m_d3d_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext3> m_d3d_context;
    Microsoft::WRL::ComPtr<ID2D1Device5> m_d2d_device;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext5> m_d2d_context;

    std::map<std::wstring, gpu_texture_info> m_textures;
};

class gpu_transform {
public:
    gpu_transform();

    void identity();
    void translate(float tx, float ty);
    void scale(float sx, float sy);
    void rotate(float angle, float cx = 0, float cy = 0);
    void multiply(const D2D1::Matrix3x2F& matrix);

    const D2D1::Matrix3x2F& matrix() const { return m_matrix; }
    D2D1::Matrix3x2F inverse() const;

    float determinant() const;
    bool is_invertible() const;

private:
    D2D1::Matrix3x2F m_matrix;
};

} // namespace hre::render