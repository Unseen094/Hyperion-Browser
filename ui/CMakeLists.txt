add_library(hyperion_ui STATIC)

target_sources(hyperion_ui
    PRIVATE
        src/renderer.cpp
        src/toolbar.cpp
        src/bookmarks_bar.cpp
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
)
