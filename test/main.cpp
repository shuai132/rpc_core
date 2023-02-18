#include <cstdio>

#include "test/Test.h"

using namespace RpcCoreTest;

int main() {
  printf("TypeTest...\n");
  TypeTest();

#ifdef RpcCore_TEST_PLUGIN
  printf("\nPluginTest...\n");
  PluginTest();
#endif

  printf("\nRpcTest...\n");
  RpcTest();

  return 0;
}
