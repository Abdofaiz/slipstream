cmake_minimum_required(VERSION 3.13)
project(slipstream C)

set(CMAKE_C_STANDARD 23)

set(PICOQUIC_FETCH_PTLS ON)

set(DISABLE_DEBUG_PRINTF OFF)
set(ENABLE_ASAN OFF)
set(ENABLE_UBSAN OFF)
set(BUILD_DEMO OFF)
set(BUILD_HTTP OFF)
set(BUILD_LOGREADER OFF)

set(BUILD_TESTING OFF)

set(PICOQUIC_ADDITIONAL_C_FLAGS "" CACHE STRING "Additional C flags for picoquic")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    list(APPEND PICOQUIC_COMPILE_DEFINITIONS BUILD_LOGLIB)
    set(BUILD_LOGLIB ON)
else()
    set(BUILD_LOGLIB OFF)
endif()

if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()
add_subdirectory(extern/picoquic)

# Common source files for both client and server
set(COMMON_SOURCES
        src/slipstream_inline_dots.c
        src/slipstream_sockloop.c
        src/slipstream_utils.c
        src/slipstream_protocols.c
        src/slipstream_udp_tunnel.c
        src/slipstream_http_tunnel.c
        src/slipstream_websocket_tunnel.c
        src/slipstream_bypass.c
        src/slipstream_proxy.c
        examples/udp_network_example.c
        include/slipstream.h
        include/slipstream_inline_dots.h
        include/slipstream_server_cc.h
        include/slipstream_slot.h
        include/slipstream_sockloop.h
        include/slipstream_utils.h
        include/slipstream_protocols.h
        include/slipstream_bypass.h

        extern/lua-resty-base-encoding/b32_data.h
        extern/lua-resty-base-encoding/base32.c
        extern/lua-resty-base-encoding/modp_stdint.h
        include/lua-resty-base-encoding-base32.h

        extern/SPCDNS/src/dns.h
        extern/SPCDNS/src/codec.c
        extern/SPCDNS/src/mappings.c
        extern/SPCDNS/src/mappings.h
        extern/SPCDNS/src/netsimple.c
        extern/SPCDNS/src/netsimple.h
        extern/SPCDNS/src/output.c
        extern/SPCDNS/src/output.h
)

# Client binary
add_executable(slipstream-client
        src/slipstream_client_cli.c
        src/slipstream_client.c
        ${COMMON_SOURCES}
)
target_link_libraries(slipstream-client PRIVATE m)
target_link_libraries(slipstream-client PRIVATE picoquic-core)
target_link_libraries(slipstream-client PRIVATE ssl crypto)
if (BUILD_LOGLIB)
    target_link_libraries(slipstream-client PRIVATE picoquic-log)
endif ()
target_include_directories(slipstream-client PRIVATE include)
target_include_directories(slipstream-client PRIVATE extern)
set_picoquic_compile_settings(slipstream-client)

# Server binary
add_executable(slipstream-server
        src/slipstream_server_cli.c
        src/slipstream_server.c
        src/slipstream_server_cc.c
        ${COMMON_SOURCES}
)
target_link_libraries(slipstream-server PRIVATE m)
target_link_libraries(slipstream-server PRIVATE picoquic-core)
target_link_libraries(slipstream-server PRIVATE ssl crypto)
if (BUILD_LOGLIB)
    target_link_libraries(slipstream-server PRIVATE picoquic-log)
endif ()
target_include_directories(slipstream-server PRIVATE include)
target_include_directories(slipstream-server PRIVATE extern)
set_picoquic_compile_settings(slipstream-server)
