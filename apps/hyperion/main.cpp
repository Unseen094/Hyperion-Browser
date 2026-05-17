#include <hyperion/browser/application.hpp>
#include <hyperion/platform/logging.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <exception>
#include <stdexcept>

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
) {
    // Unreferenced parameters
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        HYPERION_LOG_ERROR("COM initialization failed with HRESULT: {}", hr);
        return 1;
    }

    int result = 0;

    try {
        hyperion::browser::application app;
        result = app.run();
    } catch (const std::exception& e) {
        HYPERION_LOG_ERROR("Unhandled exception: {}", e.what());
        result = 1;
    } catch (...) {
        HYPERION_LOG_ERROR("Unknown unhandled exception");
        result = 1;
    }

    CoUninitialize();
    return result;
}
