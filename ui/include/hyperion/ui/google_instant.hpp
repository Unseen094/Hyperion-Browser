#pragma once

#include <hyperion/ui/omnibox.hpp>
#include <hyperion/ui/renderer.hpp>
#include <string>
#include <functional>

namespace hyperion::ui {

class google_instant_api {
public:
    google_instant_api();
    ~google_instant_api();

    void fetch_suggestions(const std::wstring& query,
        std::function<void(const std::vector<suggestion_entry>&)> callback);

    void set_enabled(bool e) { m_enabled = e; }
    bool is_enabled() const { return m_enabled; }

private:
    std::wstring m_endpoint = L"https://suggestqueries.google.com/complete/search";
    bool m_enabled = true;
};

class lottie_player {
public:
    lottie_player();
    ~lottie_player();

    bool load_from_json(const std::wstring& json_data);
    void render(renderer& r, const D2D1_RECT_F& rect, float time_sec = -1.0f);
    void play();
    void pause();
    void stop();
    bool is_playing() const { return m_playing; }
    void set_loop(bool l) { m_loop = l; }

private:
    bool m_playing = false;
    bool m_loop = true;
    float m_duration = 1.0f;
    float m_current_time = 0.0f;
    std::vector<std::pair<float, D2D1_ELLIPSE>> m_frames; // simplified shape cache
};

} // namespace hyperion::ui
