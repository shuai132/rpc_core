#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "detail/copyable.hpp"
#include "detail/log.h"

namespace RpcCore {

/**
 * 数据序列化接口
 * std::string作为数据载体 并不要求可读
 */
struct Message : copyable {
  virtual std::string serialize() const = 0;
  virtual bool deSerialize(const std::string& data) = 0;
};

/**
 * 基本数据类型模板
 */
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
 * 结构体类型模板 用于保存用户自定义结构体
 * 为确保在不同平台有一致的结构体内存
 * 1. 收发端T需使用一致的字节对齐
 * 2. 使用字长平台无关的数据类型 如int32_t/int8_t等代替int（显然不要使用size_t）
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

using Void = Raw<uint8_t>;
const Void VOID{};

/**
 * string类型 实际可存二进制内容 无需为可读的字符串
 */
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

/**
 * 二进制数据
 */
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
