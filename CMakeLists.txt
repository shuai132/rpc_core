cmake_minimum_required(VERSION 3.15)

project(rpc_core CXX)

# config
option(RPC_CORE_SERIALIZE_USE_CUSTOM "" "")
option(RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON "" OFF)
option(RPC_CORE_FEATURE_FUTURE "" OFF)
option(RPC_CORE_FEATURE_CO_ASIO "" OFF)
option(RPC_CORE_FEATURE_CODER_VARINT "" OFF)

# test
option(RPC_CORE_BUILD_TEST "" OFF)
option(RPC_CORE_TEST_PLUGIN "" OFF)
option(RPC_CORE_TEST_ASIO "" OFF)
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

if (RPC_CORE_FEATURE_CO_ASIO)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRPC_CORE_FEATURE_CO_ASIO)
endif ()

if (RPC_CORE_FEATURE_CODER_VARINT)
    target_compile_definitions(${PROJECT_NAME} INTERFACE -DRPC_CORE_FEATURE_CODER_VARINT)
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

    if (RPC_CORE_TEST_ASIO)
        find_path(RPC_CORE_ASIO_INCLUDE_DIR asio.hpp
                PATHS ENV ASIO_PATH
                PATH_SUFFIXES include
        )
        if (NOT RPC_CORE_ASIO_INCLUDE_DIR)
            message(FATAL_ERROR "RPC_CORE_TEST_ASIO requires standalone Asio. Set ASIO_PATH to the directory containing asio.hpp.")
        endif ()

        function(add_asio_tcp_test target_suffix source)
            set(ASIO_TARGET_NAME ${PROJECT_NAME}_test_asio_tcp_${target_suffix})
            add_executable(${ASIO_TARGET_NAME} ${source})
            target_link_libraries(${ASIO_TARGET_NAME} ${PROJECT_NAME} ${LIBRARIES})
            if (NOT MSVC)
                target_link_libraries(${ASIO_TARGET_NAME} pthread)
            endif ()
            target_include_directories(${ASIO_TARGET_NAME} PRIVATE ${RPC_CORE_ASIO_INCLUDE_DIR})
            target_compile_definitions(${ASIO_TARGET_NAME} PRIVATE ${EXAMPLE_COMPILE_DEFINE} ASIO_NO_DEPRECATED)
            if (WIN32)
                target_compile_definitions(${ASIO_TARGET_NAME} PRIVATE _WIN32_WINNT=0x0601 NOMINMAX)
            endif ()
        endfunction()

        add_asio_tcp_test(rpc test/test_asio_tcp_rpc.cpp)
        add_asio_tcp_test(rpc_s test/test_asio_tcp_rpc_s.cpp)
        add_asio_tcp_test(rpc_c test/test_asio_tcp_rpc_c.cpp)
    endif ()
endif ()
