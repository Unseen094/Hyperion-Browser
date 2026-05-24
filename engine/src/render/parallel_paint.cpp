#define NOMINMAX
#include <hre/render/parallel_paint.hpp>

namespace hre::render {

parallel_paint::parallel_paint() {
    m_thread_pool = std::make_unique<threading::thread_pool>();
}

parallel_paint::parallel_paint(size_t num_threads) {
    m_thread_pool = std::make_unique<threading::thread_pool>(num_threads);
}

parallel_paint::~parallel_paint() {
    wait_all();
}

void parallel_paint::paint(display_list& list, void* render_context) {
    if (m_thread_pool) {
        size_t num_items = list.item_count();
        size_t chunk_size = std::max(size_t(1), num_items / m_thread_pool->num_threads());

        for (size_t i = 0; i < num_items; i += chunk_size) {
            size_t start = i;
            size_t end = std::min(i + chunk_size, num_items);

            m_thread_pool->enqueue([this, start, end, &list, render_context]() {
                execute_paint_range(start, end, list, render_context);
            });
        }

        m_thread_pool->wait_all();
    }
}

void parallel_paint::set_num_threads(size_t num_threads) {
    if (m_thread_pool) {
        m_thread_pool->set_num_threads(num_threads);
    }
}

size_t parallel_paint::num_threads() const {
    return m_thread_pool ? m_thread_pool->num_threads() : 0;
}

void parallel_paint::wait_all() {
    if (m_thread_pool) {
        m_thread_pool->wait_all();
    }
}

void parallel_paint::execute_paint_range(size_t start, size_t end, display_list& list, void* context) {
    const auto& items = list.items();
    for (size_t i = start; i < end && i < items.size(); ++i) {
        if (m_paint_callback) {
            m_paint_callback(items[i], context);
        }
    }
}

layer_painter::layer_painter() {
    m_thread_pool = std::make_unique<threading::thread_pool>();
}

layer_painter::layer_painter(size_t num_threads) {
    m_thread_pool = std::make_unique<threading::thread_pool>(num_threads);
}

void layer_painter::paint_layers(std::vector<display_list>& layers, void* render_context) {
    if (!m_thread_pool) return;

    size_t batch_size = max_layers_per_batch();
    for (size_t i = 0; i < layers.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, layers.size());

        m_thread_pool->enqueue([this, &layers, i, end, render_context]() {
            for (size_t j = i; j < end; ++j) {
                layers[j].build_from_layout(layers[j].items().empty() ?
                    *reinterpret_cast<const layout::LayoutNode*>(nullptr) : *reinterpret_cast<const layout::LayoutNode*>(nullptr));
            }
        });
    }

    m_thread_pool->wait_all();
}

} // namespace hre::render