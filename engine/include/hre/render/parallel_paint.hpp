#pragma once

#include <hre/render/display_list.hpp>
#include <hre/threading/thread_pool.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace hre::render {

struct paint_task {
    size_t start_index;
    size_t end_index;
    display_item* items;
    void* render_context;
};

class parallel_paint {
public:
    parallel_paint();
    explicit parallel_paint(size_t num_threads);
    ~parallel_paint();

    void paint(display_list& display_list, void* render_context);

    void set_num_threads(size_t num_threads);
    size_t num_threads() const;

    using paint_callback = std::function<void(const display_item&, void*)>;
    void set_paint_callback(paint_callback cb) { m_paint_callback = cb; }

    void wait_all();

private:
    void execute_paint_range(size_t start, size_t end, display_list& list, void* context);

    std::unique_ptr<threading::thread_pool> m_thread_pool;
    paint_callback m_paint_callback;
};

class layer_painter {
public:
    layer_painter();
    explicit layer_painter(size_t num_threads);

    void paint_layers(std::vector<display_list>& layers, void* render_context);

    void set_max_layers_per_batch(size_t max_layers) { m_max_layers_per_batch = max_layers; }
    size_t max_layers_per_batch() const { return m_max_layers_per_batch; }

private:
    std::unique_ptr<threading::thread_pool> m_thread_pool;
    size_t m_max_layers_per_batch = 4;
};

} // namespace hre::render