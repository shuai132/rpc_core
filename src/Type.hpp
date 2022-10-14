#pragma once

#include <cstdint>
#include <type_traits>

#include "Message.hpp"

namespace RpcCore {

#define RpcCore_ENSURE_TYPE_IS_MESSAGE(T) typename std::enable_if<std::is_base_of<Message, T>::value, int>::type = 0

#if __cplusplus >= 201402L
#define RpcCore_MOVE_LAMBDA(arg) arg = std::move(arg)
#define RpcCore_MOVE_PARAM(arg) arg
#else
#define RpcCore_MOVE_LAMBDA(arg) arg
#define RpcCore_MOVE_PARAM(arg) const arg&
#endif

using CmdType = std::string;

using SeqType = uint32_t;

namespace InnerCmd {
const CmdType PING = "-1";
const CmdType PONG = "-2";
}  // namespace InnerCmd

}  // namespace RpcCore
