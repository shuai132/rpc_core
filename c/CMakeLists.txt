cmake_minimum_required(VERSION 3.15)

project(rpc_core_c C)

option(RPC_CORE_C_BUILD_TEST "" ON)

if (WIN32)
    set(RPC_CORE_C_TCP_SOURCE src/tcp_win32.c)
    set(RPC_CORE_C_PLATFORM_LIBS ws2_32)
else ()
    find_package(Threads REQUIRED)
    set(RPC_CORE_C_TCP_SOURCE src/tcp_posix.c)
    set(RPC_CORE_C_PLATFORM_LIBS Threads::Threads)
endif ()

add_library(rpc_core_c
        src/rpc_core.c
        src/stream.c
        ${RPC_CORE_C_TCP_SOURCE}
)
target_include_directories(rpc_core_c PUBLIC include)
target_link_libraries(rpc_core_c PUBLIC ${RPC_CORE_C_PLATFORM_LIBS})
set_target_properties(rpc_core_c PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

if (RPC_CORE_C_BUILD_TEST)
    add_executable(rpc_core_c_rpc test/rpc.c)
    target_link_libraries(rpc_core_c_rpc rpc_core_c)

    add_executable(rpc_core_c_rpc_s test/rpc_s.c)
    target_link_libraries(rpc_core_c_rpc_s rpc_core_c)

    add_executable(rpc_core_c_rpc_c test/rpc_c.c)
    target_link_libraries(rpc_core_c_rpc_c rpc_core_c)
endif ()
