#pragma once

#include "flatbuffers/flatbuffers.h"
#include "src/serialize.hpp"

namespace RPC_CORE_NAMESPACE {

template <typename T, typename std::enable_if<std::is_base_of<::flatbuffers::NativeTable, T>::value, int>::type = 0>
serialize_oarchive& operator<<(serialize_oarchive& oa, const T& t) {
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
serialize_iarchive& operator>>(serialize_iarchive& ia, T& t) {
  using TableType = typename T::TableType;

  flatbuffers::Verifier verifier((uint8_t*)ia.data_, ia.size_);
  bool ok = verifier.VerifyBuffer<TableType>();
  if (!ok) {
    ia.error_ = true;
  }
  flatbuffers::GetRoot<TableType>(ia.data_)->UnPackTo(&t);
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
