#include <cstdio>

#include "src/detail/log.h"
#include "test/test.h"

using namespace rpc_core_test;

int main() {
  RPC_CORE_LOG("TypeTest...");
  TypeTest();

#ifdef RPC_CORE_TEST_PLUGIN
  printf("\n");
  RPC_CORE_LOG("PluginTest...");
  PluginTest();
#endif

  printf("\n");
  RPC_CORE_LOG("RpcTest...");
  RpcTest();

  return 0;
}
