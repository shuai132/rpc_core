//#define RpcCore_USE_INT_CMD_TYPE
#include "test/Test.h"
#include "log.h"

using namespace RpcCoreTest;

int main() {
    LOG("TypeTest...");
    TypeTest();

    LOG("RpcTest...");
    RpcTest();

    return 0;
}
