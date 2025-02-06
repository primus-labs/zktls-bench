cmake_minimum_required(VERSION 3.0)
project (bench)
set(NAME "bench")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/emp-base.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})

find_package(emp-zk REQUIRED)
include_directories(${EMP-ZK_INCLUDE_DIRS})

find_package(otls REQUIRED)
include_directories(${OTLS_INCLUDE_DIRS})

add_subdirectory(mpc_tls)
