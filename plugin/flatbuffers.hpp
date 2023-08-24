#pragma once

#include "flatbuffers/flatbuffers.h"
#include "src/serialize.hpp"

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_base_of<::flatbuffers::NativeTable, T>::value, int>::type = 0>
std::string serialize(const T& t) {
  using TableType = typename T::TableType;

  flatbuffers::FlatBufferBuilder fbb(1024);
  auto offset = TableType::Pack(fbb, &t);
  fbb.Finish(offset);
  auto data = fbb.GetBufferPointer();
  auto size = fbb.GetSize();
  return {(char*)data, size};
}

template <typename T, typename std::enable_if<std::is_base_of<::flatbuffers::NativeTable, T>::value, int>::type = 0>
bool deserialize(const detail::string_view& data, T& t) {
  using TableType = typename T::TableType;

  flatbuffers::Verifier verifier((uint8_t*)data.data(), data.size());
  bool ok = verifier.VerifyBuffer<TableType>();
  if (!ok) {
    return false;
  }
  flatbuffers::GetRoot<TableType>(data.data())->UnPackTo(&t);
  return true;
}

}  // namespace RPC_CORE_NAMESPACE
