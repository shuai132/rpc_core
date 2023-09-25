#pragma once

#include <cstdint>
#include <memory>

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
  t.size << ia;
  t._data_ = std::shared_ptr<uint8_t>(new uint8_t[t.size], [](const uint8_t* p) {
    delete[] p;
  });
  t.data = t._data_.get();
  memcpy(t.data, ia.data, t.size);
  ia.data += t.size;
  ia.size -= t.size;
  return ia;
}

}  // namespace rpc_core
