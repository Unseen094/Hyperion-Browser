add_library(hre STATIC)

# ---- Rust HTML Tokenizer static library ----
set(RUST_TOKENIZER_LIB
"${CMAKE_SOURCE_DIR}/hyperion_html_tokenizer/target/release/hyperion_html_tokenizer.lib"
)

target_sources(hre
PRIVATE
# Parser
src/parser/html_parser.cpp
src/parser/html_tokenizer_bridge.cpp
# CSS
src/css/css_parser.cpp
src/css/rust_css_parser.cpp
    src/css/style_engine.cpp
# src/style/style_engine.cpp (old style engine - superseded by css/style_engine.hpp)
# Layout (Module 1: Phases 15-17)
src/layout/layout_engine.cpp
src/layout/animation_controller.cpp
src/layout/box_model.cpp
src/layout/block_layout.cpp
src/layout/inline_layout.cpp
    src/layout/flexbox_engine.cpp
    src/layout/grid_engine.cpp
    src/layout/positioning_engine.cpp
    src/layout/table_engine.cpp
    src/layout/replaced_engine.cpp
    src/layout/overflow_engine.cpp
# Text shaping (Phase 15)
src/text/font_engine.cpp
src/text/text_shaping.cpp
# Render (Module 3: Phases 24-29)
src/render/image_loader.cpp
src/render/render_command.cpp
src/render/compositor.cpp
src/render/painter.cpp
src/render/display_list.cpp
src/render/text_render.cpp
src/render/gpu_texture.cpp
src/render/scroll_controller.cpp
src/render/animation_controller.cpp
src/render/parallel_paint.cpp
# Layout - Incremental (Phase 30)
src/layout/incremental_layout.cpp
# SIMD Operations (Phase 30-31)
src/simd/simd_ops.cpp
# WebGL 2.0 (Phase 21)
src/webgl/webgl_context.cpp
# Media: Video Decoder (Phase 22)
src/media/video_decoder.cpp
# Media: EME/DRM Engine (Phase 23)
src/media/eme_engine.cpp
# Media: Audio Context (Phase 24)
src/media/audio_context.cpp
# Image Decoders - AVIF/WebP2 (Phases 25-26)
src/image/image_formats.cpp
src/image/image_decoder.cpp
# CSS Filters (Phase 27)
src/css/css_filters.cpp
# Graphics: 3D Transforms & Matrices (Phase 28)
src/graphics/matrix4x4.cpp
# Graphics: Clip-path & Mask (Phase 29)
src/graphics/clip_mask.cpp
# Memory Management (Phase 34)
src/memory/memory_pool.cpp
# Benchmarking (Phase 35)
src/benchmark/benchmark.cpp
# CSS Containment (Phase 36)
src/css/containment.cpp
# Color & Variable Fonts (Phase 37)
src/css/color.cpp
src/text/variable_fonts.cpp
# SVG & MathML (Phase 38)
src/svg/svg_renderer.cpp
src/mathml/mathml_renderer.cpp
# Multi-Process & IPC (Phase 31-32)
src/ipc/ipc_channel.cpp
src/process/process_manager.cpp
src/sandbox/sandbox.cpp
# Site Isolation (Phase 32)
src/security/site_isolation.cpp
# Secure Context (Phase 34)
src/security/secure_context.cpp
# MIME Type handling (Phase 35)
src/net/mime_type.cpp
# XSS Filter (Phase 36)
src/security/xss_filter.cpp
src/net/xss_filter.cpp
# Test Suite (Phase 40)
src/test/test_suite.cpp
# Fuzzing (Phase 41)
src/fuzzing/fuzzing.cpp
# Network
src/net/request.cpp
src/net/cookie_manager.cpp
src/net/lru_cache.cpp
src/net/response_cache.cpp
src/net/webrtc_peer.cpp
src/net/ssl_socket.cpp
src/net/network_manager.cpp
src/net/websocket.cpp
# Script
src/script/script_engine.cpp
src/script/fetch_api.cpp
src/script/json_api.cpp
src/script/xml_http_request.cpp
src/script/websocket_api.cpp
src/script/indexeddb_api.cpp
# src/script/storage_api.cpp (needs hjs linkage)
# Storage
src/storage/local_storage.cpp
src/storage/session_storage.cpp
# IndexedDB Engine (Phase 37)
src/storage/indexeddb.cpp
# FileSystem API (Phase 38)
src/storage/file_system.cpp
# Cache API (Phase 39)
src/storage/cache_api.cpp
# Storage Quota Manager (Phase 40)
src/storage/quota_manager.cpp
# DOM
src/dom/event_target.cpp
src/dom/canvas_element.cpp
src/dom/events.cpp
src/dom/node.cpp
# Security
src/security/csp.cpp
# src/security/csp_enforcer_impl.cpp (needs CSP API alignment)
# DevTools
src/devtools/devtools_server.cpp
# Extensions
src/extensions/extension_manager.cpp
# Config
src/config/config.cpp
# Network Stack - Phase B: HTTP/2
src/net/http2/frame.cpp
src/net/http2/hpack.cpp
src/net/http2/stream.cpp
src/net/http2/connection.cpp
src/net/http2/client.cpp
# Network Stack - Phase B: HTTP/3 & QUIC
src/net/http3/quic_transport.cpp
src/net/http3/quic_connection.cpp
src/net/http3/quic_congestion.cpp
src/net/http3/http3_client.cpp
# Network Stack - Phase B: Service Workers
src/services/service_worker.cpp
src/services/service_worker_registration.cpp
src/services/service_worker_container.cpp
src/services/background_sync.cpp
src/services/push_api.cpp
# Network Stack - Phase B: DNS Caching & HTTP Cache (Phase 4)
src/net/dns_cache.cpp
src/net/http_cache.cpp
# Network Stack - Phase B: Resource Hints (Phase 4)
src/net/resource_hints.cpp
# Network Stack - Phase B: TLS Session Cache (Phase 5)
src/net/tls_session_cache.cpp
# Network Stack - Phase B: Security Policies (Phase 5)
src/net/security_policies.cpp
src/net/fetch.cpp
src/net/cors.cpp
# Phase 1 — HTTP/2 Priority & Flow Control
src/net/http2/h2_priority.cpp
src/net/http2/h2_flow.cpp
# Phase 3 — Happy Eyeballs
src/net/happy_eyeballs.cpp
# Phase 5 — WebSocket permessage-deflate (requires zlib)
# src/net/ws_deflate.cpp
# Phase 6 — Network Metrics
src/net/net_metrics.cpp
# Phase 10 — CSS Selectors L4 & calc()
src/css/selector_ext.cpp
src/css/calc_engine.cpp
# Phase 11 — Layout Extensions
src/layout/layout_flex_ext.cpp
src/layout/layout_grid_ext.cpp
# Phase 12 — Canvas 2D
src/render/canvas_2d.cpp
# Phase 13 — WebGL Extensions
src/webgl/webgl_context_ext.cpp
# Phase 14 — Media Source Extensions
src/media/media_source_ext.cpp
# Phase 17 — AV Render & Sync
src/render/video_renderer_d3d.cpp
src/media/audio_renderer_wasapi.cpp
# src/media/media_sync_abr.cpp (needs abr_config definition fix)
# Phase 19 — WebCodecs & WebAudio
src/media/webcodecs_enc.cpp
# src/media/webaudio_nodes.cpp (needs WASAPI COM headers)
# Phase 20 — Storage Extensions
src/storage/indexeddb_impl.cpp
src/storage/cache_api_ext.cpp
# Phase 21 — Fetch Extensions & Streams (needs API alignment)
# src/script/fetch_ext.cpp
# src/script/streams.cpp
# Phase 22 — WebGPU
src/graphics/webgpu.cpp
# Phase 23 — Accessibility Tree
src/dom/a11y_tree.cpp
# Phase 24 — DevTools CDP
src/devtools/devtools_cdp.cpp
# Phase 25 — Security Extensions
# src/security/csp_enforcer_impl.cpp (needs CSP API alignment)
src/security/cors_checker.cpp
# Phase 26 — Sandbox Extensions
# src/sandbox/sandbox_policy.cpp (needs sandbox.hpp API alignment)
# Phase 27 — PGO Profiling
src/render/profile_guided.cpp
# Phase 29 — Pool Allocator Slab
src/memory/pool_allocator_slab.cpp
# Phase 30 — Integration Test
src/core/integration_test.cpp
# Thread pool
src/threading/thread_pool.cpp
)

# C Language Sources (Codecs, Crypto, Demuxers) - excluded for now
# target_sources(hre PRIVATE
#     src/net/crypto/tls13_crypto.c
#     src/media/demux_mp4.c
#     src/media/demux_webm.c
#     src/media/decoder_h264.c
#     src/media/decoder_hevc.c
#     src/media/decoder_vp9.c
#     src/media/decoder_aac.c
#     src/media/decoder_opus.c
#     src/media/decoder_mp3.c
#     src/sandbox/syscall_filter.c
# )

# Assembly Language Sources (NASM) - excluded for now, needs NASM build integration
# target_sources(hre PRIVATE
#     src/media/idct_sse2.asm
#     src/media/mc_avx2.asm
#     src/media/mdct_avx.asm
#     asm/yuv_to_rgb_sse.asm
#     asm/stack_walk_x64.asm
#     asm/mem_ops.asm
#     asm/hash_simd.asm
#     asm/utf8_validate.asm
#     asm/base64_simd.asm
# )

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
dxgi.lib
d3d11.lib
wininet.lib
)
