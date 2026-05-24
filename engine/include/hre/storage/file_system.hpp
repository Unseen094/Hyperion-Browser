#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace hre::storage {

enum class file_system_error {
    OK,
    NOT_FOUND,
    PERMISSION_DENIED,
    QUOTA_EXCEEDED,
    INVALID_OPERATION,
    NOT_SUPPORTED,
    FILE_EXISTS,
    NOT_A_FILE,
    NOT_A_DIRECTORY,
    IS_A_DIRECTORY,
    NO_SPACE
};

struct file_metadata {
    std::wstring name;
    std::wstring full_path;
    int64_t size = 0;
    int64_t last_modified = 0;
    int64_t created = 0;
    bool is_directory = false;
    bool is_file = false;
    std::wstring mime_type;
};

struct file_write_params {
    bool append = false;
    bool create = true;
    bool exclusive = false;
};

struct file_read_params {
    int64_t offset = 0;
    int64_t size = -1; // -1 = whole file
};

// ---- File Handle ----------------------------------------------------------

class file_handle {
public:
    file_handle();
    ~file_handle();

    bool open(const std::wstring& path, bool writable = false);
    void close();
    bool is_open() const { return m_is_open; }

    int64_t read(uint8_t* buffer, int64_t size);
    int64_t write(const uint8_t* data, int64_t size);
    int64_t seek(int64_t offset, int whence = 0);
    int64_t tell() const;
    int64_t size() const;

    std::wstring path() const { return m_path; }
    bool writable() const { return m_writable; }

private:
    std::wstring m_path;
    void* m_handle = nullptr;
    bool m_is_open = false;
    bool m_writable = false;
};

// ---- Directory Entry ------------------------------------------------------

struct directory_entry {
    std::wstring name;
    std::wstring full_path;
    bool is_directory = false;
    int64_t size = 0;
    int64_t last_modified = 0;
};

// ---- File System API ------------------------------------------------------

class file_system_api {
public:
    file_system_api();
    ~file_system_api();

    // File operations
    file_system_error read_file(const std::wstring& path, std::vector<uint8_t>& data);
    file_system_error write_file(const std::wstring& path, const std::vector<uint8_t>& data,
                                  const file_write_params& params = {});
    file_system_error append_file(const std::wstring& path, const std::vector<uint8_t>& data);
    file_system_error delete_file(const std::wstring& path);
    file_system_error copy_file(const std::wstring& src, const std::wstring& dest);
    file_system_error move_file(const std::wstring& src, const std::wstring& dest);
    bool file_exists(const std::wstring& path) const;
    file_metadata get_file_metadata(const std::wstring& path) const;

    // Directory operations
    file_system_error create_directory(const std::wstring& path);
    file_system_error create_directory_recursive(const std::wstring& path);
    file_system_error delete_directory(const std::wstring& path, bool recursive = false);
    bool directory_exists(const std::wstring& path) const;
    std::vector<directory_entry> list_directory(const std::wstring& path) const;

    // Path utilities
    static std::wstring get_file_name(const std::wstring& path);
    static std::wstring get_directory_name(const std::wstring& path);
    static std::wstring get_extension(const std::wstring& path);
    static std::wstring combine_path(const std::wstring& base, const std::wstring& relative);
    static std::wstring normalize_path(const std::wstring& path);

    // Temporary files
    std::wstring create_temp_file(const std::wstring& suffix = L".tmp");
    file_system_error delete_temp_file(const std::wstring& path);

    // Storage info
    uint64_t get_free_space(const std::wstring& path) const;
    uint64_t get_total_space(const std::wstring& path) const;

    // Quota management
    void set_quota(const std::wstring& origin, uint64_t max_bytes);
    uint64_t get_quota(const std::wstring& origin) const;
    uint64_t get_usage(const std::wstring& origin) const;

    // Blob storage
    std::wstring store_blob(const std::vector<uint8_t>& data, const std::wstring& mime_type = L"application/octet-stream");
    bool retrieve_blob(const std::wstring& blob_id, std::vector<uint8_t>& data) const;
    bool delete_blob(const std::wstring& blob_id);

    // File picker integration
    using file_picker_callback = std::function<void(const std::vector<std::wstring>& paths)>;
    void set_file_picker_callback(file_picker_callback cb) { m_picker_cb = cb; }

private:
    std::wstring m_temp_dir;
    std::map<std::wstring, uint64_t> m_origin_quotas;
    std::map<std::wstring, uint64_t> m_origin_usage;

    struct blob_entry {
        std::vector<uint8_t> data;
        std::wstring mime_type;
    };
    std::map<std::wstring, blob_entry> m_blobs;
    uint64_t m_next_blob_id = 1;

    file_picker_callback m_picker_cb;

    bool is_within_quota(const std::wstring& origin, uint64_t additional_bytes) const;
    std::wstring get_origin_for_path(const std::wstring& path) const;
};

// ---- File Watcher ---------------------------------------------------------

struct file_watch_event {
    enum change_type { CREATED, MODIFIED, DELETED, RENAMED };
    change_type type;
    std::wstring path;
    std::wstring new_path;
};

class file_watcher {
public:
    file_watcher();
    ~file_watcher();

    bool watch(const std::wstring& path, bool recursive = false);
    void unwatch(const std::wstring& path);
    void unwatch_all();

    using watch_callback = std::function<void(const file_watch_event&)>;
    void set_callback(watch_callback cb) { m_callback = cb; }

    void poll_events();

private:
    struct watch_entry {
        std::wstring path;
        bool recursive;
    };
    std::vector<watch_entry> m_watched;
    watch_callback m_callback;
};

} // namespace hre::storage
