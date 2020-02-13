cmake_minimum_required(VERSION 3.2)

set(ggflags_DIR "${CMAKE_SOURCE_DIR}/third_party/gflags")
add_subdirectory(${ggflags_DIR})
include_directories("${ggflags_DIR}/include")

project(rdpma)

option( LINK_STATIC_LIB "Link static library" true)

ADD_DEFINITIONS( -std=c++14)

include_directories(.)

## third party libraries
include_directories(third_party)

## seperate install
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)

add_executable(nvm_hello "./third_party/r2/src/logging.cc"   ./benchs/local.cc)

add_executable(nvm_client ./benchs/one_sided/client.cc ./third_party/r2/src/sshed.cc ./third_party/r2/src/logging.cc)
add_executable(nvm_server ./benchs/one_sided/server.cc)

add_executable(nvm_rrtserver ./benchs/two_sided/ring_server.cc ./third_party/r2/src/sshed.cc ./third_party/r2/src/logging.cc)
add_executable(nvm_rrtclient ./benchs/two_sided/ring_client.cc ./third_party/r2/src/sshed.cc ./third_party/r2/src/logging.cc)

include(cmake/boost.cmake)

#
#Configure binary linking
#
set(apps nvm_hello
    nvm_client nvm_server 
    nvm_rrtserver nvm_rrtclient )

find_library( LIBIBVERBS NAMES ibverbs)

foreach( prog ${apps} )
  if( LINK_STATIC_LIB )
    target_link_libraries( ${prog}
      rt ${LIBIBVERBS}
      boost_coroutine boost_chrono boost_thread boost_context boost_system gflags pthread)
  else()
    target_link_libraries( ${prog}
      ${LIBZMQ} rt ${LIBIBVERBS}
      boost_coroutine boost_system )
  endif()
  add_dependencies( ${prog} libboost1.61 )
endforeach( prog )

