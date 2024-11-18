#pragma once

#include <memory>
#include <utility>

// config
#include "config.hpp"

namespace rpc_core {

enum class finally_t : int {
  normal = 0,
  no_need_rsp = 1,
  timeout = 2,
  canceled = 3,
  rpc_expired = 4,
  rpc_not_ready = 5,
  rsp_serialize_error = 6,
  no_such_cmd = 7,
};

inline const char* finally_t_str(finally_t t) {
  switch (t) {
    case finally_t::normal:
      return "normal";
    case finally_t::no_need_rsp:
      return "no_need_rsp";
    case finally_t::timeout:
      return "timeout";
    case finally_t::canceled:
      return "canceled";
    case finally_t::rpc_expired:
      return "rpc_expired";
    case finally_t::rpc_not_ready:
      return "rpc_not_ready";
    case finally_t::rsp_serialize_error:
      return "rsp_serialize_error";
    case finally_t::no_such_cmd:
      return "no_such_cmd";
    default:
      return "unknown";
  }
}

template <typename T>
struct result;

template <typename T>
struct result {
  finally_t type;
  T data;

  explicit operator bool() const {
    return type == finally_t::normal;
  }

  T& operator*() {
    return data;
  }

  T&& take() {
    return std::move(data);
  }
};

template <>
struct result<void> {
  finally_t type;

  explicit operator bool() const {
    return type == finally_t::normal;
  }
};

}  // namespace rpc_core
