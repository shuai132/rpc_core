#pragma once

#include <deque>
#include <list>
#include <queue>
#include <vector>

namespace RpcCore {

namespace detail {

template <typename T>
struct is_list_like : std::false_type {};

template <typename... Args>
struct is_list_like<std::vector<Args...>> : std::true_type {};

template <typename... Args>
struct is_list_like<std::list<Args...>> : std::true_type {};

template <typename... Args>
struct is_list_like<std::deque<Args...>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type = 0>
inline std::string serialize(const T& t) {
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

template <typename T, typename std::enable_if<detail::is_list_like<T>::value, int>::type = 0>
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
    t.push_back(std::move(item));
  }
  return true;
}

}  // namespace RpcCore
