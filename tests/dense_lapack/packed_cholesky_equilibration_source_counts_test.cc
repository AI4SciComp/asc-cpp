#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_cholesky_equilibration_counts.h"

namespace {
using asc::extent_t;
using asc::internal_packed_cholesky_equilibration_counts::Equilibrate;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;

struct Checks {
  std::size_t count = 0;
  std::size_t failures = 0;
  void Expect(bool condition) {
    ++count;
    if (!condition) {
      ++failures;
    }
  }
};

struct IntegerMachine {
  extent_t limit;
  bool valid = true;
  extent_t Check(extent_t value) {
    valid = valid && value >= -limit - 1 && value <= limit;
    return value;
  }
  extent_t Add(extent_t a, extent_t b) { return Check(a + b); }
  extent_t Subtract(extent_t a, extent_t b) { return Check(a - b); }
};

// Literal S/D/C/Z PPEQU cursor statements; scalar arithmetic does not change
// control progress. Virtual limits ensure all host arithmetic itself fits.
bool Literal(extent_t n, bool upper, extent_t limit) {
  IntegerMachine m{limit};
  m.Check(n);
  if (n == 0 || !m.valid) {
    return m.valid;
  }
  extent_t jj = 1;
  for (extent_t i = 2; i <= n && m.valid; i = m.Add(i, 1)) {
    if (upper) {
      jj = m.Add(jj, i);
    } else {
      jj = m.Add(m.Subtract(m.Add(jj, n), i), 2);
    }
    m.Check(jj);
  }
  // Both the first-nonpositive search and successful scale loop use 1..N.
  // An early numerical return is a prefix of this complete finite loop.
  for (extent_t i = 1; i <= n && m.valid; i = m.Add(i, 1)) {
    m.Check(i);
  }
  return m.valid;
}

void Virtual(Checks& checks) {
  for (const extent_t limit : {3, 7, 15, 31, 63, 127, 255, 1023}) {
    for (extent_t n = 0; n <= limit + 1; ++n) {
      for (const auto triangle : {kUpper, kLower}) {
        for (const auto layout : {kColumn, kRow}) {
          const bool upper = (triangle == kUpper) == (layout == kColumn);
          const auto status = Equilibrate(n, triangle, layout, limit);
          checks.Expect(status.ok() == Literal(n, upper, limit));
          checks.Expect(status.ok() ||
                        status.code() == asc::ErrorCode::kOverflow);
        }
      }
    }
  }
}

std::array<std::size_t, 64> Diagonals(extent_t n,
                                      asc::DenseBlasTriangle triangle,
                                      asc::DenseBlasLayout layout) {
  std::array<std::size_t, 64> result{};
  std::size_t slot = 0;
  for (extent_t major = 0; major < n; ++major) {
    for (extent_t minor = 0; minor < n; ++minor) {
      const auto i = layout == kColumn ? minor : major;
      const auto j = layout == kColumn ? major : minor;
      if (triangle == kUpper ? i <= j : i >= j) {
        if (i == j) {
          result[static_cast<std::size_t>(i)] = slot;
        }
        ++slot;
      }
    }
  }
  return result;
}

void PhysicalEquivalence(Checks& checks) {
  for (extent_t n = 0; n <= 64; ++n) {
    const auto upper_row = Diagonals(n, kUpper, kRow);
    const auto lower_column = Diagonals(n, kLower, kColumn);
    const auto lower_row = Diagonals(n, kLower, kRow);
    const auto upper_column = Diagonals(n, kUpper, kColumn);
    checks.Expect(upper_row == lower_column);
    checks.Expect(lower_row == upper_column);
  }
}

void Boundaries(Checks& checks) {
  for (const auto limit : {extent_t{std::numeric_limits<std::int32_t>::max()},
                           std::numeric_limits<extent_t>::max()}) {
    const extent_t upper_limit =
        limit == std::numeric_limits<std::int32_t>::max() ? 65535 : 4294967295;
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kColumn, kRow}) {
        const bool upper = (triangle == kUpper) == (layout == kColumn);
        const auto endpoint = upper ? upper_limit : upper_limit - 1;
        for (const extent_t offset : {-1, 0, 1}) {
          checks.Expect(
              Equilibrate(endpoint + offset, triangle, layout, limit).ok() ==
              (offset <= 0));
        }
        checks.Expect(Equilibrate(-1, triangle, layout, limit).code() ==
                      asc::ErrorCode::kInvalidArgument);
        checks.Expect(Equilibrate(limit, triangle, layout, limit).code() ==
                      asc::ErrorCode::kOverflow);
        checks.Expect(Equilibrate(0, triangle, layout, limit).ok());
      }
    }
  }
  // Deliberate unnamed fixed-width flags follow Core's scoped test exception.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  constexpr auto kInvalidTriangle = static_cast<asc::DenseBlasTriangle>(9);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  constexpr auto kInvalidLayout = static_cast<asc::DenseBlasLayout>(9);
  checks.Expect(Equilibrate(0, kInvalidTriangle, kColumn, 7).code() ==
                asc::ErrorCode::kInvalidArgument);
  checks.Expect(Equilibrate(0, kUpper, kInvalidLayout, 7).code() ==
                asc::ErrorCode::kInvalidArgument);
  checks.Expect(Equilibrate(0, kUpper, kColumn, 2).code() ==
                asc::ErrorCode::kInvalidArgument);
}
}  // namespace

int main() {
  Checks checks;
  Virtual(checks);
  PhysicalEquivalence(checks);
  Boundaries(checks);
  std::printf(
      "Packed Cholesky equilibration source counts: %zu checks, %zu failures\n",
      checks.count, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
