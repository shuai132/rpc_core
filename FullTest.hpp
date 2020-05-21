#include "RpcCore.hpp"
#include "log.h"

/**
 * 用户命令类型定义
 * 支持两种形式:
 * 1. int32_t 最佳性能 系统占用<0的范围
 * 2. std::string 便于使用 系统占用"RpcCore/"开头的字符串
 */
namespace AppMsg {
using namespace RpcCore;

#ifdef RpcCore_USE_INT_CMD_TYPE
    const CmdType CMD1 = 1;
    const CmdType CMD2 = 2;
    const CmdType CMD3 = 3;
    const CmdType CMD4 = 4;
    const CmdType CMD5 = 5;
#else
    const CmdType CMD1 = "CMD1";
    const CmdType CMD2 = "CMD2";
    const CmdType CMD3 = "CMD3";
    const CmdType CMD4 = "CMD4";
    const CmdType CMD5 = "CMD5";
#endif
}

/**
 * 消息收发示例
 *
 * 自定义的几个命令 测试不同收发方式的使用方法
 * 发送 =======CmdMessageType=====> 接收
 * 响应 <======RspMessageType====== 回复
 */
inline void FullTest() {
    using namespace RpcCore;
    using String = RpcCore::String;

    // 回环的连接 用于测试 实际使用应为具体传输协议实现的Connection
    auto connection = std::make_shared<LoopbackConnection>();

    // 创建Rpc 收发消息
    Rpc rpc(connection);
    rpc.setTimerFunc([](uint32_t ms, const MsgDispatcher::TimeoutCb& cb){
        // 定时器实现 应当配合当前应用的事件循环 确保消息收发和超时在同一个线程
        // 此示例为回环的连接 不需要具体实现
    });

    /**
     * 注册和发送消息 根据使用场景不同 提供以下几种方式
     * 注:
     * 1. 收发类型为Message即可，可自定义序列化/反序列化。
     * 2. 内部提供了常用的Message子类型。包括二进制数据 也可使用内部题二进制值类型。
     * 3. send可附加超时回调和超时时间
     */
    {
        const std::string TEST("Hello World");
        String test = TEST;
        LOGI("测试开始...");
        LOGI("TEST: %s, test: %s", TEST.c_str(), test.c_str());
        assert(TEST == test);

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
            rpc.subscribe<String>(AppMsg::CMD2, [&](const String& msg) {
                LOGI("get AppMsg::CMD2: %s", msg.c_str());
                assert(msg == TEST);
            });
            rpc.send(AppMsg::CMD2, test, []() {
                LOGI("get rsp from AppMsg::CMD2");
            });
        }

        LOG("3. sender不发送数据 subscriber返回消息");
        {
            rpc.subscribe<String>(AppMsg::CMD3, [&]() {
                LOGI("get AppMsg::CMD3:");
                return test;
            });
            rpc.send<String>(AppMsg::CMD3, [&](const String& rsp) {
                LOGI("get rsp from AppMsg::CMD3: %s", rsp.c_str());
                assert(rsp == TEST);
            });
        }

        LOG("4. 双端收发消息");
        {
            rpc.subscribe<String, String>(AppMsg::CMD4, [&](const String& msg) {
                LOGI("get AppMsg::CMD4: %s", msg.c_str());
                assert(msg == TEST);
                return test;
            });
            rpc.send<String>(AppMsg::CMD4, test, [&](const String& rsp) {
                LOGI("get rsp from AppMsg::CMD4: %s", rsp.c_str());
                assert(rsp == TEST);
            });
        }

        LOG("5. 值类型双端收发验证");
        {
            const uint64_t VALUE = 0x1234567812345678;
            using UInt64_t = Value<uint64_t>;

            LOGI("TEST_VALUE: 0x%llx", VALUE);
            rpc.subscribe<UInt64_t, UInt64_t>(AppMsg::CMD5, [&](const UInt64_t& msg) {
                LOGI("get AppMsg::CMD5: 0x%llx", msg.value);
                assert(msg.value == VALUE);
                return VALUE;
            });
            rpc.send<UInt64_t>(AppMsg::CMD5, UInt64_t(VALUE), [&](const UInt64_t& rsp) {
                LOGI("get rsp from AppMsg::CMD5: 0x%llx", rsp.value);
                assert(rsp.value == VALUE);
            });
        }
    }

    LOG("PING PONG测试");
    {
        rpc.sendPing("ping", [&](const String& payload) {
            LOGI("get rsp from ping: %s", payload.c_str());
            assert(payload == "ping");
        });
    }
}
