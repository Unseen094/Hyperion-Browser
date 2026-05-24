#include "hre/net/ws_deflate.hpp"
#include <zlib.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace hre::net {

class ws_deflate::impl {
public:
    z_stream deflater_;
    z_stream inflater_;
    bool deflater_initialized_ = false;
    bool inflater_initialized_ = false;
    ws_deflate_config config_;

    ~impl() {
        if (deflater_initialized_) deflateEnd(&deflater_);
        if (inflater_initialized_) inflateEnd(&inflater_);
    }

    bool init(const ws_deflate_config& cfg) {
        config_ = cfg;

        memset(&deflater_, 0, sizeof(deflater_));
        int window_bits = -cfg.client_max_window_bits; // negative = raw deflate
        int ret = deflateInit2(&deflater_, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                               window_bits, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) return false;
        deflater_initialized_ = true;

        memset(&inflater_, 0, sizeof(inflater_));
        window_bits = -cfg.server_max_window_bits;
        ret = inflateInit2(&inflater_, window_bits);
        if (ret != Z_OK) return false;
        inflater_initialized_ = true;

        return true;
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data, bool fin) {
        if (!deflater_initialized_) return {};

        deflater_.avail_in = (uInt)data.size();
        deflater_.next_in = (Bytef*)data.data();

        std::vector<uint8_t> out;
        out.resize(data.size() + 64);
        size_t total_out = 0;

        int flush = fin ? Z_SYNC_FLUSH : Z_NO_FLUSH;

        do {
            deflater_.avail_out = (uInt)(out.size() - total_out);
            deflater_.next_out = (Bytef*)(out.data() + total_out);

            int ret = deflate(&deflater_, flush);
            if (ret == Z_STREAM_ERROR) return {};

            total_out = out.size() - deflater_.avail_out;
            if (deflater_.avail_out == 0) {
                out.resize(out.size() * 2);
            }
        } while (deflater_.avail_out == 0);

        out.resize(total_out);

        // Remove trailing 0x00 0x00 0xFF 0xFF (zlib sync flush marker)
        // RFC 7692 requires stripping this
        if (fin && out.size() >= 4) {
            size_t trim = 0;
            if (out.size() >= 4 &&
                out[out.size() - 4] == 0x00 &&
                out[out.size() - 3] == 0x00 &&
                out[out.size() - 2] == 0xFF &&
                out[out.size() - 1] == 0xFF) {
                trim = 4;
            }
            if (trim > 0) {
                out.resize(out.size() - trim);
            }
        }

        if (fin && config_.client_no_context_takeover) {
            reset_deflater();
        }

        return out;
    }

    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, bool fin) {
        if (!inflater_initialized_) return {};

        inflater_.avail_in = (uInt)data.size();
        inflater_.next_in = (Bytef*)data.data();

        std::vector<uint8_t> out;
        out.resize(data.size() * 2 + 64);
        size_t total_out = 0;

        do {
            inflater_.avail_out = (uInt)(out.size() - total_out);
            inflater_.next_out = (Bytef*)(out.data() + total_out);

            int ret = inflate(&inflater_, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT ||
                ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                return {};
            }

            total_out = out.size() - inflater_.avail_out;
            if (inflater_.avail_out == 0) {
                out.resize(out.size() * 2);
            }
        } while (inflater_.avail_out == 0);

        out.resize(total_out);

        if (fin && config_.server_no_context_takeover) {
            reset_inflater();
        }

        return out;
    }

    void reset_deflater() {
        if (deflater_initialized_) {
            deflateReset(&deflater_);
        }
    }

    void reset_inflater() {
        if (inflater_initialized_) {
            inflateReset(&inflater_);
        }
    }
};

ws_deflate::ws_deflate() : pimpl_(std::make_unique<impl>()) {}
ws_deflate::~ws_deflate() = default;

bool ws_deflate::initialize(const ws_deflate_config& config) {
    config_ = config;
    initialized_ = pimpl_->init(config);
    return initialized_;
}

bool ws_deflate::is_initialized() const {
    return initialized_;
}

std::vector<uint8_t> ws_deflate::deflate(const std::vector<uint8_t>& data, bool fin) {
    return pimpl_->compress(data, fin);
}

std::vector<uint8_t> ws_deflate::inflate(const std::vector<uint8_t>& data, bool fin) {
    return pimpl_->decompress(data, fin);
}

void ws_deflate::reset_deflater() {
    pimpl_->reset_deflater();
}

void ws_deflate::reset_inflater() {
    pimpl_->reset_inflater();
}

} // namespace hre::net
