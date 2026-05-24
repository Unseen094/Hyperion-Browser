#include <hyperion/ui/vector_icon.hpp>
#include <cmath>

namespace hyperion::ui {

vector_icon_cache::vector_icon_cache() = default;
vector_icon_cache::~vector_icon_cache() = default;

void vector_icon_cache::initialize(renderer& r) {
    m_factory = r.d2d_factory();
    if (!m_factory) return;

    m_geometries[L"back"] = create_back_arrow(m_factory.Get());
    m_geometries[L"forward"] = create_forward_arrow(m_factory.Get());
    m_geometries[L"refresh"] = create_refresh(m_factory.Get());
    m_geometries[L"home"] = create_home(m_factory.Get());
    m_geometries[L"star"] = create_star(m_factory.Get());
    m_geometries[L"search"] = create_search(m_factory.Get());
    m_geometries[L"close"] = create_close(m_factory.Get());
    m_geometries[L"plus"] = create_plus(m_factory.Get());
    m_initialized = true;
}

void vector_icon_cache::render_icon(const std::wstring& name, ID2D1DeviceContext5* ctx,
                                     const D2D1_RECT_F& rect, const D2D1::ColorF& color,
                                     float opacity) {
    auto it = m_geometries.find(name);
    if (it != m_geometries.end()) {
        render_path_geometry(ctx, it->second, rect, color, opacity);
    }
}

vector_icon_cache::path_geometry vector_icon_cache::create_back_arrow(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(12.0f, 12.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(2.0f, 12.0f));
    sink->AddLine(D2D1::Point2F(2.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(12.0f, 10.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->BeginFigure(D2D1::Point2F(12.0f, 12.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(2.0f, 12.0f));
    sink->AddLine(D2D1::Point2F(2.0f, 14.0f));
    sink->AddLine(D2D1::Point2F(12.0f, 14.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_forward_arrow(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(0.0f, 10.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(10.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(10.0f, 12.0f));
    sink->AddLine(D2D1::Point2F(0.0f, 12.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->BeginFigure(D2D1::Point2F(0.0f, 12.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(10.0f, 12.0f));
    sink->AddLine(D2D1::Point2F(10.0f, 14.0f));
    sink->AddLine(D2D1::Point2F(0.0f, 14.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_refresh(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(6.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(2.0f, 6.0f));
    sink->AddLine(D2D1::Point2F(6.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(6.0f, 7.0f));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(8.0f, 7.0f), D2D1::Point2F(10.0f, 9.0f), D2D1::Point2F(10.0f, 12.0f)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(10.0f, 15.0f), D2D1::Point2F(8.0f, 17.0f), D2D1::Point2F(6.0f, 17.0f)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(4.0f, 17.0f), D2D1::Point2F(2.0f, 15.0f), D2D1::Point2F(2.0f, 12.0f)));
    sink->AddLine(D2D1::Point2F(0.0f, 12.0f));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(0.0f, 16.0f), D2D1::Point2F(3.0f, 19.0f), D2D1::Point2F(6.0f, 19.0f)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(9.0f, 19.0f), D2D1::Point2F(12.0f, 16.0f), D2D1::Point2F(12.0f, 12.0f)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(12.0f, 9.0f), D2D1::Point2F(10.5f, 6.5f), D2D1::Point2F(6.0f, 5.0f)));
    sink->AddLine(D2D1::Point2F(6.0f, 2.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_home(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(2.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(4.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(4.0f, 20.0f));
    sink->AddLine(D2D1::Point2F(9.0f, 20.0f));
    sink->AddLine(D2D1::Point2F(9.0f, 14.0f));
    sink->AddLine(D2D1::Point2F(15.0f, 14.0f));
    sink->AddLine(D2D1::Point2F(15.0f, 20.0f));
    sink->AddLine(D2D1::Point2F(20.0f, 20.0f));
    sink->AddLine(D2D1::Point2F(20.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(22.0f, 10.0f));
    sink->AddLine(D2D1::Point2F(12.0f, 2.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_star(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(12.0f, 2.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(15.0f, 9.0f));
    sink->AddLine(D2D1::Point2F(22.0f, 9.0f));
    sink->AddLine(D2D1::Point2F(17.0f, 14.0f));
    sink->AddLine(D2D1::Point2F(19.0f, 22.0f));
    sink->AddLine(D2D1::Point2F(12.0f, 17.0f));
    sink->AddLine(D2D1::Point2F(5.0f, 22.0f));
    sink->AddLine(D2D1::Point2F(7.0f, 14.0f));
    sink->AddLine(D2D1::Point2F(2.0f, 9.0f));
    sink->AddLine(D2D1::Point2F(9.0f, 9.0f));
    sink->AddLine(D2D1::Point2F(12.0f, 2.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_search(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    auto center = D2D1::Point2F(10.0f, 10.0f);
    const float r1 = 7.0f;
    const int num_segments = 8;
    for (int i = 0; i < num_segments; ++i) {
        float a1 = 2.0f * 3.14159f * i / num_segments;
        float a2 = 2.0f * 3.14159f * (i + 1) / num_segments;
        if (i == 0) {
            sink->BeginFigure(D2D1::Point2F(center.x + static_cast<float>(r1 * std::cos(static_cast<double>(a1))), center.y + static_cast<float>(r1 * std::sin(static_cast<double>(a1)))),
                              D2D1_FIGURE_BEGIN_FILLED);
        }
        sink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(center.x + static_cast<float>(r1 * std::cos(static_cast<double>(a2))), center.y + static_cast<float>(r1 * std::sin(static_cast<double>(a2)))),
            D2D1::SizeF(r1, r1), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL
        ));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->BeginFigure(D2D1::Point2F(14.0f, 14.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(14.5f, 13.5f));
    sink->AddLine(D2D1::Point2F(22.5f, 21.5f));
    sink->AddLine(D2D1::Point2F(22.0f, 22.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_close(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(5.5f, 5.5f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(6.5f, 5.0f));
    sink->AddLine(D2D1::Point2F(19.0f, 17.5f));
    sink->AddLine(D2D1::Point2F(18.5f, 18.5f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->BeginFigure(D2D1::Point2F(18.5f, 5.5f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(17.5f, 5.0f));
    sink->AddLine(D2D1::Point2F(5.0f, 17.5f));
    sink->AddLine(D2D1::Point2F(5.5f, 18.5f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

vector_icon_cache::path_geometry vector_icon_cache::create_plus(ID2D1Factory7* factory) {
    path_geometry pg = {};
    ComPtr<ID2D1PathGeometry1> geo;
    factory->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(D2D1::Point2F(11.0f, 4.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(13.0f, 4.0f));
    sink->AddLine(D2D1::Point2F(13.0f, 11.0f));
    sink->AddLine(D2D1::Point2F(20.0f, 11.0f));
    sink->AddLine(D2D1::Point2F(20.0f, 13.0f));
    sink->AddLine(D2D1::Point2F(13.0f, 13.0f));
    sink->AddLine(D2D1::Point2F(13.0f, 20.0f));
    sink->AddLine(D2D1::Point2F(11.0f, 20.0f));
    sink->AddLine(D2D1::Point2F(11.0f, 13.0f));
    sink->AddLine(D2D1::Point2F(4.0f, 13.0f));
    sink->AddLine(D2D1::Point2F(4.0f, 11.0f));
    sink->AddLine(D2D1::Point2F(11.0f, 11.0f));
    sink->AddLine(D2D1::Point2F(11.0f, 4.0f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    pg.geometry = geo;
    pg.bounds = D2D1::RectF(0, 0, 24, 24);
    return pg;
}

void vector_icon_cache::render_path_geometry(ID2D1DeviceContext5* ctx, const path_geometry& pg,
                                              const D2D1_RECT_F& rect, const D2D1::ColorF& color,
                                              float opacity) {
    D2D1::ColorF final_color = color;
    final_color.a *= opacity;

    ComPtr<ID2D1SolidColorBrush> brush;
    ctx->CreateSolidColorBrush(final_color, &brush);

    float sx = (rect.right - rect.left) / (pg.bounds.right - pg.bounds.left);
    float sy = (rect.bottom - rect.top) / (pg.bounds.bottom - pg.bounds.top);
    float tx = rect.left - pg.bounds.left * sx;
    float ty = rect.top - pg.bounds.top * sy;

    auto old_transform = D2D1::Matrix3x2F::Identity();
    ctx->GetTransform(&old_transform);
    ctx->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy, D2D1::Point2F(0, 0)) *
                      D2D1::Matrix3x2F::Translation(tx, ty) * old_transform);

    ctx->FillGeometry(pg.geometry.Get(), brush.Get());
    ctx->SetTransform(old_transform);
}

void vector_icon_cache::render_back_arrow(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                           const D2D1::ColorF& color, float opacity) {
    render_icon(L"back", ctx, rect, color, opacity);
}

void vector_icon_cache::render_forward_arrow(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                              const D2D1::ColorF& color, float opacity) {
    render_icon(L"forward", ctx, rect, color, opacity);
}

void vector_icon_cache::render_refresh(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                        const D2D1::ColorF& color, float opacity) {
    render_icon(L"refresh", ctx, rect, color, opacity);
}

void vector_icon_cache::render_home(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                     const D2D1::ColorF& color, float opacity) {
    render_icon(L"home", ctx, rect, color, opacity);
}

void vector_icon_cache::render_star(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                     const D2D1::ColorF& color, bool filled, float opacity) {
    render_icon(L"star", ctx, rect, color, opacity);
}

void vector_icon_cache::render_search(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                       const D2D1::ColorF& color, float opacity) {
    render_icon(L"search", ctx, rect, color, opacity);
}

void vector_icon_cache::render_close(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                      const D2D1::ColorF& color, float opacity) {
    render_icon(L"close", ctx, rect, color, opacity);
}

void vector_icon_cache::render_plus(ID2D1DeviceContext5* ctx, const D2D1_RECT_F& rect,
                                     const D2D1::ColorF& color, float opacity) {
    render_icon(L"plus", ctx, rect, color, opacity);
}

} // namespace hyperion::ui
