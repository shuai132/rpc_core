#pragma once

#include <cstdint>
#include <string>

namespace rpc_core {
namespace detail {

inline void append_u16_le(std::string& data, uint16_t value) {
  data.push_back(static_cast<char>(value & 0xff));
  data.push_back(static_cast<char>((value >> 8) & 0xff));
}

inline void append_u32_le(std::string& data, uint32_t value) {
  data.push_back(static_cast<char>(value & 0xff));
  data.push_back(static_cast<char>((value >> 8) & 0xff));
  data.push_back(static_cast<char>((value >> 16) & 0xff));
  data.push_back(static_cast<char>((value >> 24) & 0xff));
}

inline void write_u32_le(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value & 0xff);
  data[1] = static_cast<uint8_t>((value >> 8) & 0xff);
  data[2] = static_cast<uint8_t>((value >> 16) & 0xff);
  data[3] = static_cast<uint8_t>((value >> 24) & 0xff);
}

inline uint16_t read_u16_le(const char* data) {
  const auto* p = reinterpret_cast<const uint8_t*>(data);
  return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1] << 8);
}

inline uint32_t read_u32_le(const char* data) {
  const auto* p = reinterpret_cast<const uint8_t*>(data);
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace detail
}  // namespace rpc_core
