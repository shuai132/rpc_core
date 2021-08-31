cmake_minimum_required(VERSION 3.1)

project(RpcCore CXX)

# common
set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE .)

set(EXAMPLE_COMPILE_DEFINE
        RpcCore_LOG_WITH_COLOR
        RpcCore_LOG_SHOW_DEBUG
        # RpcCore_LOG_SHOW_VERBOSE
        )

add_executable(${PROJECT_NAME}_example example.cpp test/RpcTest.cpp test/TypeTest.cpp)
target_link_libraries(${PROJECT_NAME}_example ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}_example PRIVATE ${EXAMPLE_COMPILE_DEFINE})
