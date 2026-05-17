#include <hyperion/ui/renderer.hpp>
#include <hyperion/platform/logging.hpp>
#include <d3d11_4.h>
#include <dxgi1_6.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace hyperion::ui {

renderer::renderer() {}
renderer::~renderer() {}

bool renderer::initialize(HWND hwnd) {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory7), &m_d2d_factory);
    if (FAILED(hr)) {
        HYPERION_LOG_ERROR("Failed to create D2D factory");
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7), &m_dwrite_factory);
    if (FAILED(hr)) {
        HYPERION_LOG_ERROR("Failed to create DWrite factory");
        return false;
    }

    create_device_resources(hwnd);
    return true;
}

void renderer::create_device_resources(HWND hwnd) {
    // Create D3D device
    ComPtr<ID3D11Device> d3d_device;
    ComPtr<ID3D11DeviceContext> d3d_context;
    
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_1 };
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &d3d_device, nullptr, &d3d_context);
    if (FAILED(hr)) return;

    ComPtr<IDXGIDevice> dxgi_device;
    d3d_device.As(&dxgi_device);

    m_d2d_factory->CreateDevice(dxgi_device.Get(), &m_d2d_device);
    m_d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2d_context);

    // Create Swap Chain
    ComPtr<IDXGIAdapter> dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);

    ComPtr<IDXGIFactory2> dxgi_factory;
    dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = { 0 };
    swap_chain_desc.Width = 0; // Use window size
    swap_chain_desc.Height = 0;
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_chain_desc.Flags = 0;

    dxgi_factory->CreateSwapChainForHwnd(d3d_device.Get(), hwnd, &swap_chain_desc, nullptr, nullptr, &m_swap_chain);

    // Create a D2D bitmap from the swap chain back buffer
    ComPtr<IDXGISurface> dxgi_back_buffer;
    m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&dxgi_back_buffer));

    D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    m_d2d_context->CreateBitmapFromDxgiSurface(dxgi_back_buffer.Get(), &bitmap_properties, &m_target_bitmap);
    m_d2d_context->SetTarget(m_target_bitmap.Get());
    
    HYPERION_LOG_INFO("Direct2D Swap Chain and Target Bitmap created");
}

void renderer::begin_draw() {
    if (m_d2d_context) {
        m_d2d_context->BeginDraw();
    }
}

void renderer::end_draw() {
    if (m_d2d_context) {
        HRESULT hr = m_d2d_context->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            // Handle device loss in a real app
        }
        m_swap_chain->Present(1, 0);
    }
}

void renderer::clear(D2D1::ColorF color) {
    if (m_d2d_context) {
        m_d2d_context->Clear(color);
    }
}

void renderer::resize(UINT32 width, UINT32 height) {
    if (!m_swap_chain || !m_d2d_context) return;

    m_d2d_context->SetTarget(nullptr);
    m_target_bitmap.Reset();

    HRESULT hr = m_swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (SUCCEEDED(hr)) {
        ComPtr<IDXGISurface> dxgi_back_buffer;
        m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&dxgi_back_buffer));

        D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );

        m_d2d_context->CreateBitmapFromDxgiSurface(dxgi_back_buffer.Get(), &bitmap_properties, &m_target_bitmap);
        m_d2d_context->SetTarget(m_target_bitmap.Get());
    }
}

} // namespace hyperion::ui
