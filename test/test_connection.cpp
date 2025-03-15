#include "rpc_core.hpp"
#include "assert_def.h"

using namespace rpc_core;

static void test_send_replacement_during_response() {
  for (bool clear : {false, true}) {
    auto conn = std::make_shared<connection>();
    auto r = rpc::create(conn);
    r->set_ready(true);
    r->set_timer([](uint32_t, rpc::timeout_cb) {});
    auto token = std::make_shared<int>(42);
    std::weak_ptr<int> weak = token;
    conn->send_package_impl = [&, token](std::string bytes) {
      auto msg = detail::coder::deserialize(bytes).second;
      auto reply = detail::msg_wrapper::make_rsp<int>(msg.seq);
      conn->on_recv_package(detail::coder::serialize(reply).second);
      ASSERT(!weak.expired() && *token == 42);
      return true;
    };
    token.reset();
    int responses = 0;
    auto req = r->cmd("x")->rsp([&] {
      ++responses;
      if (clear) conn->send_package_impl = nullptr;
      else conn->send_package_impl = [](std::string) { return true; };
    });
    ASSERT(req->call());
    ASSERT(responses == 1 && weak.expired());
    ASSERT(conn->send_package({}) == !clear);
  }
}

static void test_callbacks_keep_state_and_survive_owner_destruction() {
  auto conn = std::make_shared<stream_connection>();
  std::vector<int> counts;
  conn->send_bytes_impl = [&, count = 0](std::string) mutable {
    counts.push_back(++count);
    return true;
  };
  ASSERT(conn->send_package("a") && conn->send_package("b"));
  ASSERT((counts == std::vector<int>{1, 2}));
  auto token = std::make_shared<int>(42);
  std::weak_ptr<int> weak = token;
  conn->on_recv_package = [&, token](std::string data) {
    ASSERT(data == "a");
    conn.reset();
    ASSERT(!weak.expired() && *token == 42);
  };
  token.reset();
  detail::data_packer packer;
  auto data = packer.pack("a") + packer.pack("b");
  // Also exercise conversion to the std::function used by transport adapters.
  std::function<bool(const void*, size_t)> receive = conn->on_recv_bytes;
  ASSERT(receive(data.data(), data.size()));
  ASSERT(weak.expired() && !receive(data.data(), data.size()));

  conn = std::make_shared<stream_connection>();
  token = std::make_shared<int>(42);
  weak = token;
  conn->send_bytes_impl = [&, token](std::string) {
    conn.reset();
    ASSERT(!weak.expired() && *token == 42);
    return true;
  };
  token.reset();
  ASSERT(conn->send_package("last"));
  ASSERT(weak.expired());
}

int main() {
  test_send_replacement_during_response();
  test_callbacks_keep_state_and_survive_owner_destruction();
}
