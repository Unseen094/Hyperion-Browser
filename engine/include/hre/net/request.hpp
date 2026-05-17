#pragma once

#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>

namespace hre::net {

class request {
public:
    static std::wstring fetch(const std::wstring& url);
};

} // namespace hre::net
