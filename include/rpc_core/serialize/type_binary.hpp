#pragma once

#include <cstdint>

namespace rpc_core {

struct binary {
  binary() = default;
  binary(void* data, size_t size, bool need_free = false) : data(data), size(size), need_free(need_free) {}
  ~binary() {
    if (need_free) {
      free(data);
    }
  }
  void* data = nullptr;
  size_t size = 0;
  bool need_free = false;
};

template <typename T, typename std::enable_if<std::is_same<T, binary>::value, int>::type = 0>
inline serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  t.size >> oa;
  oa.data.append((char*)t.data, t.size);
  return oa;
}

template <typename T, typename std::enable_if<std::is_same<T, binary>::value, int>::type = 0>
inline serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  t.size << ia;
  t.data = malloc(t.size);
  t.need_free = true;
  memcpy(t.data, ia.data, t.size);
  ia.data += t.size;
  ia.size -= t.size;
  return ia;
}

}  // namespace rpc_core
