#include <cstdio>

#include "rpc_core/detail/log.h"
#include "test.h"

using namespace rpc_core_test;

int main() {
  RPC_CORE_LOG("test_type...");
  test_type();

  printf("\n");
  RPC_CORE_LOG("test_data_packer...");
  test_data_packer();

#ifdef RPC_CORE_TEST_PLUGIN
  printf("\n");
  RPC_CORE_LOG("test_plugin...");
  test_plugin();
#endif

  printf("\n");
  RPC_CORE_LOG("test_rpc...");
  test_rpc();

  return 0;
}
