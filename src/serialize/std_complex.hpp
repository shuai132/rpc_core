#pragma once

#include <complex>
#include <type_traits>

namespace RPC_CORE_NAMESPACE {

namespace detail {

template <typename T>
struct is_std_complex : std::false_type {};

template <typename T>
struct is_std_complex<std::complex<T>> : std::true_type {};

}  // namespace detail

template <typename T, typename std::enable_if<detail::is_std_complex<T>::value, int>::type = 0>
serialize_oarchive& operator>>(const T& t, serialize_oarchive& oa) {
  using VT = typename T::value_type;
  if (std::is_fundamental<VT>::value) {
    t.real() >> oa;
    t.imag() >> oa;
  } else {
    oa & t.real();
    oa & t.imag();
  }
  return oa;
}

template <typename T, typename std::enable_if<detail::is_std_complex<T>::value, int>::type = 0>
serialize_iarchive& operator<<(T& t, serialize_iarchive& ia) {
  using VT = typename T::value_type;
  if (std::is_fundamental<VT>::value) {
    VT real;
    real << ia;
    t.real(real);
    VT imag;
    imag << ia;
    t.imag(imag);
  } else {
    VT real;
    ia & real;
    t.real(std::move(real));
    VT imag;
    ia & imag;
    t.imag(std::move(imag));
  }
  return ia;
}

}  // namespace RPC_CORE_NAMESPACE
