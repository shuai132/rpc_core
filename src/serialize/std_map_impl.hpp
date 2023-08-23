#pragma once

namespace RpcCore {

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type>
std::string serialize(const T& t) {
  uint32_t size = t.size();
  std::string payload;
  payload.append((char*)&size, sizeof(size));
  for (auto& i : t) {
    auto item = serialize(i);
    uint32_t item_size = item.size();
    payload.append((char*)&item_size, sizeof(item_size));
    payload.append((char*)item.data(), item.size());
  }
  return payload;
}

template <typename T, typename std::enable_if<detail::is_map_like<T>::value, int>::type>
inline bool deserialize(const detail::string_view& data, T& t) {
  char* p = (char*)data.data();
  uint32_t size = *(uint32_t*)p;
  p += sizeof(size);
  for (uint32_t i = 0; i < size; ++i) {
    uint32_t item_size = *(uint32_t*)p;
    p += sizeof(item_size);
    typename T::value_type item;
    if (!deserialize(detail::string_view(p, item_size), item)) {
      return false;
    }
    p += item_size;
    t.insert(std::move(item));
  }
  return true;
}

}  // namespace RpcCore
