#pragma once

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

#pragma pack(push, 1)
struct CustomType2 {
  uint8_t id1{};
  uint8_t id2{};
  uint32_t id3{};
};
#pragma pack(pop)
RpcCore_DEFINE_TYPE(CustomType2, id1, id2, id3);

#pragma pack(push, 4)
struct CustomType3 {
  uint8_t id1{};
  uint8_t id2{};
  uint32_t id3{};
};
#pragma pack(pop)
RpcCore_DEFINE_TYPE(CustomType3, id1, id2, id3);

// 嵌套定义
struct CustomTypeNest {
  CustomType2 c2{};
  CustomType3 c3{};
  CustomTypeNest* ptr{};
};
RpcCore_DEFINE_TYPE(CustomTypeNest, c2, c3, ptr);
