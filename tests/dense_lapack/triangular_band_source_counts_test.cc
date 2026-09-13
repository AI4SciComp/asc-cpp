#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_triangular_band_counts.h"

namespace {
namespace counts = asc::internal_triangular_band_counts;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kTranspose = asc::DenseBlasTranspose::kTranspose;

struct Checks {
  std::size_t total = 0;
  std::size_t failed = 0;
  void Expect(bool value) {
    ++total;
    if (!value) {
      ++failed;
    }
  }
};

struct Machine {
  int limit;
  bool valid = true;
  void Use(int value) {
    valid = valid && value >= -limit - 1 && value <= limit;
  }
};

void UpperColumn(Machine& machine, int j, int kd, bool transpose) {
  machine.Use(kd + 1);
  const int offset = kd + 1 - j;
  machine.Use(offset);
  machine.Use(j - kd);
  machine.Use(j - 1);
  if (transpose) {
    int i = std::max(1, j - kd);
    machine.Use(i);
    for (; i <= j - 1; ++i) {
      machine.Use(offset + i);
    }
    machine.Use(i);
  } else {
    int i = j - 1;
    machine.Use(i);
    for (; i >= std::max(1, j - kd); --i) {
      machine.Use(offset + i);
    }
    machine.Use(i);
  }
}

void LowerColumn(Machine& machine, int n, int j, int kd, bool transpose) {
  const int offset = 1 - j;
  machine.Use(offset);
  machine.Use(j + kd);
  machine.Use(j + 1);
  if (transpose) {
    int i = std::min(n, j + kd);
    machine.Use(i);
    for (; i >= j + 1; --i) {
      machine.Use(offset + i);
    }
    machine.Use(i);
  } else {
    int i = j + 1;
    machine.Use(i);
    for (; i <= std::min(n, j + kd); ++i) {
      machine.Use(offset + i);
    }
    machine.Use(i);
  }
}

bool SourceWalk(int n, int kd, int nrhs, int limit,
                asc::DenseBlasTriangle triangle,
                asc::DenseBlasDiagonal diagonal,
                asc::DenseBlasTranspose operation) {
  if (n == 0) {
    return true;
  }
  Machine machine{limit};
  for (int value : {n, kd, nrhs, kd + 1, std::max(1, n), 1}) {
    machine.Use(value);
  }
  if (diagonal == kNonUnit) {
    int info = 1;
    for (; info <= n; ++info) {
      machine.Use(info);
      machine.Use(triangle == kUpper ? kd + 1 : 1);
    }
    machine.Use(info);
  }
  int rhs = 1;
  for (; rhs <= nrhs; ++rhs) {
    machine.Use(rhs);
    const bool transpose = operation != kNone;
    const bool ascending = (triangle == kUpper) == transpose;
    int j = ascending ? 1 : n;
    for (; ascending ? j <= n : j >= 1; j += ascending ? 1 : -1) {
      machine.Use(j);
      if (triangle == kUpper) {
        UpperColumn(machine, j, kd, transpose);
      } else {
        LowerColumn(machine, n, j, kd, transpose);
      }
    }
    machine.Use(j);
  }
  machine.Use(rhs);
  return machine.valid;
}

void Virtual(Checks& checks) {
  for (const int limit : {3, 7, 31}) {
    for (int n = 0; n <= 35; ++n) {
      for (int kd = 0; kd <= 35; ++kd) {
        for (const int nrhs : {0, 1, limit - 1, limit}) {
          for (const auto triangle : {kUpper, kLower}) {
            for (const auto diagonal : {kUnit, kNonUnit}) {
              for (const auto op :
                   {kNone, kTranspose,
                    asc::DenseBlasTranspose::kConjugateTranspose}) {
                const bool actual =
                    counts::Solve(n, kd, nrhs, kd + 1, std::max(1, n), triangle,
                                  diagonal, op, limit)
                        .ok();
                checks.Expect(actual == SourceWalk(n, kd, nrhs, limit, triangle,
                                                   diagonal, op));
              }
            }
          }
        }
      }
    }
  }
}

void NativeBoundaries(Checks& checks) {
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    // Abstract integer calls only; no forged huge backing span or execution.
    checks.Expect(
        counts::Solve(limit, 0, 1, 1, limit, kUpper, kUnit, kNone, limit).ok());
    checks.Expect(
        !counts::Solve(limit, 0, 1, 1, limit, kUpper, kUnit, kTranspose, limit)
             .ok());
    checks.Expect(
        !counts::Solve(limit, 0, 0, 1, limit, kUpper, kNonUnit, kNone, limit)
             .ok());
    checks.Expect(
        counts::Solve(limit, 0, 0, 1, limit, kLower, kUnit, kNone, limit).ok());
    checks.Expect(counts::Solve(2, limit - 2, 1, limit - 1, 2, kLower, kUnit,
                                kNone, limit)
                      .ok());
    checks.Expect(
        !counts::Solve(2, limit - 1, 1, limit, 2, kLower, kUnit, kNone, limit)
             .ok());
    checks.Expect(
        counts::Solve(2, limit - 1, 1, limit, 2, kUpper, kUnit, kNone, limit)
            .ok());
    checks.Expect(counts::Solve(0, limit - 1, limit, limit, 1, kLower, kUnit,
                                kNone, limit)
                      .ok());
  }
  checks.Expect(
      counts::Solve(-1, 0, 1, 1, 1, kUpper, kUnit, kNone, 31).code() ==
      asc::ErrorCode::kInvalidArgument);
  checks.Expect(counts::Solve(2, 3, 1, 3, 2, kUpper, kUnit, kNone, 31).code() ==
                asc::ErrorCode::kInvalidArgument);
  checks.Expect(counts::Solve(2, 0, 1, 1, 1, kUpper, kUnit, kNone, 31).code() ==
                asc::ErrorCode::kInvalidArgument);
}
}  // namespace

int main() {
  Checks checks;
  Virtual(checks);
  NativeBoundaries(checks);
  std::printf("Triangular band source counts: %zu checks, %zu failures\n",
              checks.total, checks.failed);
  return checks.failed == 0 ? 0 : 1;
}
