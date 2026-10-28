cmake_minimum_required(VERSION 3.20)
project(libsocket LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core static library target
add_library(libsocket STATIC
    src/InetAddress.cpp
    src/Socket.cpp
    src/ServerSocket.cpp
    src/SocketStream.cpp
    src/DatagramPacket.cpp
    src/DatagramSocket.cpp
)

target_include_directories(libsocket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Examples
set(EXAMPLES
    tcp_server
    tcp_client
    tcp_concurrent_server
    udp_server
    udp_client
    stream_demo
    generic_type_demo
)

foreach(EXAMPLE IN LISTS EXAMPLES)
    add_executable(${EXAMPLE} examples/${EXAMPLE}.cpp)
    target_link_libraries(${EXAMPLE} PRIVATE libsocket)
endforeach()

# Tests
enable_testing()
add_executable(test_runner tests/test_runner.cpp)
target_link_libraries(test_runner PRIVATE libsocket)
add_test(NAME libsocket_tests COMMAND test_runner)
