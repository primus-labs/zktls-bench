cmake_minimum_required(VERSION 3.0)
project (mpc_tls_bench)
set(NAME "mpc_tls_bench")

set(sources
  bench.cpp
  protocol.cpp
  prot_on_off.cpp
  prove_proxy_tls.cpp
  )

add_executable(zktls_bench ${sources})
target_link_libraries(zktls_bench io ${OTLS_LIBRARIES} ${EMP-TOOL_LIBRARIES} ${EMP-OT_LIBRARIES} ${EMP-ZK_LIBRARIES})
IF(ENABLE_WASM)
    target_compile_options(zktls_bench PRIVATE -sEXPORTED_FUNCTIONS=_main)
ENDIF()


