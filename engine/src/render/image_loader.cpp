#include <hre/render/image_loader.hpp>
#include <wrl/client.h>
#include <d2d1_3.h>
#include <winhttp.h>
#include <wininet.h>
#include <vector>

#pragma comment(lib, "wininet.lib")

namespace hre::render {

using Microsoft::WRL::ComPtr;

ComPtr<ID2D1Bitmap1> image_loader::load_from_file(const std::wstring& path, ID2D1DeviceContext5* context) {
    ComPtr<IWICImagingFactory> wic_factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_factory));

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic_factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) {
        return nullptr;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;

    ComPtr<IWICFormatConverter> converter;
    wic_factory->CreateFormatConverter(&converter);
    converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);

    ComPtr<ID2D1Bitmap1> bitmap;
    context->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap);
    return bitmap;
}

ComPtr<ID2D1Bitmap1> image_loader::load_from_url(const std::wstring& url, ID2D1DeviceContext5* context) {
    // Simple URL parsing - only support http/https for now
    if (url.find(L"http://") != 0 && url.find(L"https://") != 0) {
        return nullptr;
    }

    HINTERNET h_internet = InternetOpenW(L"HyperionImageLoader/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!h_internet) return nullptr;

    HINTERNET h_url = InternetOpenUrlW(h_internet, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!h_url) {
        InternetCloseHandle(h_internet);
        return nullptr;
    }

    std::vector<char> buffer;
    char chunk[4096];
    DWORD bytes_read = 0;

    while (InternetReadFile(h_url, chunk, sizeof(chunk), &bytes_read) && bytes_read > 0) {
        buffer.insert(buffer.end(), chunk, chunk + bytes_read);
    }

    InternetCloseHandle(h_url);
    InternetCloseHandle(h_internet);

    if (buffer.empty()) return nullptr;

    return load_from_memory(buffer.data(), buffer.size(), context);
}

ComPtr<ID2D1Bitmap1> image_loader::load_from_memory(const void* data, size_t size, ID2D1DeviceContext5* context) {
    ComPtr<IWICImagingFactory> wic_factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_factory));

    ComPtr<IWICStream> stream;
    wic_factory->CreateStream(&stream);
    stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<void*>(data)), static_cast<DWORD>(size));

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic_factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) {
        return nullptr;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return nullptr;

    ComPtr<IWICFormatConverter> converter;
    wic_factory->CreateFormatConverter(&converter);
    converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);

    ComPtr<ID2D1Bitmap1> bitmap;
    context->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap);
    return bitmap;
}

} // namespace hre::render
