#include <cstdint>
#include <type_traits>

#include "asc/core.h"

int main() {
  static_assert(std::is_same_v<asc::index_t, std::int64_t>);
  static_assert(std::is_same_v<asc::extent_t, std::int64_t>);
  static_assert(std::is_same_v<asc::stride_t, std::int64_t>);
  static_assert(std::is_same_v<asc::nnz_t, std::int64_t>);
  static_assert(std::is_same_v<asc::rank_t, std::uint32_t>);
  const asc::Status status = asc::Status::Ok();
  if (!status.ok()) {
    return 1;
  }
  return asc::ErrorCodeName(asc::ErrorCode::kOk).empty() ? 2 : 0;
}
