#pragma once

#include <hyperion/ui/renderer.hpp>
#include <unordered_map>
#include <string>
#include <memory>
#include <d2d1_3.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

namespace hyperion::ui {

struct icon_entry {
    std::wstring name;
    D2D1_RECT_F bounds;
    D2D1::ColorF color;
    float opacity;
};

class vector_icon_cache {
public:
    vector_icon_cache();
    ~vector_icon_cache();

    void initialize(renderer& r);
    void render_icon(const std::wstring& name, ID2D1DeviceContext5* ctx, 
                     const D2D1_RECT_F& rect, const D2D1::ColorF& color,
                     float opacity = 1.0f);

    void render_back_arrow(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect, 
                           const D2D1::ColorF& color, float opacity = 1.0f);
    void render_forward_arrow(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                              const D2D1::ColorF& color, float opacity = 1.0f);
    void render_refresh(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                        const D2D1::ColorF& color, float opacity = 1.0f);
    void render_home(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                     const D2D1::ColorF& color, float opacity = 1.0f);
    void render_star(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                     const D2D1::ColorF& color, bool filled = false, float opacity = 1.0f);
    void render_search(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                       const D2D1::ColorF& color, float opacity = 1.0f);
    void render_close(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                      const D2D1::ColorF& color, float opacity = 1.0f);
    void render_plus(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                     const D2D1::ColorF& color, float opacity = 1.0f);

private:
    struct path_geometry {
        ComPtr<ID2D1PathGeometry1> geometry;
        D2D1_RECT_F bounds;
    };

    path_geometry create_back_arrow(ID2D1Factory7* factory);
    path_geometry create_forward_arrow(ID2D1Factory7* factory);
    path_geometry create_refresh(ID2D1Factory7* factory);
    path_geometry create_home(ID2D1Factory7* factory);
    path_geometry create_star(ID2D1Factory7* factory);
    path_geometry create_search(ID2D1Factory7* factory);
    path_geometry create_close(ID2D1Factory7* factory);
    path_geometry create_plus(ID2D1Factory7* factory);

    void render_path_geometry(ID2D1DeviceContext5* ctx, const path_geometry& pg,
                              const D2D1_RECT_F& rect, const D2D1::ColorF& color,
                              float opacity);

    std::unordered_map<std::wstring, path_geometry> m_geometries;
    ComPtr<ID2D1Factory7> m_factory;
    bool m_initialized = false;
};

} // namespace hyperion::ui
