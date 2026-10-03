#pragma once

#include <cstdint>
#include <memory>
#include <utility>

namespace rpc_core {

struct binary_wrap {
  binary_wrap() = default;
  binary_wrap(void* data, size_t size) : data(data), size(size) {}
  void* data = nullptr;
  size_t size = 0;

  // private:
  std::shared_ptr<uint8_t> _data_;
};

template <typename T, typename std::enable_if<std::is_same<T, binary_wrap>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  t.size >> oa;
  oa.data.append((char*)t.data, t.size);
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, binary_wrap>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  size_t size = 0;
  size << ia;
  if (!ia.require(size)) return ia;
  auto data = std::shared_ptr<uint8_t>(new uint8_t[size], [](const uint8_t* p) {
    delete[] p;
  });
  // Keep the previous storage alive while copying; the input may refer to it.
  if (size != 0) memcpy(data.get(), ia.data, size);
  ia.data += size;
  ia.size -= size;
  t._data_ = std::move(data);
  t.data = t._data_.get();
  t.size = size;
  return ia;
}

}  // namespace rpc_core
