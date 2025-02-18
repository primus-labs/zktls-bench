cmake_minimum_required(VERSION 3.0)
project (bench)
set(NAME "bench")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

IF(ENABLE_WASM OR ENABLE_ANDROID)
  set(CMAKE_FOLDER ${CMAKE_INSTALL_PREFIX})
  include(${CMAKE_FOLDER}/cmake/emp-base.cmake)

  find_package(OpenSSL REQUIRED)
  include_directories(${OPENSSL_INCLUDE_DIR})

  set(EMP-TOOL_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include)
  set(EMP-TOOL_LIBRARIES ${CMAKE_INSTALL_PREFIX}/lib/libemp-tool.a ${OPENSSL_LIBRARIES})
  include_directories(${EMP-TOOL_INCLUDE_DIRS})

  set(EMP-OT_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include)
  set(EMP-OT_LIBRARIES "")
  include_directories(${EMP-OT_INCLUDE_DIRS})

  set(EMP-ZK_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include)
  set(EMP-ZK_LIBRARIES ${CMAKE_INSTALL_PREFIX}/lib/libemp-zk.a)
  include_directories(${EMP-ZK_INCLUDE_DIRS})

  set(OTLS_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include)
  set(OTLS_LIBRARIES ${CMAKE_INSTALL_PREFIX}/lib/libotls.a)
  include_directories(${OTLS_INCLUDE_DIRS})
ELSE()
  find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
  include(${CMAKE_FOLDER}/cmake/emp-base.cmake)
  
  find_package(emp-ot REQUIRED)
  include_directories(${EMP-OT_INCLUDE_DIRS})
  
  find_package(emp-zk REQUIRED)
  include_directories(${EMP-ZK_INCLUDE_DIRS})
  
  find_package(otls REQUIRED)
ENDIF()
include_directories(${OTLS_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/network)

option(USE_WEBSOCKET_IO "" ON)
IF(USE_WEBSOCKET_IO)
  add_definitions(-DUSE_WEBSOCKET_IO=1)
ENDIF()

add_subdirectory(network)
add_subdirectory(zk_tls)
