#pragma once

#include "RpcCore.hpp"
#include "flatbuffers/flatbuffers.h"

namespace RpcCore {

template <typename T, size_t InitialSize = 1024, bool EnableVerify = true>
struct FlatbuffersMsg : RpcCore::Message {
  using TableType = typename T::TableType;

  std::string serialize() const override {
    flatbuffers::FlatBufferBuilder fbb(InitialSize);
    auto offset = TableType::Pack(fbb, &msg);
    fbb.Finish(offset);
    auto data = fbb.GetBufferPointer();
    auto size = fbb.GetSize();
    return {(char*)data, size};
  }

  bool deserialize(const detail::string_view& data) override {
    if (EnableVerify) {
      flatbuffers::Verifier verifier((uint8_t*)data.data(), data.size());
      bool ok = verifier.VerifyBuffer<TableType>();
      if (!ok) {
        return false;
      }
    }
    flatbuffers::GetRoot<TableType>(data.data())->UnPackTo(&msg);
    return true;
  }

  T* operator->() {
    return &msg;
  }

  T msg;
};

}  // namespace RpcCore
