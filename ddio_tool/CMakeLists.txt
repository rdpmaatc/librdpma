cmake_minimum_required(VERSION 3.2)

ADD_DEFINITIONS( -std=c++14)

set(ggflags_DIR "${CMAKE_SOURCE_DIR}/deps/gflags")
add_subdirectory(${ggflags_DIR})
include_directories("${ggflags_DIR}/include")

project(dca)

include_directories(third_party)

add_executable(setup_dca dca.cc )

target_link_libraries(setup_dca  ${CMAKE_SOURCE_DIR}/pciutils-3.5.1/lib/libpci.a udev z gflags)
