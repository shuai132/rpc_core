#include "rpc_core.hpp"
#include "assert_def.h"
#include <stdexcept>

using namespace rpc_core;

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
static void test_receive_exception_invalidates_stream() {
  for (bool throw_from_deferred : {false, true}) {
    stream_connection conn;
    detail::data_packer packer;
    std::vector<std::string> received;
    // The next frame's body resembles a complete frame. Losing its real header
    // must not allow its body to be delivered as an unrelated packet.
    auto body = packer.pack("fake");
    auto header = packer.pack(body).substr(0, 4);
    conn.on_recv_package = [&](std::string data) {
      received.push_back(data);
      if (data == "start") {
        auto deferred = packer.pack("deferred") + header;
        ASSERT(conn.on_recv_bytes(deferred.data(), deferred.size()));
        if (!throw_from_deferred) throw std::runtime_error("receive failed");
      } else if (data == "deferred") {
        throw std::runtime_error("receive failed");
      }
    };
    auto input = packer.pack("start") + (throw_from_deferred ? std::string{} : header);
    bool caught = false;
    try {
      conn.on_recv_bytes(input.data(), input.size());
    } catch (const std::runtime_error& error) {
      caught = std::string(error.what()) == "receive failed";
    }
    ASSERT(caught && received.size() == (throw_from_deferred ? 2 : 1));
    const auto count = received.size();
    conn.on_recv_package = [&](std::string data) { received.push_back(std::move(data)); };
    ASSERT(!conn.on_recv_bytes(body.data(), body.size()));
    ASSERT(!conn.on_recv_bytes(nullptr, 0));
    ASSERT(received.size() == count);
    conn.reset();
    auto fresh = packer.pack("fresh");
    ASSERT(conn.on_recv_bytes(fresh.data(), fresh.size()));
    ASSERT(received.size() == count + 1 && received.back() == "fresh");
  }
}
#endif

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

static void test_saved_send_callback_after_connection_destruction() {
  auto conn = std::make_shared<stream_connection>();
  std::weak_ptr<stream_connection> weak = conn;
  int sends = 0;
  conn->send_bytes_impl = [&](std::string data) {
    ASSERT(data == detail::data_packer().pack("live"));
    ++sends;
    return true;
  };
  std::function<bool(std::string)> send = conn->send_package_impl;
  ASSERT(send("live") && sends == 1);
  conn.reset();
  ASSERT(weak.expired());
  ASSERT(!send("expired") && sends == 1);
}

int main() {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  test_receive_exception_invalidates_stream();
#endif
  test_saved_send_callback_after_connection_destruction();
  test_send_replacement_during_response();
  test_callbacks_keep_state_and_survive_owner_destruction();
}
