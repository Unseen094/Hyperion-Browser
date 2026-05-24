cmake_minimum_required(VERSION 3.15)

# HTTP/2 module
add_library(http2 STATIC
    http2/frames.h
    http2/frames.cpp
    http2/parser.h
    http2/parser.cpp
    http2/stream.h
    http2/stream.cpp
)

target_include_directories(http2 PRIVATE .)

# UDP transport module
target_sources(http2 PRIVATE
    udp/udp_transport.h
    udp/udp_transport.cpp
    ../quic/quic_layer.h
    ../quic/quic_layer.cpp
)

# QUIC module
target_sources(http2 PRIVATE
    quic/quic_layer.h
    quic/quic_layer.cpp
)

# Ensure C++17 for std::bit and other modern features
target_compile_features(http2 PRIVATE cxx_std_17)

# Platform-specific sources
if(WIN32)
    target_sources(http2 PRIVATE
        udp/udp_transport_win32.cpp
    )
elseif(UNIX)
    target_sources(http2 PRIVATE
        udp/udp_transport_unix.cpp
    )
endif()

# Test executable
add_executable(http2_test
    tests/test_http2.cpp
    tests/test_quic.cpp
    tests/test_udp.cpp
)

target_link_libraries(http2_test PRIVATE http2)

target_compile_features(http2_test PRIVATE cxx_std_17)

target_include_directories(http2_test PRIVATE .)

if(WIN32)
    target_link_libraries(http2_test PRIVATE ws2_32)
endif()
