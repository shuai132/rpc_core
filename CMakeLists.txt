cmake_minimum_required(VERSION 3.5)

project(rpc_core CXX)

# config
option(RPC_CORE_SERIALIZE_USE_CUSTOM "" "")
option(RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON "" OFF)
option(RPC_CORE_FEATURE_FUTURE "" OFF)
option(RPC_CORE_FEATURE_ASIO_COROUTINE "" OFF)

# test
option(RPC_CORE_BUILD_TEST "" OFF)
option(RPC_CORE_TEST_PLUGIN "" OFF)
option(RPC_CORE_TEST_LINK_PTHREAD "" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(RPC_CORE_BUILD_TEST ON)
    set(RPC_CORE_FEATURE_FUTURE ON)
endif ()

set(CMAKE_CXX_STANDARD 14)
if (MSVC)
    add_compile_options(/Zc:preprocessor)
    add_compile_options(/utf-8)
    add_compile_options(-DNOMINMAX)
else ()
    add_compile_options(-Wall -Wunused-parameter)
endif ()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

if (RPC_CORE_SERIALIZE_USE_CUSTOM)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRPC_CORE_SERIALIZE_USE_CUSTOM="${RPC_CORE_SERIALIZE_USE_CUSTOM}")
endif ()

if (RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRPC_CORE_SERIALIZE_USE_NLOHMANN_JSON)
endif ()

if (RPC_CORE_FEATURE_FUTURE)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRPC_CORE_FEATURE_FUTURE)
endif ()

if (RPC_CORE_FEATURE_ASIO_COROUTINE)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRPC_CORE_FEATURE_ASIO_COROUTINE)
endif ()

if (RPC_CORE_BUILD_TEST)
    set(EXAMPLE_COMPILE_DEFINE
            ANDROID_STANDALONE
            RPC_CORE_LOG_SHOW_DEBUG
            # RPC_CORE_LOG_SHOW_VERBOSE
    )

    set(TARGET_NAME ${PROJECT_NAME}_test)
    file(GLOB SRCS test/main.cpp test/test_rpc.cpp test/test_serialize.cpp test/test_data_packer.cpp)
    add_executable(${TARGET_NAME})
    if (RPC_CORE_TEST_LINK_PTHREAD)
        # some linux platform need link -pthread for std::future api
        set(LIBRARIES -pthread)
    endif ()
    target_link_libraries(${TARGET_NAME} ${PROJECT_NAME} ${LIBRARIES})
    target_compile_definitions(${TARGET_NAME} PRIVATE ${EXAMPLE_COMPILE_DEFINE})

    if (RPC_CORE_TEST_PLUGIN)
        list(APPEND SRCS test/test_plugin.cpp)
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
