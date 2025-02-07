cmake_minimum_required(VERSION 3.0)
project (mpc_tls_bench)
set(NAME "proxy_tls_bench")

macro (add_test_executable_with_lib _name libs)
    add_executable(test_${_name} "${_name}.cpp")
    target_link_libraries(test_${_name} ${OTLS_LIBRARIES} ${EMP-TOOL_LIBRARIES} ${EMP-OT_LIBRARIES} ${EMP-ZK_LIBRARIES})
endmacro()

macro (add_test_executable _name)
    add_test_executable_with_lib(${_name} "")
endmacro()

add_test_executable(prove_proxy_tls)
