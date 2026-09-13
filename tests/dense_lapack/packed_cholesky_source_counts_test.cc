#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_cholesky_counts.h"

namespace {
using asc::extent_t;
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

struct Checks {
  std::size_t count = 0;
  std::size_t failures = 0;
  void Expect(bool value) {
    ++count;
    if (!value) {
      ++failures;
    }
  }
};

void VirtualMachines(Checks& checks) {
  for (const extent_t limit : {3, 7, 15, 31, 63, 127, 255, 1023}) {
    for (extent_t n = 0; n <= limit + 1; ++n) {
      for (bool upper : {false, true}) {
        for (bool complex : {false, true}) {
          const auto admitted = asc::internal_packed_cholesky_counts::Factor(
              n, upper ? kUpper : kLower, limit);
          checks.Expect(admitted.ok() ==
                        LiteralFactor(n, upper, complex, limit));
          checks.Expect(admitted.ok() ||
                        admitted.code() == asc::ErrorCode::kOverflow);
        }
      }
    }
  }
}

void ActualBoundaries(Checks& checks) {
  // Exact integer endpoints from the two packed inequalities. These are
  // metadata-only calls, without fake views, backing or gigantic allocation.
  for (const auto limit : {extent_t{std::numeric_limits<std::int32_t>::max()},
                           std::numeric_limits<extent_t>::max()}) {
    const extent_t upper =
        limit == std::numeric_limits<std::int32_t>::max() ? 65535 : 4294967295;
    for (auto triangle : {kUpper, kLower}) {
      const auto endpoint = triangle == kUpper ? upper : upper - 1;
      for (const extent_t offset : {-1, 0, 1}) {
        const auto status = asc::internal_packed_cholesky_counts::Factor(
            endpoint + offset, triangle, limit);
        checks.Expect(status.ok() == (offset <= 0));
      }
      checks.Expect(
          asc::internal_packed_cholesky_counts::Factor(limit, triangle, limit)
              .code() == asc::ErrorCode::kOverflow);
      checks.Expect(
          asc::internal_packed_cholesky_counts::Factor(-1, triangle, limit)
              .code() == asc::ErrorCode::kInvalidArgument);
    }
  }
}
}  // namespace

int main() {
  Checks checks;
  VirtualMachines(checks);
  ActualBoundaries(checks);
  std::printf(
      "Packed Cholesky factor source counts: %zu checks, %zu failures\n",
      checks.count, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
