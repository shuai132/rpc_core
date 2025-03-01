#include <cstdint>
#include <limits>
#include <string>

#include "assert_def.h"
#include "rpc_core.hpp"

int main() {
  using rpc_core::detail::coder;
  using rpc_core::detail::msg_wrapper;

  // Cover every change in encoded length, including the old seq=300 regression.
  for (uint32_t seq : {0u, 1u, 127u, 128u, 255u, 256u, 300u, 16383u, 16384u, (std::numeric_limits<uint32_t>::max)()}) {
    const auto encoded = rpc_core::detail::to_varint(seq);
    uint32_t decoded_seq = 0;
    size_t bytes = 0;
    ASSERT(rpc_core::detail::from_varint(encoded.data(), encoded.size(), decoded_seq, bytes));
    ASSERT(decoded_seq == seq && bytes == encoded.size());
  }

  msg_wrapper msg;
  msg.seq = 300;
  msg.cmd = "xy";
  msg.type = msg_wrapper::command;
  msg.data = "z";
  const auto payload = coder::serialize(msg);
  ASSERT(payload.first && payload.second == std::string("\xac\x02\x02xy\x01z", 7));
  auto decoded = coder::deserialize(payload.second);
  ASSERT(decoded.first && decoded.second.seq == msg.seq && decoded.second.cmd == msg.cmd && decoded.second.type == msg.type && decoded.second.data == msg.data);

  // Payload data may be empty, but an incomplete header is invalid.
  for (size_t size = 0; size < payload.second.size() - msg.data.size(); ++size) {
    decoded = coder::deserialize(payload.second.substr(0, size));
    ASSERT(!decoded.first && decoded.second.cmd.empty() && decoded.second.data.empty());
  }
  for (const auto& invalid : {std::string("\x80", 1), std::string("\x80\x80\x80\x80\x80", 5), std::string("\x80\x80\x80\x80\x10", 5),
                              std::string("\x01\x80", 2), std::string("\x01\x80\x80\x04\x01", 5)}) {
    decoded = coder::deserialize(invalid);
    ASSERT(!decoded.first && decoded.second.cmd.empty() && decoded.second.data.empty());
  }

  msg.seq = (std::numeric_limits<uint32_t>::max)();
  msg.cmd.assign(130, 'x');
  auto encoded = coder::serialize(msg);
  ASSERT(encoded.first);
  decoded = coder::deserialize(encoded.second);
  ASSERT(decoded.first && decoded.second.seq == msg.seq && decoded.second.cmd == msg.cmd);

  msg.cmd.assign((std::numeric_limits<uint16_t>::max)(), 'x');
  encoded = coder::serialize(msg);
  ASSERT(encoded.first);
  decoded = coder::deserialize(encoded.second);
  ASSERT(decoded.first && decoded.second.cmd == msg.cmd);

  msg.cmd.assign(static_cast<size_t>((std::numeric_limits<uint16_t>::max)()) + 1, 'x');
  encoded = coder::serialize(msg);
  ASSERT(!encoded.first && encoded.second.empty());
}
