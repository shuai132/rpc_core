#include "src/Serialize.hpp"

struct CustomType {
  uint32_t id = 0;
  std::vector<uint32_t> ids;
  std::string name;
  bool operator==(const CustomType& t) const {
    return std::tie(id, ids, name) == std::tie(t.id, t.ids, t.name);
  }
};
RpcCore_DEFINE_TYPE(CustomType, id, ids, name);

struct alignas(4) CustomType2 {
  uint8_t id1;
  uint8_t id2;
  uint32_t id3;
};
// RpcCore_DEFINE_TYPE(CustomType2, id1, id2);

struct alignas(32) CustomType3 {
  uint8_t id1;
  uint8_t id2;
  uint32_t id3;
};
// RpcCore_DEFINE_TYPE(CustomType3, id1, id2);
