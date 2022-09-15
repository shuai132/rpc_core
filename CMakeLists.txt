cmake_minimum_required(VERSION 3.1)

project(RpcCore CXX)

option(RpcCore_BUILD_TEST "" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(RpcCore_BUILD_TEST ON)
endif ()

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE .)

set(EXAMPLE_COMPILE_DEFINE
        RpcCore_LOG_SHOW_DEBUG
        # RpcCore_LOG_SHOW_VERBOSE
        )

if (RpcCore_BUILD_TEST)
    add_executable(${PROJECT_NAME}_test test/main.cpp test/RpcTest.cpp test/TypeTest.cpp)
    target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME})
    target_compile_definitions(${PROJECT_NAME}_test PRIVATE ${EXAMPLE_COMPILE_DEFINE})
endif ()
