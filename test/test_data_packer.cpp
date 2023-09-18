#include <ctime>
#include <random>

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
  const uint32_t payloadSize = payload.size();
  RPC_CORE_LOGI("payloadSize:%u", payloadSize);
  ASSERT(payloadSize == TestAddCount * 10 + 4);

  RPC_CORE_LOGI("******test normal******");
  packer.feed(payload.data(), payloadSize);
  ASSERT(pass);
  pass = false;

  RPC_CORE_LOGI("******test random******");
  uint32_t sendLeft = payloadSize;
  std::default_random_engine generator(time(nullptr));  // NOLINT
  std::uniform_int_distribution<int> dis(1, 10);
  auto random = std::bind(dis, generator);  // NOLINT
  while (sendLeft > 0) {
    uint32_t randomSize = random();
    // RPC_CORE_LOGI("random: %u,  %u", randomSize, sendLeft);
    size_t needSend = std::min(randomSize, sendLeft);
    packer.feed(payload.data() + (payloadSize - sendLeft), needSend);
    sendLeft -= needSend;
  }
  ASSERT(pass);
}

namespace rpc_core_test {

void test_data_packer() {
  test_simple();
  test_random();
}

}  // namespace rpc_core_test
