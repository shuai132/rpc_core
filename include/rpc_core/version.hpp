#pragma once

#define RPC_CORE_VER_MAJOR 2
#define RPC_CORE_VER_MINOR 1
#define RPC_CORE_VER_PATCH 0

#define RPC_CORE_TO_VERSION(major, minor, patch) (major * 10000 + minor * 100 + patch)
#define RPC_CORE_VERSION RPC_CORE_TO_VERSION(RPC_CORE_VER_MAJOR, RPC_CORE_VER_MINOR, RPC_CORE_VER_PATCH)
