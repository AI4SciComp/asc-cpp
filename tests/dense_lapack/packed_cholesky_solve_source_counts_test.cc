#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_packed_cholesky_solve_counts.h"

namespace {
using asc::extent_t;
using asc::internal_packed_cholesky_solve_counts::Solve;

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
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool condition) {
    ++checks;
    if (!condition) {
      ++failures;
    }
  }
};

void Virtual(Checks& checks) {
  for (const extent_t limit : {1, 3, 7, 15, 31, 63, 127, 255, 1023}) {
    for (extent_t n = 0; n <= limit + 1; ++n) {
      for (const auto nrhs : {extent_t{0}, extent_t{1}, extent_t{2}, limit - 1,
                              limit, limit + 1}) {
        for (const bool upper : {false, true}) {
          const auto status = Solve(n, nrhs, std::max<extent_t>(1, n), limit);
          checks.Expect(status.ok() == LiteralSolve(n, nrhs, upper, limit));
          checks.Expect(status.ok() ||
                        status.code() == asc::ErrorCode::kOverflow);
        }
      }
    }
  }
}

void Boundaries(Checks& checks) {
  for (const extent_t limit :
       {extent_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<extent_t>::max()}) {
    const extent_t endpoint =
        limit == std::numeric_limits<std::int32_t>::max() ? 46340 : 3037000499;
    for (const extent_t offset : {-1, 0, 1}) {
      checks.Expect(
          Solve(endpoint + offset, 1, endpoint + offset, limit).ok() ==
          (offset <= 0));
    }
    checks.Expect(Solve(1, limit - 1, 1, limit).ok());
    checks.Expect(Solve(1, limit, 1, limit).code() ==
                  asc::ErrorCode::kOverflow);
    checks.Expect(Solve(0, limit, 1, limit).ok());
    checks.Expect(Solve(limit, 0, limit, limit).ok());
    checks.Expect(Solve(1, 1, limit, limit).ok());
    checks.Expect(Solve(-1, 0, 1, limit).code() ==
                  asc::ErrorCode::kInvalidArgument);
    checks.Expect(Solve(0, -1, 1, limit).code() ==
                  asc::ErrorCode::kInvalidArgument);
    checks.Expect(Solve(0, 0, 0, limit).code() ==
                  asc::ErrorCode::kInvalidArgument);
    checks.Expect(Solve(2, 0, 1, limit).code() ==
                  asc::ErrorCode::kInvalidArgument);
  }
  checks.Expect(Solve(0, 0, 1, 0).code() == asc::ErrorCode::kInvalidArgument);
  checks.Expect(Solve(1, 1, 8, 7).code() == asc::ErrorCode::kOverflow);
}
}  // namespace

int main() {
  Checks checks;
  Virtual(checks);
  Boundaries(checks);
  std::printf("Packed Cholesky solve source counts: %zu checks, %zu failures\n",
              checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
