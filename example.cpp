#include "test/TypeTest.hpp"

//#define RpcCore_USE_INT_CMD_TYPE
#include "test/RpcTest.hpp"

using namespace RpcCoreTest;

int main() {
    LOG("TypeTest...");
    TypeTest();

    LOG("RpcTest...");
    RpcTest();

    return 0;
}
