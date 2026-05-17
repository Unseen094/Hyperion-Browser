#include <hre/net/request.hpp>
#include <sstream>

namespace hre::net {

std::wstring request::fetch(const std::wstring& url) {
    HINTERNET h_internet = InternetOpenW(L"HyperionBrowser/0.1", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!h_internet) return L"<html><body><h1>Network Error</h1><p>Failed to initialize Internet session.</p></body></html>";

    HINTERNET h_url = InternetOpenUrlW(h_internet, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD, 0);
    if (!h_url) {
        InternetCloseHandle(h_internet);
        return L"<html><body><h1>404 Not Found</h1><p>Could not connect to: " + url + L"</p></body></html>";
    }

    std::string content;
    char buffer[4096];
    DWORD bytes_read = 0;
    while (InternetReadFile(h_url, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        content.append(buffer, bytes_read);
    }

    InternetCloseHandle(h_url);
    InternetCloseHandle(h_internet);

    // Convert UTF-8 to WString
    if (content.empty()) {
        return L"";
    }

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.size(), NULL, 0);
    if (size_needed <= 0) {
        return L"";
    }

    std::wstring wcontent(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, content.c_str(), (int)content.size(), &wcontent[0], size_needed);

    return wcontent;
}

} // namespace hre::net
