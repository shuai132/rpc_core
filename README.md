# RpcCore

[![Build Status](https://github.com/shuai132/RpcCore/workflows/build/badge.svg)](https://github.com/shuai132/RpcCore/actions?workflow=build)

RPC Core library, designed for IOT, support most microchip(Arduino、STM32、ESP8266)

## Introduction
完善的RPC框架(如gRPC)使用复杂，尤其在嵌入式平台更不现实。
本项目提供轻量级的消息注册、解析分发功能以及方便使用的API。

## Features
* 简单高效 接口易用
* 支持性能受限的平台
* Header-Only 仅有头文件 方便引用
* 自定义连接 支持任何形式（串口、TCP等）
* 支持自定义序列化反序列化方式 默认实现使用json
* 提供基本数据类型、结构体、二进制类型的序列化实现
* 提供Finish回调 获取、超时、取消都会调用
* 支持取消请求 单个请求或指定target
* 提供Dispose可利用RAII自动取消一组请求 方便UI相关应用

## Requirements
* C++11编译器 （不需要特别完整 大部分支持C++11的MCU编译器均可）
* 数据收发需要完整的数据包，例如WebSocket。如果用Socket/串口等需要自己实现消息打包解包
比如`msgpack`，也可使用这个项目更简单：[https://github.com/shuai132/PacketProcessor.](https://github.com/shuai132/PacketProcessor)

## TestCase
详见[RpcTest.cpp](test/RpcTest.cpp)

### Clone
因包含submodule，克隆仓库请添加`--recursive`参数，或clone后执行：
```bash
git submodule update --init --recursive
```

### Build
1. PC端
使用CMake:
```bash
mkdir build && cd build && cmake .. && make
```

## Usage
1. clone完整仓库
2. 在自己的项目添加搜索路径
* 方式一：cmake
```cmake
add_subdirectory(RpcCore的目录)
target_link_libraries(YOUR_TARGET RpcCore)
```
* 方式二：直接添加路径 cmake为例
```cmake
include_directories(RpcCore)
include_directories(RpcCore/modules/LOG)
include_directories(RpcCore/modules/MAKE_EVENT)
include_directories(RpcCore/modules/ArduinoJson/src)
```
3. [具体用法参考TestCase](#TestCase)
