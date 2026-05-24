#include <hre/render/image_loader.hpp>
#include <wrl/client.h>
#include <d2d1_3.h>
#include <vector>
#include <string>
#include <fstream>

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
    if (url.find(L"http://") != 0 && url.find(L"https://") != 0) {
        return nullptr;
    }
    return nullptr;
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
