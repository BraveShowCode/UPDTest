cmake_minimum_required(VERSION 3.16)
project(udp_zmq_discovery LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(udp_discovery_common src/common.cpp)
target_include_directories(udp_discovery_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(udp_discovery_common PRIVATE -Wall -Wextra -pedantic)

add_executable(udp_multicast_server src/server.cpp)
target_link_libraries(udp_multicast_server PRIVATE udp_discovery_common)
target_compile_options(udp_multicast_server PRIVATE -Wall -Wextra -pedantic)

add_executable(udp_multicast_client src/client.cpp)
target_link_libraries(udp_multicast_client PRIVATE udp_discovery_common)
target_compile_options(udp_multicast_client PRIVATE -Wall -Wextra -pedantic)
