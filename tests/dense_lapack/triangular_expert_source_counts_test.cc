#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_triangular_expert_counts.h"

namespace {
using asc::extent_t;
namespace counts = asc::internal_triangular_expert_counts;
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

// Independently walk the three simultaneous real-WORK vector segments and
// ascending source-loop terminal in a small native integer domain. Complex
// LACN2's 3*N expression reaches the same endpoint even though its WORK has
// only two segments. This is a source cursor model, not forged descriptors.
bool Cursors(int n, int nrhs, int limit) {
  if (n == 0 || nrhs == 0) {
    return true;
  }
  int cursor = 0;
  for (int segment = 0; segment < 3; ++segment) {
    for (int i = 0; i < n; ++i) {
      ++cursor;
      if (cursor > limit) {
        return false;
      }
    }
  }
  int j = 1;
  while (j <= nrhs) {
    ++j;
  }
  return j <= limit;
}

void SmallDomains(Checks& checks) {
  for (const int limit : std::array{7, 31, 127}) {
    for (int n = 0; n <= limit + 1; ++n) {
      for (int nrhs = 0; nrhs <= limit + 1; ++nrhs) {
        const extent_t leading = std::max(1, n);
        checks.Equal(
            counts::Active(n, nrhs, leading, leading, leading, limit).ok(),
            Cursors(n, nrhs, limit));
      }
    }
  }
}

void NativeLimits(Checks& checks, extent_t limit) {
  const extent_t boundary = limit / 3;
  checks.Equal(
      counts::Active(boundary, 1, boundary, boundary, boundary, limit).ok(),
      true);
  checks.Equal(counts::Active(boundary + 1, 1, boundary + 1, boundary + 1,
                              boundary + 1, limit)
                       .code() == asc::ErrorCode::kOverflow,
               true);
  checks.Equal(counts::Active(1, limit - 1, 1, 1, 1, limit).ok(), true);
  checks.Equal(counts::Active(1, limit, 1, 1, 1, limit).code() ==
                   asc::ErrorCode::kOverflow,
               true);
  checks.Equal(
      counts::Active(0, std::numeric_limits<extent_t>::max(), 1, 1, 1, limit)
          .ok(),
      true);
  checks.Equal(
      counts::Active(std::numeric_limits<extent_t>::max(), 0, 1, 1, 1, limit)
          .ok(),
      true);
  for (const std::size_t selected : std::array<std::size_t, 3>{0, 1, 2}) {
    std::array<extent_t, 3> leading{2, 2, 2};
    leading[selected] = limit;
    checks.Equal(
        counts::Active(2, 1, leading[0], leading[1], leading[2], limit).ok(),
        true);
    leading[selected] = 1;
    checks.Equal(counts::Active(2, 1, leading[0], leading[1], leading[2], limit)
                         .code() == asc::ErrorCode::kInvalidArgument,
                 true);
    if (limit < std::numeric_limits<extent_t>::max()) {
      leading[selected] = limit + 1;
      checks.Equal(
          counts::Active(2, 1, leading[0], leading[1], leading[2], limit)
                  .code() == asc::ErrorCode::kOverflow,
          true);
    }
  }
  checks.Equal(counts::Active(-1, 0, 1, 1, 1, limit).code() ==
                   asc::ErrorCode::kInvalidArgument,
               true);
  checks.Equal(counts::Active(0, -1, 1, 1, 1, limit).code() ==
                   asc::ErrorCode::kInvalidArgument,
               true);
}
}  // namespace

int main() {
  Checks checks;
  SmallDomains(checks);
  NativeLimits(checks, std::numeric_limits<std::int32_t>::max());
  NativeLimits(checks, std::numeric_limits<std::int64_t>::max());
  std::printf("Triangular estimator source counts: %zu checks, %zu failures\n",
              checks.checked, checks.failed);
  return checks.failed == 0 ? 0 : 1;
}
