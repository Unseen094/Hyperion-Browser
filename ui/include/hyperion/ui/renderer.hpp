#pragma once

#ifdef DrawText
#undef DrawText
#endif

#include <d2d1_3.h>
#include <dwrite_3.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>

namespace hyperion::ui {

using Microsoft::WRL::ComPtr;

class renderer {
public:
    renderer();
    ~renderer();

    bool initialize(HWND hwnd);
    void begin_draw();
    void end_draw();

    void clear(D2D1::ColorF color);
    void resize(UINT32 width, UINT32 height);

    ID2D1DeviceContext5* context() const { return m_d2d_context.Get(); }
    IDWriteFactory7* write_factory() const { return m_dwrite_factory.Get(); }

private:
    ComPtr<ID2D1Factory7> m_d2d_factory;
    ComPtr<ID2D1DeviceContext5> m_d2d_context;
    ComPtr<ID2D1Device5> m_d2d_device;
    ComPtr<IDXGISwapChain1> m_swap_chain;
    ComPtr<ID2D1Bitmap1> m_target_bitmap;
    ComPtr<IDWriteFactory7> m_dwrite_factory;

    void create_device_resources(HWND hwnd);
};

} // namespace hyperion::ui
