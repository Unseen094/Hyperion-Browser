add_library(hre STATIC)

# ---- Rust HTML Tokenizer static library ----
set(RUST_TOKENIZER_LIB
    "${CMAKE_SOURCE_DIR}/hyperion_html_tokenizer/target/release/hyperion_html_tokenizer.lib"
)

target_sources(hre
    PRIVATE
        src/parser/html_parser.cpp
        src/css/css_parser.cpp
        src/style/style_engine.cpp
        src/script/script_engine.cpp
        src/net/request.cpp
        src/layout/layout_engine.cpp
        src/render/image_loader.cpp
        src/render/compositor.cpp
        src/render/painter.cpp
)

target_include_directories(hre
    PUBLIC
        include
)

target_link_libraries(hre
    PUBLIC
        hyperion_ui
        hjs
        # Rust HTML tokenizer (staticlib)
        ${RUST_TOKENIZER_LIB}
        # Rust runtime deps on Windows
        ws2_32.lib
        userenv.lib
        ntdll.lib
        bcrypt.lib
        # D2D / rendering
        d2d1.lib
        dwrite.lib
        windowscodecs.lib
        wininet.lib
)
