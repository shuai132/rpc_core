#include "src/Serialize.hpp"

struct CustomType {
  uint32_t id = 0;
  std::vector<uint32_t> ids;
  std::string name;
  bool operator==(const CustomType& t) const {
    return std::tie(id, ids, name) == std::tie(t.id, t.ids, t.name);
  }
};
RpcCore_DEFINE_TYPE(CustomType, id, ids, name);
