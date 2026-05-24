#include <hre/net/mime_type.hpp>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace hre::net {

// ---- mime_type methods ----------------------------------------------------

std::wstring mime_type::to_string() const {
    std::wstring result = type + L"/" + subtype;
    for (const auto& [k, v] : params) {
        result += L"; " + k + L"=" + v;
    }
    return result;
}

bool mime_type::matches(const std::wstring& pattern) const {
    if (pattern == L"*/*") return true;
    size_t slash = pattern.find(L'/');
    if (slash == std::wstring::npos) return false;
    std::wstring ptype = pattern.substr(0, slash);
    std::wstring psub = pattern.substr(slash + 1);
    if (ptype == L"*") return psub == L"*" || psub == subtype;
    return type == ptype && (psub == L"*" || psub == subtype);
}

bool mime_type::is_script() const {
    return essence() == L"text/javascript" ||
           essence() == L"application/javascript" ||
           essence() == L"text/ecmascript" ||
           essence() == L"application/ecmascript";
}

bool mime_type::is_json() const {
    return essence() == L"application/json" ||
           essence() == L"application/vnd.api+json" ||
           (essence().find(L"+json") != std::wstring::npos);
}

bool mime_type::is_xml() const {
    return essence() == L"text/xml" ||
           essence() == L"application/xml" ||
           (essence().find(L"+xml") != std::wstring::npos);
}

bool mime_type::is_javascript() const {
    return essence() == L"text/javascript" || essence() == L"application/javascript" ||
           essence() == L"application/x-javascript" || essence() == L"text/ecmascript";
}

bool mime_type::is_image_type() const {
    return type == L"image" || essence() == L"image/svg+xml";
}

bool mime_type::is_media_type() const {
    return type == L"audio" || type == L"video";
}

bool mime_type::is_executable() const {
    return essence() == L"application/x-msdownload" ||
           essence() == L"application/x-msdos-program" ||
           essence() == L"application/x-sh" ||
           essence() == L"application/x-csh";
}

bool mime_type::is_compressed() const {
    return essence() == L"application/gzip" ||
           essence() == L"application/x-gzip" ||
           essence() == L"application/x-bzip" ||
           essence() == L"application/x-xz";
}

bool mime_type::is_archive() const {
    return essence() == L"application/zip" ||
           essence() == L"application/x-tar" ||
           essence() == L"application/x-rar-compressed" ||
           essence() == L"application/x-7z-compressed";
}

// ---- mime_sniffer ---------------------------------------------------------

mime_type mime_sniffer::sniff(const uint8_t* data, size_t size, const mime_type& supplied_type) {
    if (size == 0) return supplied_type;

    if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8) return mime_registry::jpeg();
    if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return mime_registry::png();
    if (size >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F') return mime_registry::gif();
    if (size >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 'f') return mime_registry::mp4();
    if (size >= 4 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') {
        if (size >= 12 && data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') return mime_registry::webp();
    }
    if (size >= 12 && data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p' &&
        data[8] == 'a' && data[9] == 'v' && data[10] == 'i' && data[11] == 'f') return mime_registry::avif();
    if (size >= 4 && data[0] == '<' && data[1] == 's' && data[2] == 'v' && data[3] == 'g') return mime_registry::svg();

    if (supplied_type.is_valid()) return supplied_type;
    return mime_registry::octet_stream();
}

bool mime_sniffer::matches_pattern(const uint8_t* data, size_t size, const std::wstring& pattern) {
    if (pattern == L"image/jpeg" && size >= 2) return data[0] == 0xFF && data[1] == 0xD8;
    if (pattern == L"image/png" && size >= 8) return data[0] == 0x89 && data[1] == 'P';
    if (pattern == L"image/gif" && size >= 3) return data[0] == 'G' && data[1] == 'I';
    if (pattern == L"application/pdf" && size >= 5) return data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F';
    return false;
}

mime_type mime_sniffer::from_extension(const std::wstring& ext) {
    return mime_registry::instance().lookup_by_extension(ext);
}

std::wstring mime_sniffer::to_extension(const mime_type& type) {
    auto exts = mime_registry::instance().extensions_for_type(type);
    return exts.empty() ? L"bin" : exts[0];
}

bool mime_sniffer::is_safe_to_render(const mime_type& type) {
    return type.is_text() || type.is_image() || type.is_media_type() || type.is_font() ||
           type.essence() == L"application/pdf" ||
           type.essence() == L"application/json" ||
           type.essence() == L"application/xml";
}

bool mime_sniffer::is_opaque_blocked(const mime_type& type) {
    return type.is_executable() || type.essence() == L"application/x-msdownload" ||
           type.essence() == L"application/vnd.android.package-archive" ||
           type.essence() == L"application/java-archive";
}

// ---- mime_registry --------------------------------------------------------

mime_registry& mime_registry::instance() {
    static mime_registry inst;
    return inst;
}

mime_registry::mime_registry() {
    register_type(L"html", html());
    register_type(L"htm", html());
    register_type(L"css", css());
    register_type(L"js", javascript());
    register_type(L"mjs", javascript());
    register_type(L"json", json());
    register_type(L"xml", xml());
    register_type(L"png", png());
    register_type(L"jpg", jpeg());
    register_type(L"jpeg", jpeg());
    register_type(L"gif", gif());
    register_type(L"webp", webp());
    register_type(L"avif", avif());
    register_type(L"svg", svg());
    register_type(L"ico", ico());
    register_type(L"mp4", mp4());
    register_type(L"webm", webm());
    register_type(L"pdf", pdf());
    register_type(L"wasm", wasm());
    register_type(L"txt", { L"text", L"plain" });
    register_type(L"woff", { L"font", L"woff" });
    register_type(L"woff2", { L"font", L"woff2" });
    register_type(L"ttf", { L"font", L"ttf" });
    register_type(L"otf", { L"font", L"otf" });
    register_type(L"zip", { L"application", L"zip" });
    register_type(L"tar", { L"application", L"x-tar" });
    register_type(L"gz", { L"application", L"gzip" });
}

void mime_registry::register_type(const std::wstring& extension, const mime_type& type) {
    m_extension_map[extension] = type;
    m_type_extensions[type.essence()].push_back(extension);
}

mime_type mime_registry::lookup_by_extension(const std::wstring& ext) const {
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    auto it = m_extension_map.find(lower);
    return it != m_extension_map.end() ? it->second : octet_stream();
}

std::vector<std::wstring> mime_registry::extensions_for_type(const mime_type& type) const {
    auto it = m_type_extensions.find(type.essence());
    return it != m_type_extensions.end() ? it->second : std::vector<std::wstring>();
}

// ---- mime_validator -------------------------------------------------------

mime_type mime_validator::parse(const std::wstring& mime_str) {
    mime_type result;
    size_t slash = mime_str.find(L'/');
    if (slash == std::wstring::npos) return result;
    result.type = mime_str.substr(0, slash);
    size_t semi = mime_str.find(L';', slash + 1);
    result.subtype = mime_str.substr(slash + 1, semi - slash - 1);
    while (!result.subtype.empty() && result.subtype.back() == L' ') result.subtype.pop_back();

    if (semi != std::wstring::npos) {
        size_t pos = semi + 1;
        while (pos < mime_str.size()) {
            while (pos < mime_str.size() && mime_str[pos] == L' ') ++pos;
            size_t eq = mime_str.find(L'=', pos);
            if (eq == std::wstring::npos) break;
            std::wstring key = mime_str.substr(pos, eq - pos);
            size_t next = mime_str.find(L';', eq + 1);
            std::wstring val = mime_str.substr(eq + 1, next - eq - 1);
            while (!val.empty() && val.back() == L' ') val.pop_back();
            result.params[key] = val;
            pos = (next == std::wstring::npos) ? mime_str.size() : next + 1;
        }
    }

    return result;
}

bool mime_validator::validate(const std::wstring& mime_str) {
    size_t slash = mime_str.find(L'/');
    if (slash == std::wstring::npos || slash == 0 || slash == mime_str.size() - 1) return false;
    return true;
}

bool mime_validator::is_nosniff_violation(const mime_type& actual, const mime_type& expected) {
    if (!expected.is_valid()) return false;
    if (expected.is_javascript() && !actual.is_javascript()) return true;
    if (expected.is_css() && !actual.is_css()) return true;
    return false;
}

bool mime_validator::is_corb_protected_mime(const mime_type& type) {
    return type.is_json() || type.is_xml() || type.is_html() ||
           type.essence() == L"text/plain" ||
           type.essence() == L"application/pdf" ||
           type.essence() == L"text/xml";
}

bool mime_validator::can_be_sniffed(const mime_type& type) {
    return !type.is_javascript() && !type.is_css() && !type.is_image_type() &&
           !type.is_audio() && !type.is_video() && !type.is_font();
}

// ---- file_type_detector ---------------------------------------------------

file_safety file_type_detector::classify_extension(const std::wstring& ext) {
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    static const std::set<std::wstring> safe = {
        L"txt", L"md", L"csv", L"json", L"xml", L"html", L"htm",
        L"css", L"js", L"mjs", L"png", L"jpg", L"jpeg", L"gif",
        L"webp", L"avif", L"svg", L"ico", L"bmp", L"pdf",
        L"mp4", L"webm", L"ogg", L"wav", L"mp3", L"flac"
    };
    static const std::set<std::wstring> exec = {
        L"exe", L"dll", L"sys", L"com", L"bat", L"cmd", L"ps1",
        L"vbs", L"scr", L"pif", L"msi", L"app"
    };
    static const std::set<std::wstring> scripts = {
        L"js", L"vbs", L"py", L"pl", L"rb", L"sh", L"bash",
        L"ps1", L"php", L"asp", L"jsp"
    };
    static const std::set<std::wstring> archives = {
        L"zip", L"tar", L"gz", L"bz2", L"xz", L"rar", L"7z",
        L"z", L"arj"
    };
    static const std::set<std::wstring> dangerous = {
        L"jar", L"apk", L"appx", L"msi", L"msp", L"cab",
        L"reg", L"wsf", L"wsh", L"hta"
    };

    if (safe.count(lower)) return file_safety::SAFE;
    if (exec.count(lower)) return file_safety::EXECUTABLE;
    if (scripts.count(lower)) return file_safety::SCRIPT;
    if (archives.count(lower)) return file_safety::ARCHIVE;
    if (dangerous.count(lower)) return file_safety::DANGEROUS;
    return file_safety::UNKNOWN;
}

file_safety file_type_detector::classify_by_magic(const uint8_t* data, size_t size) {
    if (size < 2) return file_safety::UNKNOWN;
    if (data[0] == 0x4D && data[1] == 0x5A) return file_safety::EXECUTABLE; // MZ
    if (size >= 4 && data[0] == 0x50 && data[1] == 0x4B) return file_safety::ARCHIVE; // PK
    if (size >= 4 && data[0] == 0x25 && data[1] == 0x50 && data[2] == 0x44 && data[3] == 0x46) return file_safety::SAFE; // PDF
    return file_safety::UNKNOWN;
}

bool file_type_detector::is_allowed_for_upload(const std::wstring& filename) {
    size_t dot = filename.find_last_of(L'.');
    if (dot == std::wstring::npos) return true;
    std::wstring ext = filename.substr(dot + 1);
    auto safety = classify_extension(ext);
    return safety == file_safety::SAFE || safety == file_safety::UNKNOWN;
}

std::vector<std::wstring> file_type_detector::dangerous_extensions() {
    return { L"exe", L"dll", L"bat", L"cmd", L"ps1", L"vbs", L"scr", L"jar", L"apk", L"msi" };
}

std::vector<std::wstring> file_type_detector::safe_extensions() {
    return { L"txt", L"md", L"html", L"css", L"js", L"png", L"jpg", L"gif", L"pdf", L"svg" };
}

bool file_type_detector::extension_matches_content(const std::wstring& ext, const uint8_t* data, size_t size) {
    mime_type expected = mime_sniffer::from_extension(ext);
    mime_type actual = mime_sniffer::sniff(data, size, expected);
    return expected == actual;
}

} // namespace hre::net
