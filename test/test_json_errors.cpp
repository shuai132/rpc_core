#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "assert_def.h"
#include "rpc_core/detail/log.h"

static std::vector<std::string> errors;

static void capture_error(const char* format, ...) {
  char message[2048];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  errors.emplace_back(message);
}

#undef RPC_CORE_LOGE
#define RPC_CORE_LOGE(...) capture_error(__VA_ARGS__)

#include "rpc_core/serialize.hpp"
#ifndef RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON
#include "rpc_core/plugin/json.hpp"
#include "rpc_core/plugin/json_msg.hpp"
#endif

static const char* secret = "test-only-secret-token";

struct RejectedMessage {};

void from_json(const nlohmann::json& json, RejectedMessage&) {
  throw std::runtime_error(json.at("token").get<std::string>());
}

#ifndef RPC_CORE_SERIALIZE_USE_NLOHMANN_JSON
RPC_CORE_DEFINE_TYPE_NLOHMANN_JSON(RejectedMessage)
#endif

int main() {
  // Both parser diagnostics and application conversion errors may contain payload data.
  const std::string malformed = std::string("{\"token\":\"") + secret + "\n\"}";
  nlohmann::json json;
  ASSERT(!rpc_core::deserialize(malformed, json));
  RejectedMessage message;
  const std::string valid = nlohmann::json{{"token", secret}}.dump();
  ASSERT(!rpc_core::deserialize(valid, message));
  ASSERT(errors.size() == 2);
  for (const auto& error : errors) {
    ASSERT(!error.empty());
    ASSERT(error.find(secret) == std::string::npos);
  }
  const std::string normal = "{\"value\":7}";
  ASSERT(rpc_core::deserialize(normal, json));
  ASSERT(json.at("value") == 7);
  ASSERT(errors.size() == 2);
}
