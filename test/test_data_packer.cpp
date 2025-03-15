#include <ctime>
#include <random>
#include <vector>

#include "assert_def.h"
#include "rpc_core.hpp"
#include "test.h"

static void test_simple() {
  RPC_CORE_LOGI();
  RPC_CORE_LOGI("test_simple...");
  rpc_core::detail::data_packer packer;
  std::string testData = "hello world";
  std::string packedData;

  packer.pack(testData.data(), testData.size(), [&](const void *data, size_t size) {
    packedData.insert(packedData.size(), (char *)data, size);
    return true;
  });
  ASSERT(packedData.size() == testData.size() + 4);

  std::string feedRecData;
  packer.on_data = [&](std::string data) {
    feedRecData = std::move(data);
  };
  packer.feed(packedData.data(), packedData.size());
  ASSERT(testData == feedRecData);
  RPC_CORE_LOGI("packedData PASS");

  std::string packedData2 = packer.pack(testData);
  ASSERT(packedData2 == packedData);
  RPC_CORE_LOGI("packedData2 PASS");

  RPC_CORE_LOGI("feed again...");
  feedRecData.clear();
  ASSERT(testData != feedRecData);
  packer.feed(packedData.data(), packedData.size());
  ASSERT(testData == feedRecData);
}

static void test_random() {
  RPC_CORE_LOGI();
  RPC_CORE_LOGI("test_random...");
  RPC_CORE_LOGI("generate big data...");
  bool pass = false;
  std::string TEST_PAYLOAD;
  size_t TestAddCount = 1000;
  for (size_t i = 0; i < TestAddCount; i++) {
    TEST_PAYLOAD += "helloworld";  // 10bytes
  }
  RPC_CORE_LOGI("data generated, size:%zu", TEST_PAYLOAD.size());
  ASSERT(TEST_PAYLOAD.size() == TestAddCount * 10);

  rpc_core::detail::data_packer packer;
  packer.on_data = [&](const std::string &data) {
    size_t size = data.size();
    RPC_CORE_LOGI("get payload size:%zu", size);
    if (data == TEST_PAYLOAD) {
      pass = true;
    }
  };

  RPC_CORE_LOGI("packing...");
  auto payload = packer.pack(TEST_PAYLOAD);
  const size_t payloadSize = payload.size();
  RPC_CORE_LOGI("payloadSize:%zu", payloadSize);
  ASSERT(payloadSize == TestAddCount * 10 + 4);

  RPC_CORE_LOGI("******test normal******");
  packer.feed(payload.data(), payloadSize);
  ASSERT(pass);
  pass = false;

  RPC_CORE_LOGI("******test random******");
  size_t sendLeft = payloadSize;
  std::default_random_engine generator((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<int> dis(1, 10);
  auto random = std::bind(dis, generator);  // NOLINT
  while (sendLeft > 0) {
    size_t randomSize = random();
    // RPC_CORE_LOGI("random: %u,  %u", randomSize, sendLeft);
    size_t needSend = std::min(randomSize, sendLeft);
    packer.feed(payload.data() + (payloadSize - sendLeft), needSend);
    sendLeft -= needSend;
  }
  ASSERT(pass);
}

static void test_empty_body() {
  rpc_core::detail::data_packer packer;
  std::vector<std::string> received;
  packer.on_data = [&](std::string data) {
    received.emplace_back(std::move(data));
  };

  auto empty = packer.pack(std::string{});
  auto nonempty = packer.pack(std::string("next"));
  packer.feed(empty.data(), empty.size());
  ASSERT(received.size() == 1);
  ASSERT(received[0].empty());

  packer.feed(nonempty.data(), nonempty.size());
  ASSERT(received.size() == 2);
  ASSERT(received[1] == "next");

  received.clear();
  auto combined = empty + nonempty;
  packer.feed(combined.data(), 2);
  packer.feed(combined.data() + 2, combined.size() - 2);
  ASSERT(received.size() == 2);
  ASSERT(received[0].empty());
  ASSERT(received[1] == "next");
}

static void test_callback_can_feed_next_frame() {
  for (bool empty : {false, true}) {
    rpc_core::detail::data_packer packer;
    std::vector<std::string> received;
    auto next = packer.pack("next");
    packer.on_data = [&](std::string data) {
      received.push_back(std::move(data));
      if (received.size() == 1) {
        ASSERT(packer.feed(next.data(), empty ? 2 : next.size()));
      }
    };
    auto first = packer.pack(empty ? "" : "first");
    ASSERT(packer.feed(first.data(), first.size()));
    if (empty) ASSERT(packer.feed(next.data() + 2, next.size() - 2));
    ASSERT(received.size() == 2 && received[1] == "next");
  }
}

static void test_reentrant_feed_follows_buffered_frames() {
  rpc_core::detail::data_packer packer;
  std::vector<std::string> received;
  auto third = packer.pack("three");
  packer.on_data = [&](std::string data) {
    received.push_back(std::move(data));
    if (received.size() == 1) ASSERT(packer.feed(third.data(), 2));
  };
  auto first_two = packer.pack("one") + packer.pack("two");
  ASSERT(packer.feed(first_two.data(), first_two.size()));
  ASSERT(packer.feed(third.data() + 2, third.size() - 2));
  ASSERT((received == std::vector<std::string>{"one", "two", "three"}));
}

static void test_many_frames_do_not_recurse() {
  rpc_core::detail::data_packer packer;
  size_t received = 0;
  packer.on_data = [&](std::string data) { ASSERT(data.empty()); ++received; };
  std::string frames(4 * 100000, '\0');
  ASSERT(packer.feed(frames.data(), frames.size()));
  ASSERT(received == 100000);
}

static void test_reset_during_delivery_discards_old_stream() {
  rpc_core::detail::data_packer packer;
  std::vector<std::string> received;
  auto next = packer.pack("new");
  packer.on_data = [&](std::string data) {
    received.push_back(std::move(data));
    if (received.size() == 1) {
      ASSERT(packer.feed(next.data(), 2));
      packer.reset();
      ASSERT(packer.feed(next.data(), next.size()));
    }
  };
  auto old = packer.pack("first") + packer.pack("discarded");
  ASSERT(packer.feed(old.data(), old.size()));
  ASSERT((received == std::vector<std::string>{"first", "new"}));
}

static void test_invalid_stream_requires_reset() {
  rpc_core::stream_connection conn(8);
  int received = 0;
  conn.on_recv_package = [&](std::string data) { ASSERT(data == "valid"); ++received; };
  auto frame = rpc_core::detail::data_packer().pack("valid");
  uint32_t oversized = 9;
  ASSERT(!conn.on_recv_bytes(&oversized, sizeof(oversized)));
  ASSERT(!conn.on_recv_bytes(frame.data(), frame.size()));
  ASSERT(received == 0);
  conn.reset();
  ASSERT(conn.on_recv_bytes(frame.data(), frame.size()));
  ASSERT(received == 1);
}

static void test_stream_callback_can_destroy_connection(bool save_callback) {
  for (bool through_rpc : {false, true}) {
    auto conn = std::make_shared<rpc_core::stream_connection>();
    std::weak_ptr<rpc_core::stream_connection> observer = conn;
    rpc_core::rpc_s rpc;
    int received = 0;
    auto destroy = [&] {
      ++received;
      rpc.reset();
      conn.reset();
    };
    std::string payload = "stop";
    if (through_rpc) {
      rpc = rpc_core::rpc::create(conn);
      rpc->subscribe("stop", destroy);
      rpc_core::detail::msg_wrapper msg;
      msg.cmd = "stop";
      payload = rpc_core::detail::coder::serialize(msg).second;
    } else {
      conn->on_recv_package = [&](std::string) { destroy(); };
    }
    auto frame = rpc_core::detail::data_packer().pack(payload);
    auto buffered = frame + frame;
    std::function<bool(const void*, size_t)> saved_receive;
    if (save_callback) saved_receive = conn->on_recv_bytes;
    ASSERT(conn->on_recv_bytes(buffered.data(), buffered.size()));
    ASSERT(observer.expired() && received == 1);
    if (saved_receive) ASSERT(!saved_receive(frame.data(), frame.size()));
    ASSERT(received == 1);
  }
}

namespace rpc_core_test {

void test_data_packer() {
  test_simple();
  test_random();
  test_empty_body();
  test_callback_can_feed_next_frame();
  test_many_frames_do_not_recurse();
  test_reentrant_feed_follows_buffered_frames();
  test_reset_during_delivery_discards_old_stream();
  test_invalid_stream_requires_reset();
  test_stream_callback_can_destroy_connection(false);
  test_stream_callback_can_destroy_connection(true);
}

}  // namespace rpc_core_test
