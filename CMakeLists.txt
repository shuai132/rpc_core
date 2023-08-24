cmake_minimum_required(VERSION 3.1)

project(rpc_core CXX)

option(RPC_CORE_BUILD_TEST "" OFF)
option(RPC_CORE_TEST_PLUGIN "" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(RPC_CORE_BUILD_TEST ON)
endif ()

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE .)

if (RPC_CORE_BUILD_TEST)
    set(EXAMPLE_COMPILE_DEFINE
            ANDROID_STANDALONE
            RPC_CORE_LOG_SHOW_DEBUG
            # RPC_CORE_LOG_SHOW_VERBOSE
            )

    set(TARGET_NAME ${PROJECT_NAME}_test)
    file(GLOB SRCS test/main.cpp test/rpc_test.cpp test/type_test.cpp)
    add_executable(${TARGET_NAME})
    target_link_libraries(${TARGET_NAME} ${PROJECT_NAME})
    target_compile_definitions(${TARGET_NAME} PRIVATE ${EXAMPLE_COMPILE_DEFINE})

    if (RPC_CORE_TEST_PLUGIN)
        list(APPEND SRCS test/plugin_test.cpp)
        target_compile_definitions(${TARGET_NAME} PRIVATE "RPC_CORE_TEST_PLUGIN")
        add_custom_target(${TARGET_NAME}_init
                # clear dir
                COMMAND rm -rf ${CMAKE_CURRENT_LIST_DIR}/thirdparty
                # json
                COMMAND mkdir -p ${CMAKE_CURRENT_LIST_DIR}/thirdparty/nlohmann
                COMMAND curl -L -o ${CMAKE_CURRENT_LIST_DIR}/thirdparty/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
                # flatbuffers
                COMMAND git clone https://github.com/google/flatbuffers.git --depth=1 --branch=v23.1.21 ${CMAKE_CURRENT_LIST_DIR}/thirdparty/flatbuffers
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                )

        target_include_directories(${TARGET_NAME} PRIVATE thirdparty)
        target_include_directories(${TARGET_NAME} PRIVATE thirdparty/flatbuffers/include)
    endif ()
    target_sources(${TARGET_NAME} PRIVATE ${SRCS})
endif ()
