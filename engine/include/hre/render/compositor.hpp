#pragma once

#include <hre/layout/layout_engine.hpp>
#include <map>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <d2d1_3.h>

namespace hyperion::ui {
    class renderer;
}

namespace hre::render {

struct render_layer {
    Microsoft::WRL::ComPtr<ID2D1CommandList> command_list;
    bool is_dirty = true;

    // Transform and positioning
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    float opacity = 1.0f;
    int z_index = 0;

    // Transform matrix (for CSS transform)
    D2D1::Matrix3x2F transform = D2D1::Matrix3x2F::Identity();
};

class compositor {
public:
    compositor();

    void begin_frame(hyperion::ui::renderer& r);
    void end_frame(hyperion::ui::renderer& r);

    void update_layer(const std::wstring& layer_id, const hre::layout::layout_node& root, hyperion::ui::renderer& r);
    void composite(hyperion::ui::renderer& r);

    // Layer management
    void set_layer_offset(const std::wstring& layer_id, float x, float y);
    void set_layer_scroll(const std::wstring& layer_id, float scroll_x, float scroll_y);
    void set_layer_opacity(const std::wstring& layer_id, float opacity);
    void set_layer_transform(const std::wstring& layer_id, const D2D1::Matrix3x2F& transform);
    void set_layer_z_index(const std::wstring& layer_id, int z_index);

    // Mark layer as needing update
    void mark_dirty(const std::wstring& layer_id);

    // Clear all layers
    void clear_layers();

private:
    std::map<std::wstring, render_layer> m_layers;
    std::vector<std::wstring> m_sorted_layers; // sorted by z-index
};

} // namespace hre::render
