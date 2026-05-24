#include <hre/script/streams.hpp>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace hre::script {

static constexpr size_t k_default_hwm = 65536;

// ---- ReadableStream --------------------------------------------------------

readable_stream::readable_stream() = default;

readable_stream::readable_stream(const std::vector<uint8_t>& data) {
    if (!data.empty()) buffer_.push(data);
}

readable_stream::~readable_stream() = default;

read_result readable_stream::read() {
    read_result result;

    if (state_ == readable_stream_state::CLOSED) {
        result.done = true;
        return result;
    }

    if (state_ == readable_stream_state::ERRORED) {
        result.done = true;
        return result;
    }

    if (!started_) {
        started_ = true;
        if (start_fn_) start_fn_(this);
    }

    if (buffer_.empty()) {
        if (pull_fn_) pull_fn_(this);
    }

    if (!buffer_.empty()) {
        result.data = std::move(buffer_.front());
        buffer_.pop();
        result.done = false;
        return result;
    }

    if (state_ == readable_stream_state::CLOSED) {
        result.done = true;
    }

    return result;
}

void readable_stream::enqueue(const std::vector<uint8_t>& chunk) {
    if (state_ != readable_stream_state::READABLE) return;
    buffer_.push(chunk);
}

void readable_stream::close() {
    state_ = readable_stream_state::CLOSED;
}

void readable_stream::error(const std::string& reason) {
    state_ = readable_stream_state::ERRORED;
    error_reason_ = reason;
}

void readable_stream::cancel_stream(const std::string& reason) {
    if (state_ == readable_stream_state::CLOSED || state_ == readable_stream_state::ERRORED) return;

    if (cancel_fn_) cancel_fn_(reason);

    while (!buffer_.empty()) buffer_.pop();
    state_ = readable_stream_state::CLOSED;
    locked_ = false;
}

std::shared_ptr<byob_reader> readable_stream::get_byob_reader() {
    auto reader = std::make_shared<byob_reader>();
    reader->stream_ = this;
    reader->active_ = true;
    locked_ = true;
    return reader;
}

// ---- BYOBReader ------------------------------------------------------------

read_result byob_reader::read(std::vector<uint8_t>& buffer) {
    read_result result;
    if (!active_ || !stream_) {
        result.done = true;
        return result;
    }

    if (stream_->state_ == readable_stream_state::CLOSED) {
        result.done = true;
        return result;
    }

    if (stream_->state_ == readable_stream_state::ERRORED) {
        result.done = true;
        return result;
    }

    if (!stream_->started_) {
        stream_->started_ = true;
        if (stream_->start_fn_) stream_->start_fn_(stream_);
    }

    if (stream_->buffer_.empty()) {
        if (stream_->pull_fn_) stream_->pull_fn_(stream_);
    }

    if (!stream_->buffer_.empty()) {
        auto& chunk = stream_->buffer_.front();
        size_t to_copy = std::min(chunk.size(), buffer.size());
        memcpy(buffer.data(), chunk.data(), to_copy);
        chunk.erase(chunk.begin(), chunk.begin() + to_copy);
        if (chunk.empty()) stream_->buffer_.pop();
        result.data.assign(buffer.begin(), buffer.begin() + to_copy);
        result.done = false;
        return result;
    }

    if (stream_->state_ == readable_stream_state::CLOSED) {
        result.done = true;
    }

    return result;
}

void byob_reader::cancel(const std::string& reason) {
    if (active_ && stream_) {
        stream_->cancel_stream(reason);
        stream_->locked_ = false;
    }
    active_ = false;
    stream_ = nullptr;
}

void byob_reader::release_lock() {
    if (stream_) stream_->locked_ = false;
    active_ = false;
    stream_ = nullptr;
}

// ---- WritableStream --------------------------------------------------------

writable_stream::writable_stream() = default;

void writable_stream::write(const std::vector<uint8_t>& chunk) {
    if (state_ != writable_stream_state::WRITABLE || closing_) return;

    buffered_ += chunk.size();

    if (write_fn_) {
        write_fn_(chunk);
    }

    write_queue_.push(chunk);

    if (needs_backpressure()) {
        // Backpressure signal - consumer should pause
    }
}

void writable_stream::close() {
    if (state_ != writable_stream_state::WRITABLE) return;

    closing_ = true;
    if (close_fn_) close_fn_();
    state_ = writable_stream_state::CLOSED;
    closing_ = false;
    buffered_ = 0;
}

void writable_stream::abort(const std::string& reason) {
    if (state_ != writable_stream_state::WRITABLE) return;

    if (abort_fn_) abort_fn_(reason);

    while (!write_queue_.empty()) write_queue_.pop();
    buffered_ = 0;
    state_ = writable_stream_state::ERRORED;
}

// ---- WritableStreamDefaultController ---------------------------------------

void writable_stream_default_controller::error(const std::string& reason) {
    errored_ = true;
}

void writable_stream_default_controller::signal_abort(const std::string& reason) {
    abort_reason_ = reason;
}

// ---- TransformStream -------------------------------------------------------

transform_stream::transform_stream() {
    readable_ = std::make_shared<readable_stream>();
    writable_ = std::make_shared<writable_stream>();

    writable_->set_write_fn([this](const std::vector<uint8_t>& chunk) {
        this->on_write(chunk);
    });

    writable_->set_close_fn([this]() {
        this->on_close();
    });
}

transform_stream::transform_stream(transform_fn fn)
    : transform_stream() {
    transform_fn_ = std::move(fn);
}

void transform_stream::on_write(const std::vector<uint8_t>& chunk) {
    if (transform_fn_) {
        transform_fn_(chunk, *readable_);
    } else {
        // Identity/passthrough: forward chunk directly
        readable_->enqueue(chunk);
    }
}

void transform_stream::on_close() {
    if (flush_fn_) {
        flush_fn_(*readable_);
    }
    readable_->close();
}

} // namespace hre::script
