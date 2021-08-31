#pragma once

#include <type_traits>
#include <cstdint>

#include "Message.hpp"

namespace RpcCore {

#define RpcCore_ENSURE_TYPE_IS_MESSAGE(T) \
        typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0

#if _LIBCPP_STD_VER >= 14
#define RpcCore_MOVE(arg) arg=std::move(arg)
#else
#define RpcCore_MOVE(arg) arg
#endif

// 默认使用string作为cmd类型
#ifndef RpcCore_USE_INT_CMD_TYPE
#define RpcCore_USE_STR_CMD_TYPE
#endif

#ifdef RpcCore_USE_INT_CMD_TYPE
using CmdType = uint16_t;
#else
using CmdType = std::string;
#endif

using SeqType = uint32_t;

namespace InnerCmd {
#ifdef RpcCore_USE_INT_CMD_TYPE
const CmdType PING = UINT16_MAX;
const CmdType PONG = UINT16_MAX - 1;
#else
const CmdType PING = "-1";
const CmdType PONG = "-2";
#endif
}

inline std::string CmdToStr(CmdType cmd) {
#ifdef RpcCore_USE_INT_CMD_TYPE
    return std::to_string(cmd);
#else
    return cmd;
#endif
}

}
