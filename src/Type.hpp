#pragma once

#include <type_traits>
#include <cstdint>

#include "Message.hpp"

namespace RpcCore {

#define RpcCore_ENSURE_TYPE_IS_MESSAGE(T) \
        typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0

#if __cplusplus >= 201402L
#define RpcCore_MOVE(arg) arg=std::move(arg)
#else
#define RpcCore_MOVE(arg) arg
#endif

using CmdType = std::string;

using SeqType = uint32_t;

namespace InnerCmd {
const CmdType PING = "-1";
const CmdType PONG = "-2";
}

}
