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
option(RPC_CORE_TEST_CO_ASIO "Build Asio coroutine regression tests" OFF)
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

    add_executable(rpc_core_test_coder test/test_coder.cpp)
    target_include_directories(rpc_core_test_coder PRIVATE include)

    add_executable(rpc_core_test_coder_varint test/test_coder_varint.cpp)
    target_include_directories(rpc_core_test_coder_varint PRIVATE include)
    target_compile_definitions(rpc_core_test_coder_varint PRIVATE RPC_CORE_FEATURE_CODER_VARINT)

    enable_testing()
    if (RPC_CORE_TEST_PLUGIN)
        add_executable(rpc_core_test_flatbuffers_decode test/test_flatbuffers_decode.cpp)
        target_include_directories(rpc_core_test_flatbuffers_decode PRIVATE include thirdparty/flatbuffers/include)
        add_test(NAME rpc_core_test_flatbuffers_decode COMMAND rpc_core_test_flatbuffers_decode)
        foreach(json_test rpc_core_test_json_plugin_errors rpc_core_test_json_backend_errors)
            add_executable(${json_test} test/test_json_errors.cpp)
            target_include_directories(${json_test} PRIVATE include thirdparty)
            add_test(NAME ${json_test} COMMAND ${json_test})
        endforeach()
        target_compile_definitions(rpc_core_test_json_backend_errors PRIVATE RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON)
    endif()
    add_executable(rpc_core_test_packed_fields test/test_packed_fields.cpp)
    target_link_libraries(rpc_core_test_packed_fields PRIVATE rpc_core)
    add_test(NAME rpc_core_test_packed_fields COMMAND rpc_core_test_packed_fields)
    add_executable(rpc_core_test_connection test/test_connection.cpp)
    target_link_libraries(rpc_core_test_connection PRIVATE rpc_core)
    add_test(NAME rpc_core_test_connection COMMAND rpc_core_test_connection)
    add_executable(rpc_core_test_session test/test_session.cpp)
    target_link_libraries(rpc_core_test_session PRIVATE rpc_core)
    add_test(NAME rpc_core_test_session COMMAND rpc_core_test_session)
    add_executable(rpc_core_test_request_lifecycle test/test_request_lifecycle.cpp)
    target_link_libraries(rpc_core_test_request_lifecycle PRIVATE rpc_core)
    add_executable(rpc_core_test_request_reuse test/test_request_reuse.cpp)
    target_link_libraries(rpc_core_test_request_reuse PRIVATE rpc_core)
    add_executable(rpc_core_test_deserialize_reuse test/test_deserialize_reuse.cpp)
    target_link_libraries(rpc_core_test_deserialize_reuse PRIVATE rpc_core)
    foreach(test_target rpc_core_test_deserialize_reuse rpc_core_test rpc_core_test_coder rpc_core_test_coder_varint rpc_core_test_request_lifecycle rpc_core_test_request_reuse)
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
    if (RPC_CORE_TEST_CO_ASIO)
        find_path(RPC_CORE_ASIO_INCLUDE_DIR asio.hpp HINTS ENV ASIO_PATH)
        if (NOT RPC_CORE_ASIO_INCLUDE_DIR)
            message(FATAL_ERROR "Asio headers are required for RPC_CORE_TEST_CO_ASIO")
        endif()
        find_package(Threads REQUIRED)
        add_executable(rpc_core_test_coroutine test/test_coroutine.cpp)
        target_link_libraries(rpc_core_test_coroutine PRIVATE rpc_core Threads::Threads)
        target_include_directories(rpc_core_test_coroutine PRIVATE ${RPC_CORE_ASIO_INCLUDE_DIR})
        target_compile_definitions(rpc_core_test_coroutine PRIVATE RPC_CORE_FEATURE_CO_ASIO)
        target_compile_features(rpc_core_test_coroutine PRIVATE cxx_std_20)
        add_test(NAME rpc_core_test_coroutine COMMAND rpc_core_test_coroutine)
        set_tests_properties(rpc_core_test_coroutine PROPERTIES TIMEOUT 10)
    endif()
endif()
