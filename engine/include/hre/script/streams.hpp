#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <queue>

namespace hre::script {

enum class readable_stream_state { READABLE, CLOSED, ERRORED };
enum class writable_stream_state { WRITABLE, CLOSED, ERRORED };

class readable_stream;

class read_result {
public:
    std::vector<uint8_t> data;
    bool done = false;
};

class byob_reader {
public:
    byob_reader() = default;

    read_result read(std::vector<uint8_t>& buffer);
    void cancel(const std::string& reason = "");
    void release_lock();

    bool active() const { return active_; }

private:
    friend class readable_stream;
    readable_stream* stream_ = nullptr;
    bool active_ = true;
};

class readable_stream {
public:
    using pull_fn = std::function<void(readable_stream* stream)>;
    using start_fn = std::function<void(readable_stream* stream)>;
    using cancel_fn = std::function<void(const std::string& reason)>;

    readable_stream();
    explicit readable_stream(const std::vector<uint8_t>& data);
    ~readable_stream();

    readable_stream_state state() const { return state_; }

    void start(start_fn fn) { start_fn_ = std::move(fn); }
    void pull(pull_fn fn) { pull_fn_ = std::move(fn); }
    void cancel(cancel_fn fn) { cancel_fn_ = std::move(fn); }

    read_result read();
    void enqueue(const std::vector<uint8_t>& chunk);
    void close();
    void error(const std::string& reason);
    void cancel_stream(const std::string& reason = "");

    std::shared_ptr<byob_reader> get_byob_reader();

    bool locked() const { return locked_; }

    size_t buffered() const { return buffer_.size(); }

private:
    readable_stream_state state_ = readable_stream_state::READABLE;
    std::queue<std::vector<uint8_t>> buffer_;
    pull_fn pull_fn_;
    start_fn start_fn_;
    cancel_fn cancel_fn_;
    bool started_ = false;
    bool locked_ = false;
    std::string error_reason_;

    friend class byob_reader;
};

class writable_stream;

class writable_stream_default_controller {
public:
    void error(const std::string& reason);
    void signal_abort(const std::string& reason);

private:
    friend class writable_stream;
    bool errored_ = false;
    std::string abort_reason_;
};

class writable_stream {
public:
    using write_fn = std::function<void(const std::vector<uint8_t>& chunk)>;
    using close_fn = std::function<void()>;
    using abort_fn = std::function<void(const std::string& reason)>;

    writable_stream();

    writable_stream_state state() const { return state_; }

    void write(const std::vector<uint8_t>& chunk);
    void close();
    void abort(const std::string& reason = "");

    void set_write_fn(write_fn fn) { write_fn_ = std::move(fn); }
    void set_close_fn(close_fn fn) { close_fn_ = std::move(fn); }
    void set_abort_fn(abort_fn fn) { abort_fn_ = std::move(fn); }

    bool locked() const { return locked_; }

    size_t high_water_mark() const { return high_water_mark_; }
    void set_high_water_mark(size_t hwm) { high_water_mark_ = hwm; }

    bool needs_backpressure() const {
        return buffered_ >= high_water_mark_;
    }

    size_t buffered_amount() const { return buffered_; }

private:
    writable_stream_state state_ = writable_stream_state::WRITABLE;
    size_t high_water_mark_ = 65536;
    size_t buffered_ = 0;
    bool locked_ = false;
    write_fn write_fn_;
    close_fn close_fn_;
    abort_fn abort_fn_;
    std::queue<std::vector<uint8_t>> write_queue_;
    bool closing_ = false;
};

class transform_stream {
public:
    using transform_fn = std::function<void(const std::vector<uint8_t>& chunk,
                                            readable_stream& writable_side)>;
    using flush_fn = std::function<void(readable_stream& writable_side)>;

    transform_stream();
    explicit transform_stream(transform_fn fn);

    std::shared_ptr<readable_stream> readable() const { return readable_; }
    std::shared_ptr<writable_stream> writable() const { return writable_; }

    void set_transform(transform_fn fn) { transform_fn_ = std::move(fn); }
    void set_flush(flush_fn fn) { flush_fn_ = std::move(fn); }

private:
    std::shared_ptr<readable_stream> readable_;
    std::shared_ptr<writable_stream> writable_;
    transform_fn transform_fn_;
    flush_fn flush_fn_;

    void on_write(const std::vector<uint8_t>& chunk);
    void on_close();
};

} // namespace hre::script
