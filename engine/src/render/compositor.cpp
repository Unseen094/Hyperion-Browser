#include <hre/render/compositor.hpp>
#include <hre/render/painter.hpp>
#include <hyperion/ui/renderer.hpp>
#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace hre::render {

compositor::compositor() = default;

void compositor::begin_frame(hyperion::ui::renderer& r) {
    (void)r;
}

void compositor::end_frame(hyperion::ui::renderer& r) {
    (void)r;
}

void compositor::update_layer(const std::wstring& layer_id, const hre::layout::LayoutNode& root, hyperion::ui::renderer& r) {
    auto& layer = m_layers[layer_id];

    auto* context = r.context();
    if (!context) return;

    // Create command list
    ComPtr<ID2D1CommandList> cmd_list;
    context->CreateCommandList(&cmd_list);
    layer.command_list = cmd_list;

    // Save current target
    ComPtr<ID2D1Image> old_target;
    context->GetTarget(&old_target);

    context->SetTarget(cmd_list.Get());

    // Apply scroll offset to the layer content
    if (layer.scroll_x != 0 || layer.scroll_y != 0) {
        D2D1::Matrix3x2F scroll = D2D1::Matrix3x2F::Translation(-layer.scroll_x, -layer.scroll_y);
        context->SetTransform(scroll);
    }

    // Paint to command list
    painter p;
    p.paint(root, r);

    // Reset transform
    context->SetTransform(D2D1::Matrix3x2F::Identity());

    cmd_list->Close();

    // Restore original target
    context->SetTarget(old_target.Get());
    layer.is_dirty = false;
}

void compositor::composite(hyperion::ui::renderer& r) {
    auto* context = r.context();
    if (!context) return;

    // Sort layers by z-index
    m_sorted_layers.clear();
    for (const auto& pair : m_layers) {
        m_sorted_layers.push_back(pair.first);
    }

    std::sort(m_sorted_layers.begin(), m_sorted_layers.end(),
        [this](const std::wstring& a, const std::wstring& b) {
            return m_layers[a].z_index < m_layers[b].z_index;
        });

    // Draw layers in order
    for (const auto& layer_id : m_sorted_layers) {
        auto& layer = m_layers[layer_id];
        if (!layer.command_list) continue;

        // Apply layer transform and position
        D2D1::Matrix3x2F transform = D2D1::Matrix3x2F::Identity();

        // Apply offset
        if (layer.offset_x != 0 || layer.offset_y != 0) {
            transform = D2D1::Matrix3x2F::Translation(layer.offset_x, layer.offset_y);
        }

        // Apply CSS transform (multiply directly)
        transform = transform * layer.transform;

        context->SetTransform(transform);

        // Apply opacity if needed
        if (layer.opacity < 1.0f) {
            D2D1_LAYER_PARAMETERS layer_params = D2D1::LayerParameters(
                D2D1::InfiniteRect(),
                nullptr,
                D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                D2D1::Matrix3x2F::Identity(),
                layer.opacity,
                nullptr,
                D2D1_LAYER_OPTIONS_NONE
            );
            context->PushLayer(layer_params, nullptr);
            context->DrawImage(layer.command_list.Get());
            context->PopLayer();
        } else {
            context->DrawImage(layer.command_list.Get());
        }
    }

    // Reset transform
    context->SetTransform(D2D1::Matrix3x2F::Identity());
}

void compositor::set_layer_offset(const std::wstring& layer_id, float x, float y) {
    m_layers[layer_id].offset_x = x;
    m_layers[layer_id].offset_y = y;
    m_layers[layer_id].is_dirty = true;
}

void compositor::set_layer_scroll(const std::wstring& layer_id, float scroll_x, float scroll_y) {
    m_layers[layer_id].scroll_x = scroll_x;
    m_layers[layer_id].scroll_y = scroll_y;
    m_layers[layer_id].is_dirty = true;
}

void compositor::set_layer_opacity(const std::wstring& layer_id, float opacity) {
    m_layers[layer_id].opacity = opacity;
}

void compositor::set_layer_transform(const std::wstring& layer_id, const D2D1::Matrix3x2F& transform) {
    m_layers[layer_id].transform = transform;
    m_layers[layer_id].is_dirty = true;
}

void compositor::set_layer_z_index(const std::wstring& layer_id, int z_index) {
    m_layers[layer_id].z_index = z_index;
}

void compositor::mark_dirty(const std::wstring& layer_id) {
    m_layers[layer_id].is_dirty = true;
}

void compositor::clear_layers() {
    m_layers.clear();
    m_sorted_layers.clear();
}

} // namespace hre::render
