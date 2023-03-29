#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "detail/copyable.hpp"
#include "detail/log.h"

namespace RpcCore {

struct Message : detail::copyable {
  virtual std::string serialize() const = 0;
  virtual bool deSerialize(const std::string& data) = 0;
};

template <typename T, typename std::enable_if<!std::is_class<T>::value, int>::type = 0>
struct Raw : Message {
  T value;

  Raw() = default;        // NOLINT
  Raw(T v) : value(v) {}  // NOLINT

  friend bool operator==(const Raw<T>& l, const T& r) {
    return l.value == r;
  }
  friend bool operator==(const T& l, const Raw<T>& r) {
    return l == r.value;
  }
  friend bool operator==(const Raw<T>& l, const Raw<T>& r) {
    return l.value == r.value;
  }

  std::string serialize() const override {
    return {(char*)&value, sizeof(T)};
  };
  bool deSerialize(const std::string& data) override {
    return memcpy((void*)&value, data.data(), sizeof(T));
  };
};

/**
 * A struct template used to store user-defined structs.
 *
 * 1. T needs to ensure the same memory layout across different platforms.
 * 2. Use platform-independent types, such as int32_t instead of int, uint32_t instead of size_t.
 *
 * NOTE:
 * This interface is not recommended to use unless you know what you are doing.
 * It is recommended to use the serialization method in the plugin.
 */
template <typename T, typename std::enable_if<std::is_class<T>::value, int>::type = 0>
struct Struct : Message {
  T value;
  uint8_t align_size;

  Struct() : align_size(alignof(T)) {}
  Struct(T v) : value(v), align_size(alignof(T)) {}  // NOLINT

  std::string serialize() const override {
    return {(char*)&value, sizeof(T) + 1};
  };
  bool deSerialize(const std::string& data) override {
    if (data.size() != sizeof(T) + 1) {
      RpcCore_LOGE("wrong data size");
      return false;
    }
    memcpy(&value, data.data(), sizeof(T) + 1);
    if (align_size != alignof(T)) {
      RpcCore_LOGE("wrong align_size: alignof(T)=%d != %zu", align_size, alignof(T));
      return false;
    }
    return true;
  }
};

struct Void : Message {
  std::string serialize() const override {
    return {};
  };
  bool deSerialize(const std::string& data) override {
    (void)data;
    return true;
  }
};

struct String : Message, public std::string {
  using std::string::string;
  String() = default;
  String(std::string str) {  // NOLINT
    this->swap(str);
  }
  std::string serialize() const override {
    return *this;  // NOLINT
  }
  bool deSerialize(const std::string& data) override {
    *this = data;
    return true;
  }
};

using Binary = String;

struct Bool : Raw<uint8_t> {
  Bool() {
    value = 0;
  };
  Bool(bool v) {  // NOLINT
    value = v;
  }
  operator bool() {  // NOLINT
    return value != 0;
  }
};

}  // namespace RpcCore
