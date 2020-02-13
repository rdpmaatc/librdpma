include( ExternalProject )

## install Boost
set( BOOST_INSTALL_DIR ${CMAKE_SOURCE_DIR}/third_party/boost )
ExternalProject_Add( libboost1.61
#  URL $ENV{HOME}/download/boost_1_61_0.tar.bz2
  URL https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.bz2
  URL_HASH SHA256=a547bd06c2fd9a71ba1d169d9cf0339da7ebf4753849a8f7d6fdb8feee99b640
  CONFIGURE_COMMAND ./bootstrap.sh --prefix=${BOOST_INSTALL_DIR} --with-libraries=system,coroutine
  BUILD_COMMAND ./b2
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND ./b2 install
)
set( LIBBOOST_HEADERS ${BOOST_INSTALL_DIR}/include )
set( LIBBOOST_LIBRARIES ${BOOST_INSTALL_DIR}/lib )

#
# Configure dependencies: both built-in and external
#
find_library( LIBZMQ NAMES zmq HINTS $ENV{HOME}/local/zeromq/lib)
find_library( LIBIBVERBS NAMES ibverbs)
if( LINK_STATIC_LIB )
  add_library( boost_system STATIC IMPORTED )
  set_target_properties( boost_system PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_system.a
    )
  add_library( boost_coroutine STATIC IMPORTED )
  set_target_properties( boost_coroutine PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_coroutine.a
    )
  add_library( boost_chrono STATIC IMPORTED )
  set_target_properties( boost_chrono PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_chrono.a
    )
  add_library( boost_thread STATIC IMPORTED )
  set_target_properties( boost_thread PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_thread.a
    )
  add_library( boost_context STATIC IMPORTED )
  set_target_properties( boost_context PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_context.a
    )
else()
  add_library( boost_system SHARED IMPORTED )
  set_target_properties( boost_system PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_system.so
    )
  add_library( boost_coroutine SHARED IMPORTED )
  set_target_properties( boost_coroutine PROPERTIES
    IMPORTED_LOCATION ${LIBBOOST_LIBRARIES}/libboost_coroutine.so
    )
endif()
