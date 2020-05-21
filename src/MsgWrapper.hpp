#pragma once

#include <string>

#include "base/copyable.hpp"
#include "Type.hpp"
#include "log.h"

namespace RpcCore {

/**
 * 包装数据 用于协议分析
 */
struct MsgWrapper : copyable {
    enum MsgType : uint8_t {
        COMMAND = 0,
        RESPONSE = 1,
    };

    SeqType seq;
    CmdType cmd;
    MsgType type;
    std::string data;

    std::string dump() const {
        char tmp[100];
        snprintf(tmp, 100, "seq:%u, type:%u, cmd:%s", seq, type, CmdToStr(cmd).c_str());
        return tmp;
    }

    /**
     * 将data解析为指定类型的数据
     * T为Message类型
     */
    template<typename T, RpcCore_ENSURE_TYPE_IS_MESSAGE(T)>
    T unpackAs() const {
        T message;
        bool ok = message.deSerialize(data);
        if (not ok) {
            LOGE("serialize error, msg info:%s", dump().c_str());
        }
        return message;
    }

    /**
     * 创建Cmd消息
     * @param cmd
     * @param data
     * @return
     */
    static MsgWrapper MakeCmd(CmdType cmd, SeqType seq, const Message& data = VOID) {
        MsgWrapper msg;
        msg.type = MsgWrapper::COMMAND;
        msg.cmd = cmd;
        msg.seq = seq;
        if (&data != &VOID) {
            msg.data = data.serialize();
        }
        return msg;
    }

    /**
     * 创建Rsp消息
     * @param seq 对应Cmd的seq
     * @param message 将保存在Msg.data
     * @param success 成功/失败
     * @return
     */
    static MsgWrapper MakeRsp(SeqType seq, const Message& message = VOID) {
        MsgWrapper msg;
        msg.type = MsgWrapper::RESPONSE;
        msg.seq = seq;
        if ((intptr_t*) &message != (intptr_t*) &VOID) {
            msg.data = message.serialize();
        }
        return msg;
    }
};

}
