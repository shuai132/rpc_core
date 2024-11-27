#pragma once

#include <memory>
#include <utility>

// config
#include "config.hpp"

// include
#include "detail/callable/callable.hpp"
#include "detail/noncopyable.hpp"

namespace rpc_core {

template <typename Req, typename Rsp>
struct request_response_impl : public std::enable_shared_from_this<request_response_impl<Req, Rsp>>, private detail::noncopyable {
  using request_response_t = request_response_impl<Req, Rsp>;

 public:
  static std::shared_ptr<request_response_t> create() {
    struct helper : public request_response_t {
      explicit helper() : request_response_t() {}
    };
    return std::make_shared<helper>();
  }

  std::weak_ptr<request_response_t> weak_ptr() {
    return request_response_t::shared_from_this();
  }

 private:
  request_response_impl() = default;

 public:
  Req req;
  Rsp rsp_data;

  bool rsp_ready{false};
  std::function<void(Rsp)> rsp;
};

template <typename Req, typename Rsp>
using request_response = std::shared_ptr<rpc_core::request_response_impl<Req, Rsp>>;

namespace detail {

template <typename T>
struct is_request_response : std::false_type {};

template <typename... Args>
struct is_request_response<std::shared_ptr<rpc_core::request_response_impl<Args...>>> : std::true_type {};

template <typename F, bool ONE_PARAM = false>
struct fp_is_request_response_helper {
  static constexpr bool value = false;
};

template <typename F>
struct fp_is_request_response_helper<F, true> {
  using request_response = detail::remove_cvref_t<typename detail::callable_traits<F>::template argument_type<0>>;
  static constexpr bool value = detail::is_request_response<request_response>::value;
};

template <typename F>
struct fp_is_request_response {
  static constexpr bool ONE_PARAM = detail::callable_traits<F>::argc == 1;
  static constexpr bool value = fp_is_request_response_helper<F, ONE_PARAM>::value;
};

}  // namespace detail

}  // namespace rpc_core
