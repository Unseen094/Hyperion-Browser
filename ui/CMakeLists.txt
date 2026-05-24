add_library(hyperion_ui STATIC)

target_sources(hyperion_ui
    PRIVATE
        src/renderer.cpp
        src/toolbar.cpp
        src/vector_icon.cpp
        src/theme_manager.cpp
        src/omnibox.cpp
        src/tab_renderer.cpp
        src/modern_bookmarks_bar.cpp
        src/effects/glass_effect.cpp
        src/animation/animation_controller.cpp
        src/accessibility_handler.cpp
        src/google_instant.cpp
        src/component.cpp
        src/dpi_manager.cpp
        src/dwrite_helpers.cpp
        src/weather_widget.cpp
        src/new_tab_dashboard.cpp
        src/ai_sidebar.cpp
        src/ai_client.cpp
        src/window_chrome.cpp
)

target_include_directories(hyperion_ui
    PUBLIC
        include
)

target_link_libraries(hyperion_ui
    PUBLIC
        hyperion_platform
        d2d1.lib
        dwrite.lib
        windowscodecs.lib
        dwmapi.lib
        dxguid.lib
)

target_compile_definitions(hyperion_ui
    PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
