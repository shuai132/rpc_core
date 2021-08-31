#include "test/Test.h"
#include "src/log.h"

using namespace RpcCoreTest;

int main() {
    RpcCore_LOG("TypeTest...");
    TypeTest();

    RpcCore_LOG("RpcTest...");
    RpcTest();

    return 0;
}
