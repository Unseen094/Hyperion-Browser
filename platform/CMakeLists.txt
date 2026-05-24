add_library(hyperion_platform STATIC)

target_sources(hyperion_platform
    PRIVATE
        src/logging.cpp
        src/window.cpp
        src/process_manager.cpp
        src/ipc/pipe_channel.cpp
        src/ipc/message_dispatcher.cpp
)

target_include_directories(hyperion_platform
    PUBLIC
        include
)

target_link_libraries(hyperion_platform
    PUBLIC
        user32.lib
        gdi32.lib
        dwmapi.lib
)

# Enable modules if needed in the future
# target_compile_features(hyperion_platform PUBLIC cxx_std_23)
