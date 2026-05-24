#include <hre/storage/file_system.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <windows.h>

namespace hre::storage {

file_system_api::file_system_api() {
    wchar_t temp_buf[MAX_PATH];
    if (GetTempPathW(MAX_PATH, temp_buf)) m_temp_dir = temp_buf;
}

file_system_api::~file_system_api() = default;

file_system_error file_system_api::read_file(const std::wstring& path, std::vector<uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return file_system_error::NOT_FOUND;
    auto size = file.tellg();
    file.seekg(0);
    data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    return file_system_error::OK;
}

file_system_error file_system_api::write_file(const std::wstring& path, const std::vector<uint8_t>& data,
                                               const file_write_params& params) {
    std::ios_base::openmode mode = std::ios::binary;
    if (params.append) mode |= std::ios::app;
    else mode |= std::ios::trunc;

    if (params.exclusive) {
        std::ifstream test(path);
        if (test.is_open()) return file_system_error::FILE_EXISTS;
    }

    std::ofstream file(path, mode);
    if (!file.is_open()) return file_system_error::PERMISSION_DENIED;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file_system_error::OK;
}

file_system_error file_system_api::append_file(const std::wstring& path, const std::vector<uint8_t>& data) {
    file_write_params params;
    params.append = true;
    return write_file(path, data, params);
}

file_system_error file_system_api::delete_file(const std::wstring& path) {
    if (DeleteFileW(path.c_str())) return file_system_error::OK;
    if (GetLastError() == ERROR_FILE_NOT_FOUND) return file_system_error::NOT_FOUND;
    return file_system_error::PERMISSION_DENIED;
}

file_system_error file_system_api::copy_file(const std::wstring& src, const std::wstring& dest) {
    if (CopyFileW(src.c_str(), dest.c_str(), FALSE)) return file_system_error::OK;
    return file_system_error::NOT_FOUND;
}

file_system_error file_system_api::move_file(const std::wstring& src, const std::wstring& dest) {
    if (MoveFileW(src.c_str(), dest.c_str())) return file_system_error::OK;
    return file_system_error::NOT_FOUND;
}

bool file_system_api::file_exists(const std::wstring& path) const {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

file_metadata file_system_api::get_file_metadata(const std::wstring& path) const {
    file_metadata meta;
    meta.full_path = path;
    meta.name = get_file_name(path);

    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) {
        meta.is_directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        meta.is_file = !meta.is_directory;
        LARGE_INTEGER sz;
        sz.LowPart = info.nFileSizeLow;
        sz.HighPart = info.nFileSizeHigh;
        meta.size = sz.QuadPart;

        LARGE_INTEGER mod;
        mod.LowPart = info.ftLastWriteTime.dwLowDateTime;
        mod.HighPart = info.ftLastWriteTime.dwHighDateTime;
        meta.last_modified = mod.QuadPart / 10000000 - 11644473600LL;

        LARGE_INTEGER cr;
        cr.LowPart = info.ftCreationTime.dwLowDateTime;
        cr.HighPart = info.ftCreationTime.dwHighDateTime;
        meta.created = cr.QuadPart / 10000000 - 11644473600LL;
    }

    return meta;
}

file_system_error file_system_api::create_directory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) return file_system_error::OK;
    if (GetLastError() == ERROR_ALREADY_EXISTS) return file_system_error::FILE_EXISTS;
    return file_system_error::PERMISSION_DENIED;
}

file_system_error file_system_api::create_directory_recursive(const std::wstring& path) {
    size_t pos = 0;
    while ((pos = path.find_first_of(L"\\/", pos + 1)) != std::wstring::npos) {
        std::wstring parent = path.substr(0, pos);
        if (!parent.empty() && !directory_exists(parent)) {
            CreateDirectoryW(parent.c_str(), nullptr);
        }
    }
    if (!directory_exists(path)) {
        if (!CreateDirectoryW(path.c_str(), nullptr)) return file_system_error::PERMISSION_DENIED;
    }
    return file_system_error::OK;
}

file_system_error file_system_api::delete_directory(const std::wstring& path, bool recursive) {
    if (recursive) {
        for (const auto& entry : list_directory(path)) {
            if (entry.is_directory) {
                delete_directory(entry.full_path, true);
            } else {
                DeleteFileW(entry.full_path.c_str());
            }
        }
    }
    if (RemoveDirectoryW(path.c_str())) return file_system_error::OK;
    return file_system_error::NOT_FOUND;
}

bool file_system_api::directory_exists(const std::wstring& path) const {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<directory_entry> file_system_api::list_directory(const std::wstring& path) const {
    std::vector<directory_entry> entries;
    std::wstring search = path + L"\\*";
    WIN32_FIND_DATAW find_data;
    HANDLE find = FindFirstFileW(search.c_str(), &find_data);
    if (find == INVALID_HANDLE_VALUE) return entries;

    do {
        std::wstring name = find_data.cFileName;
        if (name == L"." || name == L"..") continue;
        directory_entry entry;
        entry.name = name;
        entry.full_path = path + L"\\" + name;
        entry.is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        LARGE_INTEGER sz;
        sz.LowPart = find_data.nFileSizeLow;
        sz.HighPart = find_data.nFileSizeHigh;
        entry.size = sz.QuadPart;
        entries.push_back(entry);
    } while (FindNextFileW(find, &find_data));

    FindClose(find);
    return entries;
}

std::wstring file_system_api::get_file_name(const std::wstring& path) {
    size_t sep = path.find_last_of(L"\\/");
    return (sep == std::wstring::npos) ? path : path.substr(sep + 1);
}

std::wstring file_system_api::get_directory_name(const std::wstring& path) {
    size_t sep = path.find_last_of(L"\\/");
    return (sep == std::wstring::npos) ? L"" : path.substr(0, sep);
}

std::wstring file_system_api::get_extension(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    size_t sep = path.find_last_of(L"\\/");
    if (sep != std::wstring::npos && sep > dot) return L"";
    return path.substr(dot);
}

std::wstring file_system_api::combine_path(const std::wstring& base, const std::wstring& relative) {
    if (base.empty()) return relative;
    if (relative.empty()) return base;
    if (relative.find(L":\\") != std::wstring::npos || relative.find(L"\\\\") == 0) return relative;
    std::wstring result = base;
    if (result.back() != L'\\') result += L'\\';
    result += relative;
    return result;
}

std::wstring file_system_api::normalize_path(const std::wstring& path) {
    std::wstring result = path;
    std::replace(result.begin(), result.end(), L'/', L'\\');
    return result;
}

std::wstring file_system_api::create_temp_file(const std::wstring& suffix) {
    wchar_t temp_path[MAX_PATH];
    wchar_t temp_file[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp_path)) return L"";
    if (!GetTempFileNameW(temp_path, L"hre", 0, temp_file)) return L"";
    std::wstring result = temp_file;
    if (!suffix.empty() && suffix != L".tmp") {
        std::wstring new_path = result + suffix;
        MoveFileW(result.c_str(), new_path.c_str());
        return new_path;
    }
    return result;
}

file_system_error file_system_api::delete_temp_file(const std::wstring& path) {
    return delete_file(path);
}

uint64_t file_system_api::get_free_space(const std::wstring& path) const {
    ULARGE_INTEGER free;
    if (GetDiskFreeSpaceExW(path.c_str(), &free, nullptr, nullptr)) return free.QuadPart;
    return 0;
}

uint64_t file_system_api::get_total_space(const std::wstring& path) const {
    ULARGE_INTEGER total;
    if (GetDiskFreeSpaceExW(path.c_str(), nullptr, &total, nullptr)) return total.QuadPart;
    return 0;
}

void file_system_api::set_quota(const std::wstring& origin, uint64_t max_bytes) {
    m_origin_quotas[origin] = max_bytes;
}

uint64_t file_system_api::get_quota(const std::wstring& origin) const {
    auto it = m_origin_quotas.find(origin);
    return it != m_origin_quotas.end() ? it->second : UINT64_MAX;
}

uint64_t file_system_api::get_usage(const std::wstring& origin) const {
    auto it = m_origin_usage.find(origin);
    return it != m_origin_usage.end() ? it->second : 0;
}

std::wstring file_system_api::store_blob(const std::vector<uint8_t>& data, const std::wstring& mime_type) {
    std::wstring id = L"blob_" + std::to_wstring(m_next_blob_id++);
    m_blobs[id] = { data, mime_type };
    return id;
}

bool file_system_api::retrieve_blob(const std::wstring& blob_id, std::vector<uint8_t>& data) const {
    auto it = m_blobs.find(blob_id);
    if (it == m_blobs.end()) return false;
    data = it->second.data;
    return true;
}

bool file_system_api::delete_blob(const std::wstring& blob_id) {
    return m_blobs.erase(blob_id) > 0;
}

bool file_system_api::is_within_quota(const std::wstring& origin, uint64_t additional_bytes) const {
    uint64_t max_quota = get_quota(origin);
    if (max_quota == UINT64_MAX) return true;
    uint64_t current = get_usage(origin);
    return (current + additional_bytes) <= max_quota;
}

std::wstring file_system_api::get_origin_for_path(const std::wstring& path) const {
    (void)path;
    return L"default";
}

// ---- file_watcher ---------------------------------------------------------

file_watcher::file_watcher() = default;
file_watcher::~file_watcher() { unwatch_all(); }

bool file_watcher::watch(const std::wstring& path, bool recursive) {
    m_watched.push_back({ path, recursive });
    return true;
}

void file_watcher::unwatch(const std::wstring& path) {
    m_watched.erase(std::remove_if(m_watched.begin(), m_watched.end(),
        [&](const watch_entry& e) { return e.path == path; }), m_watched.end());
}

void file_watcher::unwatch_all() { m_watched.clear(); }

void file_watcher::poll_events() {
    file_system_api fs;
    for (const auto& w : m_watched) {
        if (fs.directory_exists(w.path)) {
            auto entries = fs.list_directory(w.path);
            for (const auto& e : entries) {
                file_watch_event ev;
                ev.type = file_watch_event::CREATED;
                ev.path = e.full_path;
                if (m_callback) m_callback(ev);
            }
        }
    }
}

} // namespace hre::storage
