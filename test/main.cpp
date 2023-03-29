#include <cstdio>

#include "src/detail/log.h"
#include "test/Test.h"

using namespace RpcCoreTest;

int main() {
  RpcCore_LOG("TypeTest...");
  TypeTest();

#ifdef RpcCore_TEST_PLUGIN
  printf("\n");
  RpcCore_LOG("PluginTest...");
  PluginTest();
#endif

  printf("\n");
  RpcCore_LOG("RpcTest...");
  RpcTest();

  return 0;
}
