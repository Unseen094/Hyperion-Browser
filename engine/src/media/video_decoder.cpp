#include <hre/media/video_decoder.hpp>
#include <algorithm>
#include <cstring>

#ifndef MFVideoFormat_AV01
struct __declspec(uuid("31313041-0000-0010-8000-00AA00389B71")) MFVideoFormat_AV01_;
static const GUID MFVideoFormat_AV01 = { 0x31313041, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
#endif

namespace hre::media {

int video_decoder::s_mf_ref_count = 0;

video_decoder::video_decoder() = default;
video_decoder::~video_decoder() {
    flush();
    if (mf_initialized) {
        MFShutdown();
        mf_initialized = false;
    }
}

bool video_decoder::init(ID3D11Device5* device) {
    m_d3d_device = device;
    m_supports_hardware = (device != nullptr);
    return init_media_foundation();
}

bool video_decoder::init_media_foundation() {
    if (mf_initialized) return true;
    HRESULT hr = MFStartup(MF_VERSION);
    if (SUCCEEDED(hr)) {
        mf_initialized = true;
        return true;
    }
    return false;
}

bool video_decoder::open(const std::wstring& url) {
    if (!mf_initialized) return false;
    flush();

    HRESULT hr = MFCreateSourceReaderFromURL(url.c_str(), nullptr, &m_reader);
    if (FAILED(hr)) return false;

    if (m_d3d_device && m_supports_hardware) {
        Microsoft::WRL::ComPtr<IMFAttributes> attributes;
        MFCreateAttributes(&attributes, 1);
        attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, m_d3d_device.Get());
    }

    Microsoft::WRL::ComPtr<IMFMediaType> media_type;
    hr = m_reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &media_type);
    if (SUCCEEDED(hr)) {
        UINT32 width = 0, height = 0;
        MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
        m_stream_info.width = width;
        m_stream_info.height = height;

        GUID subtype = {};
        media_type->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (subtype == MFVideoFormat_H264) m_stream_info.codec = video_codec::H264;
        else if (subtype == MFVideoFormat_H265) m_stream_info.codec = video_codec::H265;
        else if (subtype == MFVideoFormat_VP90) m_stream_info.codec = video_codec::VP9;
        else if (subtype == MFVideoFormat_AV01) m_stream_info.codec = video_codec::AV1;

        UINT32 num, den;
        MFGetAttributeRatio(media_type.Get(), MF_MT_FRAME_RATE, &num, &den);
        if (den > 0) m_stream_info.framerate = static_cast<double>(num) / den;
    }

    m_is_open = true;
    return true;
}

bool video_decoder::open_from_bytes(const uint8_t* data, size_t size) {
    (void)data; (void)size;
    return false;
}

bool video_decoder::read_frame(video_frame& frame) {
    if (!m_reader) return false;

    DWORD stream_index = MF_SOURCE_READER_FIRST_VIDEO_STREAM;
    DWORD flags = 0;
    Microsoft::WRL::ComPtr<IMFSample> sample;
    HRESULT hr = m_reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &stream_index, &flags, nullptr, &sample);

    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) return false;

    if (sample) {
        LONGLONG timestamp = 0;
        sample->GetSampleTime(&timestamp);
        frame.timestamp_ns = static_cast<int64_t>(timestamp * 100);

        LONGLONG duration = 0;
        sample->GetSampleDuration(&duration);
        frame.duration_ns = static_cast<int64_t>(duration * 100);

        DWORD buffer_count = 0;
        sample->GetBufferCount(&buffer_count);

        Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
        sample->GetBufferByIndex(0, &media_buffer);

        BYTE* buffer_data = nullptr;
        DWORD buffer_length = 0;
        media_buffer->Lock(&buffer_data, nullptr, &buffer_length);

        frame.width = m_stream_info.width;
        frame.height = m_stream_info.height;
        frame.format = m_stream_info.format;

        if (m_stream_info.format == pixel_format::NV12) {
            size_t y_size = frame.width * frame.height;
            frame.y_plane.assign(buffer_data, buffer_data + y_size);
            frame.u_plane.assign(buffer_data + y_size, buffer_data + y_size + (frame.width * frame.height / 2));
        }

        media_buffer->Unlock();
    }

    return true;
}

bool video_decoder::seek(int64_t timestamp_ns) {
    if (!m_reader) return false;
    PROPVARIANT var;
    var.vt = VT_I8;
    var.hVal.QuadPart = timestamp_ns / 100;
    HRESULT hr = m_reader->SetCurrentPosition(GUID_NULL, var);
    return SUCCEEDED(hr);
}

void video_decoder::flush() {
    if (m_reader) {
        m_reader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    }
}

bool video_decoder::create_reader() { return false; }

void video_decoder::convert_nv12_to_rgb(const uint8_t* nv12, uint8_t* rgb, int width, int height, int stride) {
    int y_size = width * height;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int y_idx = y * width + x;
            int uv_idx = (y / 2) * width + (x & ~1);
            int Y = nv12[y_idx];
            int U = nv12[y_size + uv_idx];
            int V = nv12[y_size + uv_idx + 1];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = (298 * C + 409 * E + 128) >> 8;
            int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int B = (298 * C + 516 * D + 128) >> 8;

            int out_idx = y * stride + x * 4;
            rgb[out_idx] = static_cast<uint8_t>(std::clamp(R, 0, 255));
            rgb[out_idx + 1] = static_cast<uint8_t>(std::clamp(G, 0, 255));
            rgb[out_idx + 2] = static_cast<uint8_t>(std::clamp(B, 0, 255));
            rgb[out_idx + 3] = 255;
        }
    }
}

void video_decoder::convert_yuv_to_rgb(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* rgb, int width, int height, int y_stride, int uv_stride) {
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int Y = y[row * y_stride + col];
            int U = u[(row / 2) * uv_stride + (col / 2)] - 128;
            int V = v[(row / 2) * uv_stride + (col / 2)] - 128;

            int R = (298 * (Y - 16) + 409 * V + 128) >> 8;
            int G = (298 * (Y - 16) - 100 * U - 208 * V + 128) >> 8;
            int B = (298 * (Y - 16) + 516 * U + 128) >> 8;

            int idx = row * width * 4 + col * 4;
            rgb[idx] = static_cast<uint8_t>(std::clamp(R, 0, 255));
            rgb[idx + 1] = static_cast<uint8_t>(std::clamp(G, 0, 255));
            rgb[idx + 2] = static_cast<uint8_t>(std::clamp(B, 0, 255));
            rgb[idx + 3] = 255;
        }
    }
}

bool video_decoder::is_codec_supported(video_codec codec) {
    return codec == video_codec::H264 || codec == video_codec::H265 || codec == video_codec::VP9;
}

// ---- Hardware Decoder Manager ---------------------------------------------

hardware_decoder_manager& hardware_decoder_manager::instance() {
    static hardware_decoder_manager inst;
    return inst;
}

bool hardware_decoder_manager::init(ID3D11Device5* device) {
    m_device = device;
    m_initialized = true;
    return true;
}

bool hardware_decoder_manager::can_decode(video_codec codec) {
    return m_initialized && (codec == video_codec::H264 || codec == video_codec::H265);
}

std::shared_ptr<video_decoder> hardware_decoder_manager::create_decoder(video_codec codec) {
    (void)codec;
    auto decoder = std::make_shared<video_decoder>();
    if (decoder->init(m_device)) return decoder;
    return nullptr;
}

// ---- Codec Detection ------------------------------------------------------

const std::vector<codec_support_info>& get_supported_codecs() {
    static const std::vector<codec_support_info> codecs = {
        { video_codec::H264, L"h264", true, true, L"video/mp4", { L".mp4", L".m4v" } },
        { video_codec::H265, L"hevc", true, true, L"video/mp4", { L".mp4", L".hevc" } },
        { video_codec::VP9, L"vp9", false, true, L"video/webm", { L".webm" } },
        { video_codec::AV1, L"av1", false, false, L"video/mp4", { L".mp4", L".mkv" } },
    };
    return codecs;
}

} // namespace hre::media
