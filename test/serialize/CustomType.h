#pragma once

#include "rpc_core/serialize.hpp"

struct CustomType {
  uint32_t id = 0;
  std::vector<uint32_t> ids;
  std::string name;
  bool operator==(const CustomType& t) const {
    return std::tie(id, ids, name) == std::tie(t.id, t.ids, t.name);
  }
  bool operator<(const CustomType& t) const {
    return std::tie(id, ids, name) < std::tie(t.id, t.ids, t.name);
  }
};
RPC_CORE_DEFINE_TYPE(CustomType, id, ids, name);

// 指针
struct CustomTypePtr {
  int32_t* int_n;
  int32_t* int_v;
  std::unique_ptr<int32_t> unique_ptr_n;
  std::unique_ptr<int32_t> unique_ptr_v;
  std::shared_ptr<int32_t> shared_ptr_n;
  std::shared_ptr<int32_t> shared_ptr_v;
};
RPC_CORE_DEFINE_TYPE(CustomTypePtr, int_n, int_v, unique_ptr_n, unique_ptr_v, shared_ptr_n, shared_ptr_v);

#pragma pack(push, 1)
struct CustomType2 {
  uint8_t id1{};
  uint8_t id2{};
  uint32_t id3{};
};
#pragma pack(pop)
RPC_CORE_DEFINE_TYPE(CustomType2, id1, id2, id3);

#pragma pack(push, 4)
struct CustomType3 {
  uint8_t id1{};
  uint8_t id2{};
  uint32_t id3{};
};
#pragma pack(pop)
RPC_CORE_DEFINE_TYPE(CustomType3, id1, id2, id3);

namespace test {
// 嵌套定义
struct CustomTypeNest {
  CustomType2 c2{};
  CustomType3 c3{};
  CustomTypeNest* ptr{};
};
RPC_CORE_DEFINE_TYPE(test::CustomTypeNest, c2, c3, ptr);

// 内部定义
struct CustomTypeNest2 {
  // can be private
  CustomType2 c2{};
  CustomType3 c3{};
  CustomTypeNest* ptr{};
  RPC_CORE_DEFINE_TYPE_INNER(c2, c3, ptr);
};

}  // namespace test
