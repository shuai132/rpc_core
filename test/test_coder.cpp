#include <limits>

#include "assert_def.h"
#include "rpc_core.hpp"

int main() {
  using rpc_core::detail::coder;
  using rpc_core::detail::msg_wrapper;

  msg_wrapper msg;
  msg.seq = 300;
  msg.cmd = "xy";
  msg.type = msg_wrapper::command;
  msg.data = "z";
  const auto payload = coder::serialize(msg);
  ASSERT(payload.first && payload.second == std::string("\x2c\x01\x00\x00\x02\x00xy\x01z", 10));
  auto decoded = coder::deserialize(payload.second);
  ASSERT(decoded.first && decoded.second.seq == msg.seq && decoded.second.cmd == msg.cmd && decoded.second.type == msg.type && decoded.second.data == msg.data);

  // Payload data may be empty, but an incomplete header is invalid.
  for (size_t size = 0; size < payload.second.size() - msg.data.size(); ++size) {
    decoded = coder::deserialize(payload.second.substr(0, size));
    ASSERT(!decoded.first && decoded.second.cmd.empty() && decoded.second.data.empty());
  }
  auto invalid = payload.second;
  invalid[4] = static_cast<char>(0xff);
  invalid[5] = static_cast<char>(0xff);
  decoded = coder::deserialize(invalid);
  ASSERT(!decoded.first && decoded.second.cmd.empty() && decoded.second.data.empty());

  msg.cmd.assign((std::numeric_limits<uint16_t>::max)(), 'x');
  auto encoded = coder::serialize(msg);
  ASSERT(encoded.first);
  decoded = coder::deserialize(encoded.second);
  ASSERT(decoded.first && decoded.second.cmd == msg.cmd);

  msg.cmd.assign(static_cast<size_t>((std::numeric_limits<uint16_t>::max)()) + 1, 'x');
  encoded = coder::serialize(msg);
  ASSERT(!encoded.first && encoded.second.empty());
}
