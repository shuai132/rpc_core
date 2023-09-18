#pragma once

#include "flatbuffers/flatbuffers.h"
#include "rpc_core/serialize.hpp"

namespace rpc_core {

template <typename T, typename std::enable_if<std::is_base_of<::flatbuffers::NativeTable, T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  using TableType = typename T::TableType;

  flatbuffers::FlatBufferBuilder fbb(1024);
  auto offset = TableType::Pack(fbb, &t);
  fbb.Finish(offset);
  auto data = fbb.GetBufferPointer();
  auto size = fbb.GetSize();
  oa.data.append((char*)data, size);
  return oa;
}

template <typename T, typename std::enable_if<std::is_base_of<::flatbuffers::NativeTable, T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using TableType = typename T::TableType;

  flatbuffers::Verifier verifier((uint8_t*)ia.data, ia.size);
  bool ok = verifier.VerifyBuffer<TableType>();
  if (!ok) {
    ia.error = true;
    return ia;
  }
  flatbuffers::GetRoot<TableType>(ia.data)->UnPackTo(&t);
  return ia;
}

}  // namespace rpc_core
