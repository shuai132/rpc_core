#pragma once

#include "RpcCore.hpp"

namespace RpcCoreTest {

struct MyData {
    uint8_t a;
    uint16_t b;
};

inline void TypeTest() {
    using namespace RpcCore;
    {
        LOGD("Raw<uint64_t>...");
        const uint64_t value = 0x12345678abcd;
        Raw<uint64_t> a(value);
        assert(a == a.value);
        Raw<uint64_t> b;
        b.deSerialize(a.serialize());
        assert(b == a);
    }
    {
        LOGD("Struct...");
        MyData data{1, 2};
        Struct<MyData> a;
        assert(a.align_size == alignof(MyData));
        a.value = data;
        Struct<MyData> b;
        b.deSerialize(a.serialize());
        assert(b.value.a == 1);
        assert(b.value.b == 2);
        assert(0 == memcmp(&a.value, &b.value, sizeof(MyData)));
    }
    {
        LOGD("String/Bianry...");
        uint8_t data[] = {0, 2, 4, 255, 0/*important*/, 1, 3, 5};
        Bianry a((char*)data, sizeof(data));
        Bianry b;
        b.deSerialize(a.serialize());
        assert(b.size() == sizeof(data));
        assert(b == a);
    }
}

}
