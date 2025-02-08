cmake_minimum_required(VERSION 3.0)
project (mpc_tls_bench)
set(NAME "mpc_tls_bench")

# macro (add_test_executable_with_lib _name libs)
#     add_executable(test_${_name} "${_name}.cpp")
#     target_link_libraries(test_${_name} io ${OTLS_LIBRARIES} ${EMP-TOOL_LIBRARIES} ${EMP-OT_LIBRARIES} ${EMP-ZK_LIBRARIES})
# IF(ENABLE_WASM)
#     target_compile_options(test_${_name} PRIVATE -sEXPORTED_FUNCTIONS=_main)
# ENDIF()
#     message(STATUS "otls library ${OTLS_LIBRARIES}")
# endmacro()
# 
# macro (add_test_executable _name)
#     add_test_executable_with_lib(${_name} "")
# endmacro()
# 
# add_test_executable(protocol)
# add_test_executable(prot_on_off)

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


