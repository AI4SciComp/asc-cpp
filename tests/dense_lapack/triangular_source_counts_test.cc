#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_triangular_counts.h"

namespace {
namespace counts = asc::internal_triangular_counts;
using asc::extent_t;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kTranspose = asc::DenseBlasTranspose::kTranspose;
constexpr auto kConjugate = asc::DenseBlasTranspose::kConjugateTranspose;

struct Checks {
  std::size_t total = 0;
  std::size_t failed = 0;
  void Equal(bool actual, bool expected) {
    ++total;
    if (actual != expected) {
      ++failed;
    }
  }
};

// Execute the pinned source's integer loop endpoints in a deliberately small
// simulated INTEGER domain. No descriptors, numerical arrays or native call
// are constructed. The actual 32/64-bit endpoints are checked separately.
bool InverseCursors(int n, bool upper, bool blocked, int limit) {
  if (n == 0) {
    return true;
  }
  if (n > limit) {
    return false;
  }
  if (blocked) {
    // Nonunit TRTRI scans INFO=1,N. Unit is conservatively admitted by the
    // same dimension guard; the production query is metadata-only.
    int info = 1;
    while (info <= n) {
      ++info;
    }
    if (info > limit) {
      return false;
    }
    if (n > 64 && upper) {
      int j = 1;
      while (j <= n) {
        j += 64;
      }
      if (j > limit) {
        return false;
      }
    }
    return true;
  }
  if (upper) {
    int j = 1;
    while (j <= n) {
      ++j;
    }
    return j <= limit;
  }
  // Lower J descends N..1 to0; suborder N-J<=N-1 gives every SCAL
  // increment-one loop terminal<=N. TRMV is lower/no-transpose, INCX=1.
  for (int j = n; j >= 1; --j) {
    int i = 1;
    while (i <= n - j) {
      ++i;
    }
    if (i > limit) {
      return false;
    }
  }
  return true;
}

void Simulated(Checks& checks) {
  for (const int limit : std::array{31, 127, 255}) {
    for (int n = 0; n <= limit + 1; ++n) {
      for (const auto triangle : std::array{kUpper, kLower}) {
        for (const bool blocked : std::array{false, true}) {
          checks.Equal(
              counts::Inverse(n, std::max(1, n), triangle, blocked, limit).ok(),
              InverseCursors(n, triangle == kUpper, blocked, limit));
        }
      }
    }
  }
}

void NativeLimits(Checks& checks, extent_t limit) {
  const auto max = std::numeric_limits<extent_t>::max();
  // Empty local completion does not narrow unused foreign dimensions.
  checks.Equal(counts::Inverse(0, max, kUpper, true, limit).ok(), true);
  checks.Equal(
      counts::Solve(0, max, max, max, kLower, kNonUnit, kTranspose, limit).ok(),
      true);
  checks.Equal(counts::Inverse(limit, limit, kUpper, false, limit).ok(), false);
  checks.Equal(counts::Inverse(limit, limit, kLower, false, limit).ok(), true);
  checks.Equal(counts::Inverse(limit - 1, limit, kLower, true, limit).ok(),
               true);
  checks.Equal(counts::Inverse(limit, limit, kLower, true, limit).ok(), false);
  // Upper TRTRI block-loop terminal: last valid n is the end of the block
  // preceding the final representable start. Adjacent n crosses to that start.
  const extent_t boundary = ((limit - 1) / 64) * 64;
  checks.Equal(counts::Inverse(boundary, limit, kUpper, true, limit).ok(),
               true);
  checks.Equal(counts::Inverse(boundary + 1, limit, kUpper, true, limit).ok(),
               false);
  for (auto triangle : std::array{kUpper, kLower}) {
    for (auto operation : std::array{kNone, kTranspose, kConjugate}) {
      checks.Equal(counts::Solve(limit, 0, limit, limit, triangle, kUnit,
                                 operation, limit)
                       .ok(),
                   true);
      checks.Equal(counts::Solve(limit, 0, limit, limit, triangle, kNonUnit,
                                 operation, limit)
                       .ok(),
                   false);
      checks.Equal(counts::Solve(limit, 1, limit, limit, triangle, kUnit,
                                 operation, limit)
                       .ok(),
                   triangle == kUpper && operation == kNone);
      checks.Equal(counts::Solve(limit - 1, limit, limit, limit, triangle,
                                 kUnit, operation, limit)
                       .ok(),
                   false);
      checks.Equal(counts::Solve(limit - 1, limit - 1, limit, limit, triangle,
                                 kNonUnit, operation, limit)
                       .ok(),
                   true);
    }
  }
  checks.Equal(counts::Inverse(-1, 1, kUpper, true, limit).ok(), false);
  checks.Equal(counts::Inverse(2, 1, kUpper, true, limit).ok(), false);
  checks.Equal(counts::Solve(2, -1, 2, 2, kUpper, kUnit, kNone, limit).ok(),
               false);
  checks.Equal(counts::Solve(2, 1, 1, 2, kUpper, kUnit, kNone, limit).ok(),
               false);
  checks.Equal(counts::Solve(2, 1, 2, 1, kUpper, kUnit, kNone, limit).ok(),
               false);
}
}  // namespace

int main() {
  Checks checks;
  Simulated(checks);
  NativeLimits(checks, std::numeric_limits<std::int32_t>::max());
  NativeLimits(checks, std::numeric_limits<std::int64_t>::max());
  std::printf("Triangular source counts: %zu checks, %zu failures\n",
              checks.total, checks.failed);
  return checks.failed == 0 ? 0 : 1;
}
