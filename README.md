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
* 支持设置超时重试次数

## Requirements

* C++11编译器 （不需要特别完整 大部分支持C++11的MCU编译器均可）
* 数据收发需要完整的数据包，例如WebSocket。如果用Socket/串口等需要自己实现消息打包解包
  比如`msgpack`，也可使用这个项目更简单：[https://github.com/shuai132/PacketProcessor](https://github.com/shuai132/PacketProcessor)

## TestCase

详见[RpcTest.cpp](test/RpcTest.cpp)

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
```

3. [具体用法参考TestCase](#TestCase)

## Design

### 类说明

* Connection 提供收发实现 不存在连接断开状态 但可以控制它是否收发
* Message 用户消息接口 自定义序列化/反序列化规则
* MsgWrapper 包装Message 用于封装一致的消息格式和传输
* MsgDispatcher 解析MsgWrapper 分发消息
* Coder MsgWrapper序列化实现
* Request 消息请求 提供设置Message、接收/超时回调、取消等方法
* Dispose 通过RAII的方式 用于自动取消Request
* Rpc 对外接口 提供注册消息和创建请求的方法

### LifeCycle

* Connection是全局的
* Request不持有Rpc 发送到完成之间Rpc将持有Request(为了接收回复)
* Dispose持有Request的弱引用

## Plugin

* [FlatbuffersMsg.hpp](./plugin/FlatbuffersMsg.hpp)  
  支持直接使用Flatbuffers生成的对象作为消息传输(`flatc`需添加参数`--gen-object-api`)
