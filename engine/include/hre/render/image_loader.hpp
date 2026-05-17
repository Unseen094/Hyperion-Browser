#pragma once

#include <string>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <wincodec.h>

#ifdef DrawText
#undef DrawText
#endif

#include <d2d1_3.h>

namespace hre::render {

class image_loader {
public:
    static Microsoft::WRL::ComPtr<ID2D1Bitmap1> load_from_file(
        const std::wstring& path,
        ID2D1DeviceContext5* context);

    static Microsoft::WRL::ComPtr<ID2D1Bitmap1> load_from_url(
        const std::wstring& url,
        ID2D1DeviceContext5* context);

    static Microsoft::WRL::ComPtr<ID2D1Bitmap1> load_from_memory(
        const void* data,
        size_t size,
        ID2D1DeviceContext5* context);
};

} // namespace hre::render
