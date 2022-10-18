# RpcCore

[![Build Status](https://github.com/shuai132/RpcCore/workflows/build/badge.svg)](https://github.com/shuai132/RpcCore/actions?workflow=build)

RPC Core library, designed for IOT, support most microchip(Arduino、STM32、ESP8266)

## Introduction

完善的RPC框架(如gRPC)使用复杂，尤其在嵌入式平台更不现实。
本项目提供轻量级的消息注册、解析分发功能以及方便使用的API。

## Features

* 简单高效易用 支持性能受限的平台
* Header-Only 仅有头文件
* 支持任意形式的连接（串口、TCP等）
* 提供基本数据类型、结构体、字符串、二进制类型的序列化实现
* 方便自定义消息类型 提供了`Flatbuffers`的实现
* 提供Dispose基于RAII自动取消请求 方便UI相关应用
* 支持设置超时重试次数

## Requirements

* C++11
* 数据收发需要完整的数据包，例如WebSocket。  
  如果用Socket/串口等需要自己实现消息打包解包。可使用：[PacketProcessor](https://github.com/shuai132/PacketProcessor)

### Build

1. PC端
   使用CMake:

```bash
mkdir build && cd build && cmake .. && make
```

## Usage

1. 在自己的项目添加搜索路径

```cmake
include_directories(RpcCore的目录)
```

2. 基本用法（省略初始化过程）

```c++
// 接收端
rpc->subscribe("cmd", [](const String& msg) -> String {
    assert(msg == "hello");
    return "world";
});

// 发送端
rpc->cmd("cmd")
    ->msg(String("hello"))
    ->rsp([](const String& rsp) {
      assert(rsp == "world");
    })
    ->call();
```

详细的初始化流程和单元测试见：[RpcTest.cpp](test/RpcTest.cpp)

## Design

### 类说明

* Connection 提供收发实现
* Message 消息自定义序列化/反序列化规则
* MsgWrapper 包装Message 用于内部传输解析
* Coder MsgWrapper序列化实现
* MsgDispatcher 解析MsgWrapper 分发消息
* Request 提供消息请求的各种方法
* Dispose 通过RAII的方式 用于自动取消Request
* Rpc 提供注册消息和创建请求的方法

## Plugin

* [FlatbuffersMsg.hpp](./plugin/FlatbuffersMsg.hpp)  
  支持直接使用`Flatbuffers`生成的对象作为消息传输(`flatc`需添加参数`--gen-object-api`)
