#include <array>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_cholesky_inverse_counts.h"

namespace {
using asc::extent_t;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
namespace counts = asc::internal_packed_cholesky_inverse_counts;

// Literal integer statements use wide verifier arithmetic with a small virtual
// provider range. No matrix descriptor, pointer or fake backing span is made.
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

void ScalarLoops(Cursor& cursor, extent_t n, bool unrolled) {
  if (n == 0) {
    return;
  }
  if (!unrolled) {
    for (extent_t i = 1; i <= n; ++i) {
      cursor.Check(i);
    }
    cursor.Check(n + 1);
    return;
  }
  const extent_t remainder = n % 5;
  cursor.Check(remainder + 1);
  if (n < 5) {
    return;
  }
  for (extent_t i = remainder + 1; i <= n; i += 5) {
    cursor.Check(i + 4);
    cursor.Check(i + 5);
  }
}

void PackedProduct(Cursor& cursor, extent_t n, bool upper) {
  if (n == 0) {
    return;
  }
  if (upper) {
    extent_t kk = 1;
    for (extent_t j = 1; j <= n; ++j) {
      cursor.Check(cursor.Check(kk + j) - 1);
      kk = cursor.Check(kk + j);
    }
    cursor.Check(n + 1);
  } else {
    extent_t kk = cursor.Check(n * cursor.Check(n + 1)) / 2;
    for (extent_t j = n; j >= 1; --j) {
      cursor.Check(cursor.Check(kk - n) + j);
      kk = cursor.Check(kk - (n - j + 1));
    }
  }
}

void TriangularInverse(Cursor& cursor, extent_t n, bool upper) {
  extent_t diagonal = upper ? 0 : 1;
  for (extent_t j = 1; j <= n; ++j) {
    if (upper) {
      diagonal = cursor.Check(diagonal + j);
    } else {
      diagonal = cursor.Check(cursor.Check(cursor.Check(diagonal + n) - j) + 1);
    }
  }
  cursor.Check(n + 1);
  if (upper) {
    extent_t jc = 1;
    for (extent_t j = 1; j <= n; ++j) {
      cursor.Check(cursor.Check(jc + j) - 1);
      PackedProduct(cursor, j - 1, true);
      ScalarLoops(cursor, j - 1, true);
      ScalarLoops(cursor, j - 1, false);
      jc = cursor.Check(jc + j);
    }
  } else {
    extent_t jc = cursor.Check(n * cursor.Check(n + 1)) / 2;
    for (extent_t j = n; j >= 1; --j) {
      if (j < n) {
        cursor.Check(jc + 1);
        PackedProduct(cursor, n - j, false);
        ScalarLoops(cursor, n - j, true);
        ScalarLoops(cursor, n - j, false);
      }
      jc = cursor.Check(cursor.Check(cursor.Check(jc - n) + j) - 2);
    }
  }
}

bool InverseOracle(extent_t n, bool upper, extent_t limit) {
  if (n == 0) {
    return true;
  }
  Cursor cursor(limit);
  cursor.Check(n);
  TriangularInverse(cursor, n, upper);
  extent_t jj = upper ? 0 : 1;
  for (extent_t j = 1; j <= n; ++j) {
    if (upper) {
      cursor.Check(jj + 1);
      jj = cursor.Check(jj + j);
      PackedProduct(cursor, j - 1, true);  // SPR/HPR packed update cursors.
      ScalarLoops(cursor, j, true);
      ScalarLoops(cursor, j, false);
    } else {
      const auto jjn = cursor.Check(cursor.Check(cursor.Check(jj + n) - j) + 1);
      ScalarLoops(cursor, n - j + 1, true);
      ScalarLoops(cursor, n - j + 1, false);
      if (j < n) {
        cursor.Check(jj + 1);
        PackedProduct(cursor, n - j, false);
      }
      jj = jjn;
    }
  }
  cursor.Check(n + 1);
  return cursor.valid();
}

struct Checks {
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      if (failures < 10) {
        std::fprintf(stderr, "Packed Cholesky inverse count failure: %zu\n",
                     checks);
      }
      ++failures;
    }
  }
};

void SmallRanges(Checks& checks) {
  for (extent_t limit : {1, 3, 7, 15, 31, 63, 127, 255, 511, 1023}) {
    for (extent_t n = 0; n <= 128; ++n) {
      for (auto triangle : {kUpper, kLower}) {
        const auto status = counts::Inverse(n, triangle, limit);
        checks.Expect(status.ok() ==
                      InverseOracle(n, triangle == kUpper, limit));
        if (!status.ok()) {
          checks.Expect(status.code() == asc::ErrorCode::kOverflow);
        }
      }
    }
  }
}

void Endpoints(Checks& checks) {
  for (const auto& endpoints :
       {std::array<extent_t, 3>{2147483647, 65535, 46340},
        std::array<extent_t, 3>{std::numeric_limits<extent_t>::max(),
                                4294967295, 3037000499}}) {
    const auto [limit, upper, lower] = endpoints;
    checks.Expect(counts::Inverse(upper, kUpper, limit).ok());
    checks.Expect(counts::Inverse(upper + 1, kUpper, limit).code() ==
                  asc::ErrorCode::kOverflow);
    checks.Expect(counts::Inverse(lower, kLower, limit).ok());
    checks.Expect(counts::Inverse(lower + 1, kLower, limit).code() ==
                  asc::ErrorCode::kOverflow);
  }
  checks.Expect(counts::Inverse(-1, kUpper, 127).code() ==
                asc::ErrorCode::kInvalidArgument);
  checks.Expect(counts::Inverse(0, kUpper, 0).code() ==
                asc::ErrorCode::kInvalidArgument);
  // This fixed uint8_t enum can represent unnamed value9. Deliberately
  // exercise recoverable validation, as in Core's memory_execution_test.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_triangle = static_cast<asc::DenseBlasTriangle>(9);
  checks.Expect(counts::Inverse(0, invalid_triangle, 127).code() ==
                asc::ErrorCode::kInvalidArgument);
}
}  // namespace

int main() {
  Checks checks;
  SmallRanges(checks);
  Endpoints(checks);
  std::printf(
      "Packed Cholesky inverse source counts: %zu checks, %zu failures\n",
      checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
