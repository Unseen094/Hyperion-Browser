#include <hre/image/image_decoder.hpp>

namespace hre::image {

decoder_registry& decoder_registry::instance() {
    static decoder_registry reg;
    return reg;
}

void decoder_registry::register_decoder(std::unique_ptr<image_decoder> decoder) {
    m_decoders.push_back(std::move(decoder));
}

bool decoder_registry::decode(const uint8_t* data, size_t size, image_decode_result& result) {
    auto fmt = detect_format(data, size);
    for (auto& d : m_decoders) {
        if (d->format() == fmt) {
            return d->decode(data, size, result);
        }
    }
    return false;
}

image_format decoder_registry::detect_format(const uint8_t* data, size_t size) {
    if (size < 8) return image_format::UNKNOWN;
    // PNG: 89 50 4E 47
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
        return image_format::PNG;
    // JPEG: FF D8 FF
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return image_format::JPEG;
    // GIF: 47 49 46 38
    if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x38)
        return image_format::GIF;
    // BMP: 42 4D
    if (data[0] == 0x42 && data[1] == 0x4D)
        return image_format::BMP;
    // WEBP: RIFF .... WEBP
    if (size >= 12 && data[0] == 0x52 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x46 &&
        data[8] == 0x57 && data[9] == 0x45 && data[10] == 0x42 && data[11] == 0x50)
        return image_format::WEBP;
    // AVIF: ftypavif / ftypmif1
    if (size >= 12 && data[4] == 0x66 && data[5] == 0x74 && data[6] == 0x79 &&
        ((data[8] == 0x61 && data[9] == 0x76 && data[10] == 0x69 && data[11] == 0x66) ||
         (data[8] == 0x6D && data[9] == 0x69 && data[10] == 0x66 && data[11] == 0x31)))
        return image_format::AVIF;
    return image_format::UNKNOWN;
}

} // namespace hre::image
