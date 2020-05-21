cmake_minimum_required(VERSION 3.1)

project(RpcCore CXX)

# common
set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

include_directories(modules/LOG)
include_directories(modules/MAKE_EVENT)
include_directories(modules/ArduinoJson/src)
add_executable(${PROJECT_NAME}_example example.cpp)
