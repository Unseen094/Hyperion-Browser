#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace hre::image {

enum class image_format {
    UNKNOWN,
    PNG,
    JPEG,
    WEBP,
    AVIF,
    GIF,
    BMP
};

struct image_decode_result {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bytes_per_pixel = 4;
    std::vector<uint8_t> pixel_data;
    image_format format = image_format::UNKNOWN;
    bool has_alpha = true;
};

class image_decoder {
public:
    virtual ~image_decoder() = default;

    virtual bool can_decode(const std::string& magic_bytes) const = 0;
    virtual image_format format() const = 0;
    virtual bool decode(const uint8_t* data, size_t size, image_decode_result& result) = 0;
};

class decoder_registry {
public:
    static decoder_registry& instance();

    void register_decoder(std::unique_ptr<image_decoder> decoder);
    bool decode(const uint8_t* data, size_t size, image_decode_result& result);
    image_format detect_format(const uint8_t* data, size_t size);

private:
    decoder_registry() = default;

    std::vector<std::unique_ptr<image_decoder>> m_decoders;
};

} // namespace hre::image