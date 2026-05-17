add_library(hyperion_browser STATIC)

target_sources(hyperion_browser
    PRIVATE
        src/application.cpp
        src/tab.cpp
        src/tab_manager.cpp
)

target_include_directories(hyperion_browser
    PUBLIC
        include
)

target_link_libraries(hyperion_browser
    PUBLIC
        hyperion_platform
        hyperion_ui
        hre
)
