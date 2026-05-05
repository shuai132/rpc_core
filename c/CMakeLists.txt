cmake_minimum_required(VERSION 3.15)

project(rpc_core_c C)

option(RPC_CORE_C_BUILD_TEST "" ON)

if (WIN32)
    message(FATAL_ERROR "rpc_core_c currently provides the POSIX TCP adapter only.")
endif ()

find_package(Threads REQUIRED)

add_library(rpc_core_c
        src/rpc_core.c
        src/stream.c
        src/tcp_posix.c
)
target_include_directories(rpc_core_c PUBLIC include)
set_target_properties(rpc_core_c PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

if (RPC_CORE_C_BUILD_TEST)
    add_executable(rpc_core_c_rpc test/rpc.c)
    target_link_libraries(rpc_core_c_rpc rpc_core_c Threads::Threads)

    add_executable(rpc_core_c_rpc_s test/rpc_s.c)
    target_link_libraries(rpc_core_c_rpc_s rpc_core_c Threads::Threads)

    add_executable(rpc_core_c_rpc_c test/rpc_c.c)
    target_link_libraries(rpc_core_c_rpc_c rpc_core_c Threads::Threads)
endif ()
