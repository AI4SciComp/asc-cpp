#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_cholesky_driver_counts.h"

namespace {
using asc::extent_t;
using asc::internal_packed_cholesky_driver_counts::Driver;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;

struct IntegerMachine {
  extent_t limit;
  bool valid = true;
  extent_t Check(extent_t value) {
    valid = valid && value >= -limit - 1 && value <= limit;
    return value;
  }
  extent_t Add(extent_t a, extent_t b) { return Check(a + b); }
  extent_t Subtract(extent_t a, extent_t b) { return Check(a - b); }
  extent_t Multiply(extent_t a, extent_t b) { return Check(a * b); }
};

// Literal selected BLAS loop controls, including the value on loop exit.
// Virtual machines below are small; these host calculations cannot overflow.
void ScalarLoop(IntegerMachine& machine, extent_t n, bool complex) {
  if (n <= 0) {
    return;
  }
  if (complex) {
    for (extent_t i = 1; i <= n && machine.valid; i = machine.Add(i, 1)) {
      machine.Check(i);
    }
    return;
  }
  const auto remainder = n % 5;
  if (remainder != 0) {
    for (extent_t i = 1; i <= remainder && machine.valid;
         i = machine.Add(i, 1)) {
      machine.Check(i);
    }
    if (n < 5) {
      return;
    }
  }
  const auto first = machine.Add(remainder, 1);
  for (extent_t i = first; i <= n && machine.valid; i = machine.Add(i, 5)) {
    for (extent_t offset = 1; offset <= 4; ++offset) {
      machine.Add(i, offset);
    }
  }
}

void UpperTransposeSolve(IntegerMachine& machine, extent_t n) {
  if (n == 0) {
    return;
  }
  extent_t kk = 1;
  for (extent_t j = 1; j <= n && machine.valid; j = machine.Add(j, 1)) {
    auto k = kk;
    const auto end = machine.Subtract(j, 1);
    for (extent_t i = 1; i <= end && machine.valid; i = machine.Add(i, 1)) {
      machine.Check(k);
      k = machine.Add(k, 1);
    }
    machine.Subtract(machine.Add(kk, j), 1);
    kk = machine.Add(kk, j);
  }
}

void LowerRankUpdate(IntegerMachine& machine, extent_t n, bool complex) {
  extent_t kk = 1;
  for (extent_t j = 1; j <= n && machine.valid; j = machine.Add(j, 1)) {
    auto k = complex ? machine.Add(kk, 1) : kk;
    const auto first = complex ? machine.Add(j, 1) : j;
    for (extent_t i = first; i <= n && machine.valid; i = machine.Add(i, 1)) {
      machine.Check(k);
      k = machine.Add(k, 1);
    }
    kk = machine.Add(machine.Subtract(machine.Add(kk, n), j), 1);
  }
}

bool LiteralFactor(extent_t n, bool upper, bool complex, extent_t limit) {
  IntegerMachine machine{limit};
  machine.Check(n);
  if (n == 0 || !machine.valid) {
    return machine.valid;
  }
  extent_t jj = upper ? 0 : 1;
  for (extent_t j = 1; j <= n && machine.valid; j = machine.Add(j, 1)) {
    if (upper) {
      machine.Add(jj, 1);  // JC = JJ+1.
      jj = machine.Add(jj, j);
      const auto previous = machine.Subtract(j, 1);
      if (j > 1) {
        UpperTransposeSolve(machine, previous);
      }
      ScalarLoop(machine, previous, complex);  // DOT/DOTC.
    } else if (j < n) {
      const auto tail = machine.Subtract(n, j);
      machine.Add(jj, 1);
      ScalarLoop(machine, tail, complex);  // SCAL or real-complex SCAL.
      machine.Add(machine.Subtract(machine.Add(jj, n), j), 1);
      LowerRankUpdate(machine, tail, complex);
      jj = machine.Add(machine.Subtract(machine.Add(jj, n), j), 1);
    }
  }
  return machine.valid;
}

// Literal selected INCX=1 paths from pinned S/D/C/Z TPSV. Virtual limits are
// small, so host arithmetic does not overflow while detecting native overflow.
void UpperNone(IntegerMachine& m, extent_t n) {
  auto kk = m.Multiply(n, m.Add(n, 1)) / 2;
  for (auto j = n; j >= 1 && m.valid; j = m.Subtract(j, 1)) {
    m.Check(kk);
    auto k = m.Subtract(kk, 1);
    for (auto i = m.Subtract(j, 1); i >= 1 && m.valid; i = m.Subtract(i, 1)) {
      m.Check(k);
      k = m.Subtract(k, 1);
    }
    kk = m.Subtract(kk, j);
  }
}

void UpperTranspose(IntegerMachine& m, extent_t n) {
  extent_t kk = 1;
  for (extent_t j = 1; j <= n && m.valid; j = m.Add(j, 1)) {
    auto k = kk;
    const auto last = m.Subtract(j, 1);
    for (extent_t i = 1; i <= last && m.valid; i = m.Add(i, 1)) {
      m.Check(k);
      k = m.Add(k, 1);
    }
    m.Subtract(m.Add(kk, j), 1);
    kk = m.Add(kk, j);
  }
}

void LowerNone(IntegerMachine& m, extent_t n) {
  extent_t kk = 1;
  for (extent_t j = 1; j <= n && m.valid; j = m.Add(j, 1)) {
    m.Check(kk);
    auto k = m.Add(kk, 1);
    for (auto i = m.Add(j, 1); i <= n && m.valid; i = m.Add(i, 1)) {
      m.Check(k);
      k = m.Add(k, 1);
    }
    kk = m.Add(kk, m.Add(m.Subtract(n, j), 1));
  }
}

void LowerTranspose(IntegerMachine& m, extent_t n) {
  auto kk = m.Multiply(n, m.Add(n, 1)) / 2;
  for (auto j = n; j >= 1 && m.valid; j = m.Subtract(j, 1)) {
    auto k = kk;
    const auto last = m.Add(j, 1);
    for (auto i = n; i >= last && m.valid; i = m.Subtract(i, 1)) {
      m.Check(k);
      k = m.Subtract(k, 1);
    }
    m.Add(m.Subtract(kk, n), j);
    kk = m.Subtract(kk, m.Add(m.Subtract(n, j), 1));
  }
}

bool LiteralSolve(extent_t n, extent_t nrhs, bool upper, extent_t limit) {
  // ASC's local empty path never narrows these dimensions or enters Fortran.
  if (n == 0 || nrhs == 0) {
    return true;
  }
  IntegerMachine m{limit};
  m.Check(n);
  m.Check(nrhs);
  // Every RHS repeats these same TPSV integer paths. Numeric zero branches
  // may skip inner work; the all-active path bounds that subset as well.
  if (upper) {
    UpperTranspose(m, n);
    UpperNone(m, n);
  } else {
    LowerNone(m, n);
    LowerTranspose(m, n);
  }
  for (extent_t i = 1; i <= nrhs && m.valid; i = m.Add(i, 1)) {
    m.Check(i);
  }
  return m.valid;
}

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

bool LiteralDriver(extent_t n, extent_t nrhs, extent_t ldb, bool upper,
                   bool complex, extent_t limit) {
  if (n == 0) {
    return true;
  }
  IntegerMachine machine{limit};
  machine.Check(n);
  machine.Check(nrhs);
  machine.Check(ldb);
  // PPSV's successful factor path is the worst case. Numerical failure
  // returns after a prefix of the same factor arithmetic and never solves.
  return machine.valid && LiteralFactor(n, upper, complex, limit) &&
         LiteralSolve(n, nrhs, upper, limit);
}

void Virtual(Checks& checks) {
  for (const extent_t limit : {3, 7, 15, 31, 63, 127, 255, 1023}) {
    for (extent_t n = 0; n <= limit + 1; ++n) {
      for (const extent_t nrhs : {extent_t{0}, extent_t{1}, extent_t{2},
                                  limit - 1, limit, limit + 1}) {
        for (const auto ldb :
             {std::max<extent_t>(1, n), std::max<extent_t>(limit + 1, n)}) {
          for (const bool upper : {false, true}) {
            for (const bool complex : {false, true}) {
              const auto status =
                  Driver(n, nrhs, ldb, upper ? kUpper : kLower, limit);
              checks.Expect(status.ok() ==
                            LiteralDriver(n, nrhs, ldb, upper, complex, limit));
              checks.Expect(status.ok() ||
                            status.code() == asc::ErrorCode::kOverflow);
            }
          }
        }
      }
    }
  }
}

void Boundaries(Checks& checks) {
  for (const auto limit : {extent_t{std::numeric_limits<std::int32_t>::max()},
                           std::numeric_limits<extent_t>::max()}) {
    const bool lp64 = limit == std::numeric_limits<std::int32_t>::max();
    const extent_t upper = lp64 ? 65535 : 4294967295;
    const extent_t solve = lp64 ? 46340 : 3037000499;
    for (const auto triangle : {kUpper, kLower}) {
      const auto factor = triangle == kUpper ? upper : upper - 1;
      for (const extent_t offset : {-1, 0, 1}) {
        checks.Expect(
            Driver(factor + offset, 0, factor + offset, triangle, limit).ok() ==
            (offset <= 0));
        checks.Expect(
            Driver(solve + offset, 1, solve + offset, triangle, limit).ok() ==
            (offset <= 0));
      }
      checks.Expect(Driver(0, limit, limit, triangle, limit).ok());
      checks.Expect(Driver(1, limit - 1, limit, triangle, limit).ok());
      checks.Expect(Driver(1, limit, 1, triangle, limit).code() ==
                    asc::ErrorCode::kOverflow);
      checks.Expect(Driver(limit, 0, limit, triangle, limit).code() ==
                    asc::ErrorCode::kOverflow);
      checks.Expect(Driver(-1, 0, 1, triangle, limit).code() ==
                    asc::ErrorCode::kInvalidArgument);
      checks.Expect(Driver(0, -1, 1, triangle, limit).code() ==
                    asc::ErrorCode::kInvalidArgument);
      checks.Expect(Driver(0, 0, 0, triangle, limit).code() ==
                    asc::ErrorCode::kInvalidArgument);
      checks.Expect(Driver(2, 0, 1, triangle, limit).code() ==
                    asc::ErrorCode::kInvalidArgument);
    }
  }
  checks.Expect(Driver(1, 0, 8, kUpper, 7).code() == asc::ErrorCode::kOverflow);
  checks.Expect(Driver(0, 8, 8, kUpper, 7).ok());
  checks.Expect(Driver(0, 0, 1, kUpper, 2).code() ==
                asc::ErrorCode::kInvalidArgument);
  // Unnamed flags are intentional negative inputs of this fixed uint8_t enum.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  constexpr auto kInvalidTriangle = static_cast<asc::DenseBlasTriangle>(9);
  checks.Expect(Driver(0, 0, 1, kInvalidTriangle, 7).code() ==
                asc::ErrorCode::kInvalidArgument);
}
}  // namespace

int main() {
  Checks checks;
  Virtual(checks);
  Boundaries(checks);
  std::printf(
      "Packed Cholesky driver source counts: %zu checks, %zu failures\n",
      checks.count, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
