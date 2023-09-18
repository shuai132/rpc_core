#pragma once

#include <cstdint>
#include <string>

// config
#include "config.hpp"

namespace rpc_core {

#if __cplusplus >= 201402L
#define RPC_CORE_MOVE_LAMBDA(arg) arg = std::move(arg)
#define RPC_CORE_MOVE_PARAM(arg) arg
#else
#define RPC_CORE_MOVE_LAMBDA(arg) arg
#define RPC_CORE_MOVE_PARAM(arg) const arg&
#endif

using cmd_type = std::string;

using seq_type = uint32_t;

}  // namespace rpc_core
