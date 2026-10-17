#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_packed_cholesky_condition_counts.h"

namespace {
namespace counts = asc::internal_packed_cholesky_condition_counts;
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

// Literal small-domain virtual integer execution: no huge backing arrays are
// invented. Accumulate the predivision product before dividing, walk the three
// real work segments, and execute both LATPS diagonal cursor orientations.
// The two PPCON solves reuse the same buffers and repeat these bounded walks.
bool NativeExpressions(int n, int limit) {
  if (n == 0) {
    return true;
  }
  int product = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j <= n; ++j) {
      if (++product > limit) {
        return false;
      }
    }
  }
  int work = 0;
  for (int segment = 0; segment < 3; ++segment) {
    for (int i = 0; i < n; ++i) {
      if (++work > limit) {
        return false;
      }
    }
  }
  for (int solve = 0; solve < 2; ++solve) {
    int ascending = 1;
    int descending = product / 2;
    int length = 1;
    for (int j = 1; j <= n; ++j) {
      ++length;
      ascending += length;
      descending -= length;
      if (ascending > limit || descending < -limit - 1) {
        return false;
      }
    }
  }
  return true;
}

void SmallDomains(Checks& checks) {
  for (int limit = 3; limit <= 383; ++limit) {
    for (int n = 0; n <= 48; ++n) {
      checks.Equal(counts::Condition(n, limit).ok(),
                   NativeExpressions(n, limit));
    }
  }
  for (const int limit : {511, 1023, 2047, 4095, 8191}) {
    for (int n = 0; n <= 100; ++n) {
      checks.Equal(counts::Condition(n, limit).ok(),
                   NativeExpressions(n, limit));
    }
  }
}

void NativeLimits(Checks& checks, extent_t limit, extent_t boundary) {
  // These are actual INTEGER endpoints, not ASC memory-capacity claims.
  for (extent_t offset : std::array<extent_t, 3>{-1, 0, 1}) {
    const auto status = counts::Condition(boundary + offset, limit);
    checks.Equal(status.ok(), offset <= 0);
    if (offset > 0) {
      checks.Equal(status.code() == asc::ErrorCode::kOverflow, true);
    }
  }
  checks.Equal(counts::Condition(0, limit).ok(), true);
  checks.Equal(counts::Condition(1, limit).ok(), true);
  checks.Equal(counts::Condition(2, limit).ok(), true);
  checks.Equal(
      counts::Condition(limit, limit).code() == asc::ErrorCode::kOverflow,
      true);
  checks.Equal(
      counts::Condition(-1, limit).code() == asc::ErrorCode::kInvalidArgument,
      true);
  checks.Equal(
      counts::Condition(std::numeric_limits<extent_t>::max(), limit).code() ==
          asc::ErrorCode::kOverflow,
      true);
}

void Malformed(Checks& checks) {
  for (const extent_t limit : {-1, 0, 1, 2}) {
    for (const extent_t n : {-1, 0, 1}) {
      checks.Equal(counts::Condition(n, limit).code() ==
                       asc::ErrorCode::kInvalidArgument,
                   true);
    }
  }
  checks.Equal(counts::Condition(1, 3).ok(), true);
  checks.Equal(counts::Condition(2, 5).code() == asc::ErrorCode::kOverflow,
               true);
  checks.Equal(counts::Condition(2, 6).ok(), true);
}
}  // namespace

int main() {
  Checks checks;
  SmallDomains(checks);
  NativeLimits(checks, std::numeric_limits<std::int32_t>::max(), 46340);
  NativeLimits(checks, std::numeric_limits<std::int64_t>::max(), 3037000499);
  Malformed(checks);
  std::printf(
      "Packed Cholesky condition source counts: %zu checks, %zu failures\n",
      checks.checked, checks.failed);
  return checks.failed == 0 ? 0 : 1;
}
