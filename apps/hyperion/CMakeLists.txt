add_executable(hyperion
    main.cpp
)

target_link_libraries(hyperion
    PRIVATE
        hyperion_browser
)

# Set subsystem to Windows for no console window in release
set_target_properties(hyperion PROPERTIES
    WIN32_EXECUTABLE TRUE
)
