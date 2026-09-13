#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_packed_triangular_expert_counts.h"

namespace {
namespace counts = asc::internal_packed_triangular_expert_counts;
using asc::extent_t;

struct Checks {
  std::size_t checked = 0;
  std::size_t failed = 0;
  void Equal(bool actual, bool expected) {
    ++checked;
    if (actual != expected) {
      ++failed;
    }
  }
};

// Independently walk the complete predivision packed product, three vector
// segments, the RHS terminal and both LATPS diagonal cursor orientations.
// This models selected source expressions, not a hypothetical tiny-bit BLAS.
bool Cursors(int n, int nrhs, int limit) {
  if (n == 0 || nrhs == 0) {
    return true;
  }
  int cursor = 0;
  for (int row = 0; row < n; ++row) {
    for (int column = 0; column <= n; ++column) {
      if (++cursor > limit) {
        return false;
      }
    }
  }
  const int packed = cursor / 2;
  cursor = 0;
  for (int segment = 0; segment < 3; ++segment) {
    for (int i = 0; i < n; ++i) {
      if (++cursor > limit) {
        return false;
      }
    }
  }
  int ascending = 1;
  int descending = packed;
  int length = 1;
  for (int j = 1; j <= n; ++j) {
    ++length;
    ascending += length;
    descending -= length;
    if (ascending > limit || descending < -limit - 1) {
      return false;
    }
  }
  int rhs = 1;
  while (rhs <= nrhs) {
    ++rhs;
  }
  return rhs <= limit;
}

void SmallDomains(Checks& checks) {
  for (const int limit : {3, 7, 31, 127, 600}) {
    for (int n = 0; n <= 70; ++n) {
      for (const int nrhs : {0, 1, limit - 1, limit}) {
        const extent_t leading = std::max(1, n);
        checks.Equal(counts::Active(n, nrhs, leading, leading, limit).ok(),
                     Cursors(n, nrhs, limit));
      }
    }
  }
}

void NativeLimits(Checks& checks, extent_t limit, extent_t boundary) {
  checks.Equal(counts::Active(boundary, 1, boundary, boundary, limit).ok(),
               true);
  checks.Equal(
      counts::Active(boundary + 1, 1, boundary + 1, boundary + 1, limit)
              .code() == asc::ErrorCode::kOverflow,
      true);
  checks.Equal(counts::Active(1, limit - 1, 1, 1, limit).ok(), true);
  checks.Equal(
      counts::Active(1, limit, 1, 1, limit).code() == asc::ErrorCode::kOverflow,
      true);
  checks.Equal(
      counts::Active(0, std::numeric_limits<extent_t>::max(), 1, 1, limit).ok(),
      true);
  checks.Equal(
      counts::Active(std::numeric_limits<extent_t>::max(), 0, 1, 1, limit).ok(),
      true);
  for (std::size_t selected : std::array<std::size_t, 2>{0, 1}) {
    std::array<extent_t, 2> leading{2, 2};
    leading[selected] = limit;
    checks.Equal(counts::Active(2, 1, leading[0], leading[1], limit).ok(),
                 true);
    leading[selected] = 1;
    checks.Equal(counts::Active(2, 1, leading[0], leading[1], limit).code() ==
                     asc::ErrorCode::kInvalidArgument,
                 true);
    if (limit < std::numeric_limits<extent_t>::max()) {
      leading[selected] = limit + 1;
      checks.Equal(counts::Active(2, 1, leading[0], leading[1], limit).code() ==
                       asc::ErrorCode::kOverflow,
                   true);
    }
  }
  checks.Equal(counts::Active(-1, 0, 1, 1, limit).code() ==
                   asc::ErrorCode::kInvalidArgument,
               true);
  checks.Equal(counts::Active(0, -1, 1, 1, limit).code() ==
                   asc::ErrorCode::kInvalidArgument,
               true);
}
}  // namespace

int main() {
  Checks checks;
  SmallDomains(checks);
  NativeLimits(checks, std::numeric_limits<std::int32_t>::max(), 46340);
  NativeLimits(checks, std::numeric_limits<std::int64_t>::max(), 3037000499);
  std::printf("Packed estimator source counts: %zu checks, %zu failures\n",
              checks.checked, checks.failed);
  return checks.failed == 0 ? 0 : 1;
}
