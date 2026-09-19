cmake_minimum_required(VERSION 3.16)
project(PeerHayes VERSION 0.1.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Embed version for --version and banners
add_compile_definitions(
  PEERHAYES_BUILD_VERSION="${PROJECT_VERSION}"
)

set(PEERHAYES_CORE_SOURCES
  src/hayes/parser.c
  src/hayes/responses.c
  src/modem/modem.c
  src/modem/escape.c
  src/session/vmcp.c
  src/directory/static_map.c
  src/util/ringbuf.c
  src/util/log.c
  src/config/config.c
  src/platform/time.c
)

add_library(peerhayes_core STATIC ${PEERHAYES_CORE_SOURCES})
target_include_directories(peerhayes_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# In-process two-modem loopback (development / CI)
add_executable(peerhayes_loopback
  src/cli/loopback_main.c
  src/transport/loopback.c
)
target_link_libraries(peerhayes_loopback PRIVATE peerhayes_core)

# Main daemon
add_executable(peerhayesd
  src/cli/peerhayesd_main.c
  src/transport/tcp.c
  src/serial/serial.c
)
target_link_libraries(peerhayesd PRIVATE peerhayes_core)
if(WIN32)
  target_link_libraries(peerhayesd PRIVATE ws2_32)
endif()

enable_testing()

add_executable(test_hayes tests/test_hayes.c)
target_link_libraries(test_hayes PRIVATE peerhayes_core)
add_test(NAME test_hayes COMMAND test_hayes)

add_executable(test_vmcp tests/test_vmcp.c)
target_link_libraries(test_vmcp PRIVATE peerhayes_core)
add_test(NAME test_vmcp COMMAND test_vmcp)

add_executable(test_loopback tests/test_loopback.c src/transport/loopback.c)
target_link_libraries(test_loopback PRIVATE peerhayes_core)
add_test(NAME test_loopback COMMAND test_loopback)

add_executable(test_config tests/test_config.c)
target_link_libraries(test_config PRIVATE peerhayes_core)
add_test(NAME test_config COMMAND test_config WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_tcp_pair tests/test_tcp_pair.c src/transport/tcp.c)
target_link_libraries(test_tcp_pair PRIVATE peerhayes_core)
if(WIN32)
  target_link_libraries(test_tcp_pair PRIVATE ws2_32)
endif()
add_test(NAME test_tcp_pair COMMAND test_tcp_pair)
