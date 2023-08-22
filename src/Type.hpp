#pragma once

#include <cstdint>
#include <string>

namespace RpcCore {

#if __cplusplus >= 201402L
#define RpcCore_MOVE_LAMBDA(arg) arg = std::move(arg)
#define RpcCore_MOVE_PARAM(arg) arg
#else
#define RpcCore_MOVE_LAMBDA(arg) arg
#define RpcCore_MOVE_PARAM(arg) const arg&
#endif

using CmdType = std::string;

using SeqType = uint32_t;

}  // namespace RpcCore
