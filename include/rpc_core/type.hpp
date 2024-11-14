#pragma once

#include <cstdint>
#include <string>

// config
#include "config.hpp"

namespace rpc_core {

#define RPC_CORE_UNUSED(x) (void)x

using cmd_type = std::string;

using seq_type = uint32_t;

}  // namespace rpc_core
