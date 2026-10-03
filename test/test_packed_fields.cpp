#include <cstdint>
#include <new>

#include "rpc_core.hpp"
#include "assert_def.h"

enum class field_kind : uint16_t { value = 42 };

#pragma pack(push, 1)
struct packed_external {
  uint32_t count = 123456;
  double number = 1.25;
  field_kind kind = field_kind::value;
  int* pointer = nullptr;
};
struct packed_internal {
  uint32_t count = 123456;
  double number = 1.25;
  field_kind kind = field_kind::value;
  int* pointer = nullptr;
  RPC_CORE_DEFINE_TYPE_INNER(count, number, kind, pointer);
};
#pragma pack(pop)
RPC_CORE_DEFINE_TYPE(packed_external, count, number, kind, pointer);

struct aligned_fields {
  uint32_t count = 123456;
  double number = 1.25;
  field_kind kind = field_kind::value;
  int* pointer = nullptr;
};
RPC_CORE_DEFINE_TYPE(aligned_fields, count, number, kind, pointer);

struct address_overload {
  int value = 0;
  address_overload() = default;
  address_overload(const address_overload&) = delete;
  address_overload* operator&() = delete;
  const address_overload* operator&() const = delete;
  RPC_CORE_DEFINE_TYPE_INNER(value);
};

struct non_scalar_fields {
  explicit non_scalar_fields(int& target) : reference(target) {}
  address_overload object;
  std::unique_ptr<int> pointer;
  int& reference;
};
RPC_CORE_DEFINE_TYPE(non_scalar_fields, object, pointer, reference);

template <typename T>
static void check_packed_fields() {
  // Place the packed object at a deliberately unaligned address.
  alignas(aligned_fields) unsigned char storage[sizeof(T) + 1];
  auto packed = new (storage + 1) T{};
  ASSERT(reinterpret_cast<uintptr_t>(&packed->count) % alignof(uint32_t) != 0);
  aligned_fields expected;
  int target = 42;
  expected.pointer = packed->pointer = &target;
  const auto wire = rpc_core::serialize(expected);
  ASSERT(rpc_core::serialize(*packed) == wire);
  packed->count = 0;
  packed->number = 0;
  packed->kind = static_cast<field_kind>(0);
  packed->pointer = nullptr;
  ASSERT(rpc_core::deserialize(wire, *packed));
  ASSERT(packed->count == expected.count && packed->number == expected.number);
  ASSERT(packed->kind == expected.kind && packed->pointer == &target);
  packed->~T();
}

int main() {
  check_packed_fields<packed_external>();
  check_packed_fields<packed_internal>();
  int source_value = 17, target_value = 0;
  non_scalar_fields source(source_value), target(target_value);
  source.object.value = 42;
  source.pointer.reset(new int(7));
  ASSERT(rpc_core::deserialize(rpc_core::serialize(source), target));
  ASSERT(target.object.value == 42 && *target.pointer == 7 && target_value == 17);
  ASSERT(&target.reference == &target_value);
}
