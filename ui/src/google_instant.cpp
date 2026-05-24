#include <hyperion/ui/google_instant.hpp>
#include <hyperion/platform/logging.hpp>
#include <cmath>

namespace hyperion::ui {

// === google_instant_api ===

google_instant_api::google_instant_api() = default;
google_instant_api::~google_instant_api() = default;

void google_instant_api::fetch_suggestions(const std::wstring& query,
    std::function<void(const std::vector<suggestion_entry>&)> callback) {
    if (!m_enabled || query.empty()) {
        if (callback) callback({});
        return;
    }
    HYPERION_LOG_DEBUG("Google Instant suggest called");
    std::vector<suggestion_entry> results;
    results.push_back({query + L" (Google search)", L"", L"", suggestion_entry::SEARCH});
    if (callback) callback(results);
}

// === lottie_player ===

lottie_player::lottie_player() = default;
lottie_player::~lottie_player() = default;

bool lottie_player::load_from_json(const std::wstring& json_data) {
    (void)json_data;
    // Simplified: would parse Lottie JSON and create shape cache
    m_duration = 1.0f;
    for (int i = 0; i < 10; i++) {
        float t = i / 10.0f;
        m_frames.push_back({t, D2D1::Ellipse(D2D1::Point2F(50 + sinf(t * 6.28f) * 30, 50 + cosf(t * 6.28f) * 30), 5.0f, 5.0f)});
    }
    HYPERION_LOG_INFO("Lottie animation loaded with {} frames", m_frames.size());
    return true;
}

void lottie_player::render(renderer& r, const D2D1_RECT_F& rect, float time_sec) {
    auto* ctx = r.context();
    if (!ctx) return;
    auto& colors = theme_manager::instance().colors();

    float t = (time_sec >= 0) ? time_sec : m_current_time;
    if (m_frames.empty()) return;

    int frame = (int)(t / m_duration * m_frames.size()) % (int)m_frames.size();
    auto& [ft, shape] = m_frames[frame];
    (void)ft;

    D2D1_POINT_2F center = D2D1::Point2F(
        rect.left + (rect.right - rect.left) * shape.point.x / 100.0f,
        rect.top + (rect.bottom - rect.top) * shape.point.y / 100.0f);

    ComPtr<ID2D1SolidColorBrush> brush;
    ctx->CreateSolidColorBrush(colors.accent, &brush);
    ctx->FillEllipse(D2D1::Ellipse(center, 5.0f, 5.0f), brush.Get());
}

void lottie_player::play() {
    m_playing = true;
    HYPERION_LOG_INFO("Lottie animation playing");
}

void lottie_player::pause() {
    m_playing = false;
}

void lottie_player::stop() {
    m_playing = false;
    m_current_time = 0.0f;
}

} // namespace hyperion::ui
