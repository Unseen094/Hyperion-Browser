add_executable(hyperion_renderer
    src/main.cpp
)

target_include_directories(hyperion_renderer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/include
        ${CMAKE_SOURCE_DIR}/engine/include
        ${CMAKE_SOURCE_DIR}/ui/include
        ${CMAKE_SOURCE_DIR}/HyperionJS/include
)

target_link_libraries(hyperion_renderer
    PRIVATE
        hyperion_platform
        hre
        hjs
)

# Render process is launched in the background, keeping it as standard executable
# We can link standard libraries needed for Direct2D and Windows networking
target_link_libraries(hyperion_renderer
    PRIVATE
        user32.lib
        gdi32.lib
)
