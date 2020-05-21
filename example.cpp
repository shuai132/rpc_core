#include "RpcCore.hpp"
#include "log.h"

using namespace RpcCore;

namespace AppMsg {
    const CmdType CMD1 = 1;
    const CmdType CMD2 = 2;
    const CmdType CMD3 = 3;
    const CmdType CMD4 = 4;
    const CmdType CMD5 = 5;
}

/**
 * 消息收发示例
 *
 * 自定义的几个命令 测试不同收发方式的使用方法
 * 发送 =======CmdMessageType=====> 接收
 * 响应 <======RspMessageType====== 回复
 */
int main() {
    // 回环的连接 用于测试 实际使用应为具体传输协议实现的Connection
    auto connection = std::make_shared<LoopbackConnection>();

    // 创建Rpc 收发消息
    Rpc rpc(connection);
    rpc.setTimerFunc([](uint32_t ms, const MsgDispatcher::TimeoutCb& cb){
        // 定时器实现 应当配合当前应用的事件循环 确保消息收发和超时在同一个线程
        // 此示例为回环的连接 不需要具体实现
    });

    // 测试所用payload
    const std::string TEST_PAYLOAD("Hello World");

    // 注册和发送消息 根据使用场景不同 提供以下几种方式
    // 此处收发类型均为StringValue 实际场景可为其他自定义的protobuf的Message类型
    {
        LOGI("start test...");
        // 待测试消息
        StringValue test_message;
        test_message.value = TEST_PAYLOAD;

        LOG("1. sender发送命令 subscriber无返回消息");
        {
            rpc.subscribe(AppMsg::CMD1, [] {
                LOGI("get AppMsg::CMD1");
            });
            rpc.send(AppMsg::CMD1, [] {
                LOGI("get rsp from AppMsg::CMD1");
            });
        }

        LOG("2. sender发送数据 subscriber无返回消息");
        {
            rpc.subscribe<StringValue>(AppMsg::CMD2, [&](const StringValue& msg) {
                LOGI("get AppMsg::CMD2: %s", msg.value.c_str());
                assert(msg.value == TEST_PAYLOAD);
            });
            rpc.send(AppMsg::CMD2, test_message, []() {
                LOGI("get rsp from AppMsg::CMD2");
            });
        }

        LOG("3. sender不发送数据 subscriber返回消息");
        {
            rpc.subscribe<StringValue>(AppMsg::CMD3, [&]() {
                LOGI("get AppMsg::CMD3:");
                return test_message;
            });
            rpc.send<StringValue>(AppMsg::CMD3, [&](const StringValue& rsp) {
                LOGI("get rsp from AppMsg::CMD3: %s", rsp.value.c_str());
                assert(rsp.value == TEST_PAYLOAD);
            });
        }

        LOG("4. 双端收发消息");
        {
            rpc.subscribe<StringValue, StringValue>(AppMsg::CMD4, [&](const StringValue& msg) {
                LOGI("get AppMsg::CMD4: %s", msg.value.c_str());
                assert(msg.value == TEST_PAYLOAD);
                return test_message;
            });
            rpc.send<StringValue>(AppMsg::CMD4, test_message, [&](const StringValue& rsp) {
                LOGI("get rsp from AppMsg::CMD4: %s", rsp.value.c_str());
                assert(rsp.value == TEST_PAYLOAD);
            });
        }

        LOG("5. 值类型双端收发验证");
        {
            const uint64_t TEST_VALUE = 0x1234567;
            using MyInt = Value<uint64_t>;

            LOGI("TEST_VALUE: 0x%llx", TEST_VALUE);
            rpc.subscribe<MyInt, MyInt>(AppMsg::CMD5, [&](const MyInt& msg) {
                LOGI("get AppMsg::CMD5: 0x%llx", msg.value);
                assert(msg.value == TEST_VALUE);
                return MyInt(TEST_VALUE);
            });
            rpc.send<MyInt>(AppMsg::CMD5, MyInt(TEST_VALUE), [&](const MyInt& rsp) {
                LOGI("get rsp from AppMsg::CMD5: 0x%llx", rsp.value);
                assert(rsp.value == TEST_VALUE);
            });
        }
    }

    LOG("PING PONG测试");
    {
        rpc.sendPing(TEST_PAYLOAD, [&](const StringValue& payload) {
            LOGI("get rsp from ping: %s", payload.value.c_str());
            assert(payload.value == TEST_PAYLOAD);
        });
    }

    return 0;
}
