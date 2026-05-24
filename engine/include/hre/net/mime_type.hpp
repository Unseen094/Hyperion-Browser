#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>

namespace hre::net {

// ---- MIME Type Class ------------------------------------------------------

struct mime_type {
    std::wstring type;       // e.g. "text"
    std::wstring subtype;    // e.g. "html"
    std::map<std::wstring, std::wstring> params; // e.g. charset=utf-8

    mime_type() = default;
    mime_type(const std::wstring& t, const std::wstring& st) : type(t), subtype(st) {}

    std::wstring to_string() const;
    std::wstring essence() const { return type + L"/" + subtype; }
    bool is_valid() const { return !type.empty() && !subtype.empty(); }
    bool operator==(const mime_type& other) const { return essence() == other.essence(); }
    bool operator!=(const mime_type& other) const { return !(*this == other); }

    bool matches(const std::wstring& pattern) const;
    bool is_text() const { return type == L"text"; }
    bool is_image() const { return type == L"image"; }
    bool is_audio() const { return type == L"audio"; }
    bool is_video() const { return type == L"video"; }
    bool is_application() const { return type == L"application"; }
    bool is_multipart() const { return type == L"multipart"; }
    bool is_font() const { return type == L"font"; }
    bool is_script() const;
    bool is_html() const { return essence() == L"text/html"; }
    bool is_json() const;
    bool is_xml() const;
    bool is_javascript() const;
    bool is_css() const { return essence() == L"text/css"; }
    bool is_image_type() const;
    bool is_media_type() const;
    bool is_executable() const;
    bool is_compressed() const;
    bool is_archive() const;
};

// ---- MIME Type Sniffing ---------------------------------------------------

class mime_sniffer {
public:
    // Sniff MIME type from content bytes (MIME sniffing algorithm)
    static mime_type sniff(const uint8_t* data, size_t size, const mime_type& supplied_type);

    // Check if binary data looks like a given type
    static bool matches_pattern(const uint8_t* data, size_t size, const std::wstring& pattern);

    // Get MIME type from file extension
    static mime_type from_extension(const std::wstring& extension);

    // Get file extension from MIME type
    static std::wstring to_extension(const mime_type& type);

    // Determine if a MIME type is safe to render
    static bool is_safe_to_render(const mime_type& type);

    // Determine if a MIME type should be treated as opaque (blocked from scripting)
    static bool is_opaque_blocked(const mime_type& type);
};

// ---- MIME Type Registry ---------------------------------------------------

class mime_registry {
public:
    static mime_registry& instance();

    void register_type(const std::wstring& extension, const mime_type& type);
    mime_type lookup_by_extension(const std::wstring& extension) const;
    std::vector<std::wstring> extensions_for_type(const mime_type& type) const;

    // Well-known MIME types
    static mime_type html() { return { L"text", L"html" }; }
    static mime_type css() { return { L"text", L"css" }; }
    static mime_type javascript() { return { L"text", L"javascript" }; }
    static mime_type json() { return { L"application", L"json" }; }
    static mime_type xml() { return { L"application", L"xml" }; }
    static mime_type png() { return { L"image", L"png" }; }
    static mime_type jpeg() { return { L"image", L"jpeg" }; }
    static mime_type gif() { return { L"image", L"gif" }; }
    static mime_type webp() { return { L"image", L"webp" }; }
    static mime_type avif() { return { L"image", L"avif" }; }
    static mime_type svg() { return { L"image", L"svg+xml" }; }
    static mime_type ico() { return { L"image", L"x-icon" }; }
    static mime_type mp4() { return { L"video", L"mp4" }; }
    static mime_type webm() { return { L"video", L"webm" }; }
    static mime_type octet_stream() { return { L"application", L"octet-stream" }; }
    static mime_type pdf() { return { L"application", L"pdf" }; }
    static mime_type wasm() { return { L"application", L"wasm" }; }
    static mime_type form_urlencoded() { return { L"application", L"x-www-form-urlencoded" }; }
    static mime_type multipart_form_data() { return { L"multipart", L"form-data" }; }

private:
    mime_registry();
    std::map<std::wstring, mime_type> m_extension_map;
    std::map<std::wstring, std::vector<std::wstring>> m_type_extensions;
};

// ---- MIME Type Validation -------------------------------------------------

class mime_validator {
public:
    // Strict MIME type parsing
    static mime_type parse(const std::wstring& mime_str);
    static bool validate(const std::wstring& mime_str);

    // Check for MIME type mismatch (nosniff)
    static bool is_nosniff_violation(const mime_type& actual, const mime_type& expected);

    // CORB protection: should a response body be blocked based on MIME type?
    static bool is_corb_protected_mime(const mime_type& type);

    // X-Content-Type-Options: nosniff enforcement
    static bool can_be_sniffed(const mime_type& type);
};

// ---- File Extension Safety ------------------------------------------------

enum class file_safety {
    SAFE,         // Safe to open/execute (images, text, etc.)
    EXECUTABLE,   // Executable files (exe, dll, etc.)
    ARCHIVE,      // Archive files (zip, tar, etc.)
    SCRIPT,       // Script files (js, py, etc.)
    DANGEROUS,    // Known dangerous types
    UNKNOWN       // Unrecognized extension
};

class file_type_detector {
public:
    static file_safety classify_extension(const std::wstring& extension);
    static file_safety classify_by_magic(const uint8_t* data, size_t size);
    static bool is_allowed_for_upload(const std::wstring& filename);
    static std::vector<std::wstring> dangerous_extensions();
    static std::vector<std::wstring> safe_extensions();

    // Check if content matches file extension
    static bool extension_matches_content(const std::wstring& extension, const uint8_t* data, size_t size);
};

} // namespace hre::net
