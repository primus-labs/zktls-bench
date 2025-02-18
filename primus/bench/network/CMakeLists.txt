cmake_minimum_required(VERSION 3.0)

set(sources
  sha1.cpp
  ws.cpp
  proxy_client.cpp
  emp_io_channel.cpp
  websocket_io_channel.cpp
  common/runtime_error.cpp
  )


add_compile_options(-fPIC)
add_library(io ${sources})

install(TARGETS io DESTINATION lib)
