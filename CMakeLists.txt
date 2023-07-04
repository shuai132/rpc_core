cmake_minimum_required(VERSION 3.1)

project(RpcCore CXX)

option(RpcCore_BUILD_TEST "" OFF)
option(RpcCore_TEST_PLUGIN "" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(RpcCore_BUILD_TEST ON)
endif ()

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE .)

if (RpcCore_BUILD_TEST)
    set(EXAMPLE_COMPILE_DEFINE
            ANDROID_STANDALONE
            RpcCore_LOG_SHOW_DEBUG
            # RpcCore_LOG_SHOW_VERBOSE
            )

    set(TARGET_NAME ${PROJECT_NAME}_test)
    file(GLOB SRCS test/*.cpp)
    add_executable(${TARGET_NAME} ${SRCS})
    target_link_libraries(${TARGET_NAME} ${PROJECT_NAME})
    target_compile_definitions(${TARGET_NAME} PRIVATE ${EXAMPLE_COMPILE_DEFINE})

    if (RpcCore_TEST_PLUGIN)
        target_compile_definitions(${TARGET_NAME} PRIVATE "RpcCore_TEST_PLUGIN")
        add_custom_target(${TARGET_NAME}_init
                # clear dir
                COMMAND rm -rf ${CMAKE_CURRENT_LIST_DIR}/thirdparty
                # json
                COMMAND mkdir -p ${CMAKE_CURRENT_LIST_DIR}/thirdparty/nlohmann
                COMMAND wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -P ${CMAKE_CURRENT_LIST_DIR}/thirdparty/nlohmann
                # flatbuffers
                COMMAND git clone https://github.com/google/flatbuffers.git --depth=1 --branch=v23.1.21 ${CMAKE_CURRENT_LIST_DIR}/thirdparty/flatbuffers
                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                )

        target_include_directories(${TARGET_NAME} PRIVATE thirdparty)
        target_include_directories(${TARGET_NAME} PRIVATE thirdparty/flatbuffers/include)
    endif ()
endif ()
