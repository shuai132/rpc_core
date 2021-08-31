#include "RpcCore.hpp"
#include "Test.h"

namespace RpcCoreTest {

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
#else
const CmdType CMD1 = "CMD1";
const CmdType CMD2 = "CMD2";
const CmdType CMD3 = "CMD3";
#endif
}

struct TestStruct {
    uint8_t a;
    uint16_t b;
    uint32_t c;
};

void RpcTest() {
    using namespace RpcCore;
    using String = RpcCore::String;

    // 回环的连接 用于测试 实际使用应为具体传输协议实现的Connection
    auto connection = std::make_shared<LoopbackConnection>();

    // 创建Rpc 收发消息
    auto rpc = Rpc::create(connection);
    rpc->setTimer([](uint32_t ms, const Rpc::TimeoutCb &cb) {
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
        RpcCore_LOGI("测试开始...");
        RpcCore_LOGI("TEST: %s, test: %s", TEST.c_str(), test.c_str());
        assert(TEST == test);

        RpcCore_LOG("1. 收发消息完整测试");
        // 在机器A上注册监听
        rpc->subscribe<String, String>(AppMsg::CMD1, [&](const String& msg) {
            RpcCore_LOGI("get AppMsg::CMD1: %s", msg.c_str());
            assert(msg == TEST);
            return test+"test";
        });
        // 在机器B上发送请求 请求支持很多方法 可根据需求使用所需部分
        auto request = rpc->createRequest()
                ->cmd(AppMsg::CMD1)
                ->msg(test)
                ->rsp<String>([&](const String& rsp){
                    RpcCore_LOGI("get rsp from AppMsg::CMD1: %s", rsp.c_str());
                    assert(rsp == TEST+"test");
                })
                ->timeout([]{
                    RpcCore_LOGI("超时");
                })
                ->finally([](FinishType type) {
                    RpcCore_LOGI("完成: type:%d", (int) type);
                });
        RpcCore_LOGI("执行请求");
        request->call();
        // 其他功能测试
        RpcCore_LOGI("多次调用");
        request->call();
        RpcCore_LOGI("可以取消");
        request->cancel();
        request->call();
        RpcCore_LOGI("恢复取消");
        request->canceled(false);
        request->call();
        RpcCore_LOGI("设置target 配合Dispose");
        Dispose dispose;
        auto target = (void*)1;
        request->target(target);
        dispose.addRequest(request);
        dispose.cancelTarget(target);
        request->call();
        RpcCore_LOGI("先创建Request");
        Request::create()
                ->cmd(AppMsg::CMD1)
                ->msg(test)
                ->call(rpc);
    }

    RpcCore_LOG("2. 值类型双端收发验证");
    {
        const uint64_t VALUE = 0x00001234abcd0000;
        using UInt64_t = Raw<uint64_t>;

        RpcCore_LOGI("TEST_VALUE: 0x%016llx", VALUE);
        rpc->subscribe<UInt64_t, UInt64_t>(AppMsg::CMD2, [&](const UInt64_t& msg) {
            RpcCore_LOGI("get AppMsg::CMD2: 0x%llx", msg.value);
            assert(msg.value == VALUE);
            return VALUE;
        });

        rpc->createRequest()
                ->cmd(AppMsg::CMD2)
                ->msg(UInt64_t(VALUE))
                ->rsp<UInt64_t>([&](const UInt64_t& rsp) {
                    RpcCore_LOGI("get rsp from AppMsg::CMD2: 0x%llx", rsp.value);
                    assert(rsp.value == VALUE);
                })
                ->call();
    }

    RpcCore_LOG("3. 自定义的结构体类型");
    {
        TestStruct testStruct{1, 2, 3};
        using RStruct = RpcCore::Struct<TestStruct>;

        rpc->subscribe<RStruct, RStruct>(AppMsg::CMD3, [&](const RStruct& msg) {
            RpcCore_LOGI("get AppMsg::CMD3");
            assert(msg.value.a == 1);
            assert(msg.value.b == 2);
            assert(msg.value.c == 3);
            return testStruct;
        });
        rpc->createRequest()
                ->cmd(AppMsg::CMD3)
                ->msg(RStruct(testStruct))
                ->rsp<RStruct>([&](const RStruct& rsp) {
                    RpcCore_LOGI("get rsp from AppMsg::CMD3");
                    assert(rsp.value.a == 1);
                    assert(rsp.value.b == 2);
                    assert(rsp.value.c == 3);
                })
                ->call();
    }

    RpcCore_LOG("PING PONG测试");
    {
        rpc->ping("ping")
                ->rsp<String>([&](const String& payload) {
                    RpcCore_LOGI("get rsp from ping: %s", payload.c_str());
                    assert(payload == "ping");
                })
                ->call();
    }
}

}
