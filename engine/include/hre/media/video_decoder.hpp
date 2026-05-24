#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <wrl/client.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <d3d11_4.h>

namespace hre::media {

enum class video_codec {
    UNKNOWN,
    H264,
    H265,
    VP9,
    AV1
};

enum class pixel_format {
    UNKNOWN,
    NV12,
    I420,
    YUY2,
    ARGB,
    RGBA
};

struct video_frame {
    uint32_t width = 0;
    uint32_t height = 0;
    pixel_format format = pixel_format::UNKNOWN;
    int64_t timestamp_ns = 0;
    int64_t duration_ns = 0;
    bool is_key_frame = false;

    // YUV planes
    std::vector<uint8_t> y_plane;
    std::vector<uint8_t> u_plane;
    std::vector<uint8_t> v_plane;

    // RGB data (for decoded output)
    std::vector<uint8_t> rgb_data;
    int stride = 0;

    // DirectX texture (for hardware decoding)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> dx_texture;
    int texture_subresource = 0;

    void clear() {
        y_plane.clear();
        u_plane.clear();
        v_plane.clear();
        rgb_data.clear();
        dx_texture.Reset();
    }
};

struct video_stream_info {
    video_codec codec = video_codec::UNKNOWN;
    uint32_t width = 0;
    uint32_t height = 0;
    double framerate = 30.0;
    uint64_t bitrate = 0;
    pixel_format format = pixel_format::NV12;
    int profile = 0;
    int level = 0;
    bool has_b_frames = false;
    int ref_frames = 0;
};

class video_decoder {
public:
    video_decoder();
    virtual ~video_decoder();

    bool init(ID3D11Device5* device = nullptr);

    bool open(const std::wstring& url);
    bool open_from_bytes(const uint8_t* data, size_t size);

    bool read_frame(video_frame& frame);
    bool seek(int64_t timestamp_ns);

    void flush();

    const video_stream_info& stream_info() const { return m_stream_info; }
    bool is_open() const { return m_is_open; }
    bool supports_hardware() const { return m_supports_hardware; }

    void set_software_fallback(bool fallback) { m_software_fallback = fallback; }

    static bool is_codec_supported(video_codec codec);

private:
    bool init_media_foundation();
    bool create_reader();
    void convert_nv12_to_rgb(const uint8_t* nv12, uint8_t* rgb, int width, int height, int stride);
    void convert_yuv_to_rgb(const uint8_t* y, const uint8_t* u, const uint8_t* v, uint8_t* rgb, int width, int height, int y_stride, int uv_stride);

    video_stream_info m_stream_info;
    Microsoft::WRL::ComPtr<IMFSourceReader> m_reader;
    Microsoft::WRL::ComPtr<ID3D11Device5> m_d3d_device;

    bool m_is_open = false;
    bool m_supports_hardware = false;
    bool m_software_fallback = true;
    bool mf_initialized = false;

    static int s_mf_ref_count;
};

// ---- Hardware decoder manager ---------------------------------------------

class hardware_decoder_manager {
public:
    static hardware_decoder_manager& instance();

    bool init(ID3D11Device5* device);
    bool can_decode(video_codec codec);

    std::shared_ptr<video_decoder> create_decoder(video_codec codec);

private:
    hardware_decoder_manager() = default;
    bool m_initialized = false;
    ID3D11Device5* m_device = nullptr;
};

// ---- Codec detection ------------------------------------------------------

struct codec_support_info {
    video_codec codec;
    std::wstring name;
    bool hardware_supported;
    bool software_supported;
    std::wstring mime_type;
    std::vector<std::wstring> file_extensions;
};

const std::vector<codec_support_info>& get_supported_codecs();

} // namespace hre::media
