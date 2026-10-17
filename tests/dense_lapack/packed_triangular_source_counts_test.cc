#include <array>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_triangular_counts.h"

namespace {
using asc::extent_t;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
namespace counts = asc::internal_packed_triangular_counts;

// Independently walk the selected pinned integer expressions with a small
// virtual native range. Keep wide verifier values after a simulated overflow.
// This oracle never allocates or fabricates a matrix backing span.
class Cursor {
 public:
  explicit Cursor(extent_t limit) : limit_(limit) {}
  extent_t Check(extent_t value) {
    valid_ = valid_ && value >= -limit_ - 1 && value <= limit_;
    return value;
  }
  [[nodiscard]] bool valid() const { return valid_; }

 private:
  extent_t limit_;
  bool valid_ = true;
};

void DiagonalScan(Cursor& cursor, extent_t n, bool upper, bool inverse) {
  extent_t position = upper && inverse ? 0 : 1;
  for (extent_t i = 1; i <= n; ++i) {
    if (upper && inverse) {
      position = cursor.Check(position + i);
    } else if (upper) {
      cursor.Check(cursor.Check(position + i) - 1);
      position = cursor.Check(position + i);
    } else {
      position = cursor.Check(cursor.Check(cursor.Check(position + n) - i) + 1);
    }
  }
  cursor.Check(n + 1);
}

bool InverseOracle(extent_t n, bool upper, extent_t limit) {
  if (n == 0) {
    return true;
  }
  Cursor cursor(limit);
  cursor.Check(n);
  DiagonalScan(cursor, n, upper, true);
  if (upper) {
    extent_t position = 1;
    for (extent_t j = 1; j <= n; ++j) {
      cursor.Check(cursor.Check(position + j) - 1);
      position = cursor.Check(position + j);
    }
    cursor.Check(n + 1);
  } else {
    extent_t position = cursor.Check(n * cursor.Check(n + 1)) / 2;
    for (extent_t j = n; j >= 1; --j) {
      if (j < n) {
        cursor.Check(position + 1);
        const extent_t order = n - j;
        cursor.Check(order * cursor.Check(order + 1));
      }
      position = cursor.Check(cursor.Check(cursor.Check(position - n) + j) - 2);
    }
  }
  return cursor.valid();
}

bool SolveOracle(extent_t n, extent_t nrhs, bool upper, bool unit,
                 bool transpose, extent_t limit) {
  if (n == 0) {
    return true;
  }
  Cursor cursor(limit);
  cursor.Check(n);
  cursor.Check(nrhs);
  if (!unit) {
    DiagonalScan(cursor, n, upper, false);
  }
  if (nrhs != 0) {
    if (upper != transpose) {
      extent_t position = cursor.Check(n * cursor.Check(n + 1)) / 2;
      for (extent_t j = n; j >= 1; --j) {
        if (upper) {
          cursor.Check(position - 1);
          position = cursor.Check(position - j);
        } else {
          cursor.Check(cursor.Check(position - n) + j);
          position = cursor.Check(position - (n - j + 1));
        }
      }
    } else {
      extent_t position = 1;
      for (extent_t j = 1; j <= n; ++j) {
        if (upper) {
          cursor.Check(cursor.Check(position + j) - 1);
          position = cursor.Check(position + j);
        } else {
          cursor.Check(position + 1);
          cursor.Check(j + 1);
          position = cursor.Check(position + (n - j + 1));
        }
      }
      cursor.Check(n + 1);
    }
  }
  cursor.Check(nrhs + 1);
  return cursor.valid();
}

class Checks {
 public:
  void Expect(bool result) {
    ++checks_;
    failures_ += result ? 0 : 1;
  }
  [[nodiscard]] int Finish() const {
    std::printf("Packed triangular source counts: %zu checks, %zu failures\n",
                checks_, failures_);
    return failures_ == 0 ? 0 : 1;
  }

 private:
  std::size_t checks_ = 0;
  std::size_t failures_ = 0;
};

void SmallRanges(Checks& checks) {
  for (extent_t limit = 3; limit <= 600; ++limit) {
    for (extent_t n = 0; n <= 48; ++n) {
      for (const auto triangle : {kUpper, kLower}) {
        checks.Expect(counts::Inverse(n, triangle, limit).ok() ==
                      InverseOracle(n, triangle == kUpper, limit));
        for (const auto diagonal : {kUnit, kNonUnit}) {
          for (const auto operation :
               {kNone, asc::DenseBlasTranspose::kTranspose,
                asc::DenseBlasTranspose::kConjugateTranspose}) {
            for (const auto nrhs :
                 std::array<extent_t, 4>{0, 1, limit - 1, limit}) {
              checks.Expect(counts::Solve(n, nrhs, n == 0 ? 1 : n, triangle,
                                          diagonal, operation, limit)
                                .ok() ==
                            SolveOracle(n, nrhs, triangle == kUpper,
                                        diagonal == kUnit, operation != kNone,
                                        limit));
            }
          }
        }
      }
    }
  }
}

void RealBoundaries(Checks& checks) {
  constexpr extent_t kLp64 = 2147483647;
  constexpr extent_t kIlp64 = std::numeric_limits<extent_t>::max();
  for (const auto& boundary :
       {std::array<extent_t, 3>{kLp64, 46340, 65535},
        std::array<extent_t, 3>{kIlp64, 3037000499, 4294967295}}) {
    const auto [limit, product_last, cursor_last] = boundary;
    checks.Expect(counts::Inverse(product_last, kLower, limit).ok());
    checks.Expect(!counts::Inverse(product_last + 1, kLower, limit).ok());
    checks.Expect(counts::Inverse(cursor_last, kUpper, limit).ok());
    checks.Expect(!counts::Inverse(cursor_last + 1, kUpper, limit).ok());
    checks.Expect(
        counts::Solve(limit, 0, limit, kLower, kUnit, kNone, limit).ok());
    checks.Expect(
        !counts::Solve(limit, 0, limit, kLower, kNonUnit, kNone, limit).ok());
  }
  checks.Expect(!counts::Inverse(-1, kUpper, kLp64).ok());
  checks.Expect(!counts::Solve(2, 1, 1, kUpper, kUnit, kNone, kLp64).ok());
}
}  // namespace

int main() {
  Checks checks;
  SmallRanges(checks);
  RealBoundaries(checks);
  return checks.Finish();
}
