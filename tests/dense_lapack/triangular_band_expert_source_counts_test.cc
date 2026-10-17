#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_triangular_band_expert_counts.h"

namespace {
namespace counts = asc::internal_triangular_band_expert_counts;
using asc::extent_t;
using Code = asc::ErrorCode;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;

class Checks {
 public:
  void Equal(Code actual, Code expected) {
    ++checks_;
    if (actual != expected) {
      if (failures_ < 12) {
        std::fprintf(stderr, "Band expert integer code %d, expected %d\n",
                     static_cast<int>(actual), static_cast<int>(expected));
      }
      ++failures_;
    }
  }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Triangular band expert source counts: %zu checks, %zu failures\n",
        checks_, failures_);
    return failures_ == 0 ? 0 : 1;
  }

 private:
  std::size_t checks_ = 0;
  std::size_t failures_ = 0;
};

// Literal selected Fortran expressions and DO controls in a virtual signed
// machine. These small loops use host extent_t; no near-limit array or pointer
// is fabricated. The tested helper uses closed bounds, not these traversals.
class Machine {
 public:
  explicit Machine(extent_t limit) : limit_(limit) {}
  void Native(extent_t value) { ok_ = ok_ && value >= 0 && value <= limit_; }
  extent_t Add(extent_t a, extent_t b) {
    const extent_t value = a + b;  // Virtual tests have operands in [-66,66].
    ok_ = ok_ && value >= -limit_ - 1 && value <= limit_;
    return value;
  }
  extent_t Sub(extent_t a, extent_t b) { return Add(a, -b); }
  extent_t Mul(extent_t a, extent_t b) {
    const extent_t value = a * b;
    ok_ = ok_ && value >= -limit_ - 1 && value <= limit_;
    return value;
  }
  template <typename Function>
  void Walk(extent_t first, extent_t last, Function body) {
    if (!ok_) {
      return;
    }
    for (extent_t i = first; i <= last && ok_; ++i) {
      body(i);
      static_cast<void>(Add(i, 1));
    }
  }
  [[nodiscard]] bool ok() const { return ok_; }
  [[nodiscard]] Code Result() const {
    return ok_ ? Code::kOk : Code::kOverflow;
  }

 private:
  extent_t limit_;
  bool ok_ = true;
};

void Vectors(Machine& machine, extent_t n) {
  // S/D/C/Z LACN2's terminal estimate evaluates 3*N; real WORK reaches
  // 2*N+I. All shared unit-stride vector loops and their terminals fit here.
  static_cast<void>(machine.Mul(3, n));
  static_cast<void>(machine.Add(machine.Mul(2, n), 1));
  machine.Walk(1, n, [&](extent_t i) {
    static_cast<void>(machine.Add(machine.Mul(2, n), i));
  });
}

void Norm(Machine& machine, extent_t n, extent_t kd, bool upper, bool unit,
          bool one_norm) {
  machine.Walk(1, n, [&](extent_t j) {
    if (one_norm) {
      extent_t first = 0;
      extent_t last = 0;
      if (upper) {
        first = std::max<extent_t>(1, machine.Sub(machine.Add(kd, 2), j));
        last = unit ? kd : machine.Add(kd, 1);
      } else {
        first = unit ? 2 : 1;
        last = std::min(machine.Sub(machine.Add(n, 1), j), machine.Add(kd, 1));
      }
      machine.Walk(first, last, [](extent_t) {});
    } else {
      extent_t offset = 0;
      extent_t first = 0;
      extent_t last = 0;
      if (upper) {
        offset = machine.Sub(machine.Add(kd, 1), j);
        first = std::max<extent_t>(1, machine.Sub(j, kd));
        last = unit ? machine.Sub(j, 1) : j;
      } else {
        offset = machine.Sub(1, j);
        first = unit ? machine.Add(j, 1) : j;
        last = std::min(n, machine.Add(j, kd));
      }
      machine.Walk(first, last, [&](extent_t i) {
        static_cast<void>(machine.Add(offset, i));
      });
    }
  });
}

void ScaledSolve(Machine& machine, extent_t n, extent_t kd, bool upper) {
  machine.Walk(1, n, [&](extent_t j) {
    const extent_t length =
        std::min(kd, upper ? machine.Sub(j, 1) : machine.Sub(n, j));
    if (upper) {
      static_cast<void>(machine.Sub(machine.Add(kd, 1), length));
      machine.Walk(1, length, [&](extent_t i) {
        static_cast<void>(machine.Sub(machine.Add(kd, i), length));
        static_cast<void>(
            machine.Add(machine.Sub(machine.Sub(j, length), 1), i));
      });
    } else {
      machine.Walk(1, length, [&](extent_t i) {
        static_cast<void>(machine.Add(i, 1));
        static_cast<void>(machine.Add(j, i));
      });
      // Either estimator orientation can select TBSV's lower J+K branch.
      static_cast<void>(machine.Add(j, kd));
    }
  });
}

Code ConditionOracle(extent_t n, extent_t kd, extent_t ldab, bool upper,
                     bool unit, bool one_norm, extent_t limit) {
  if (n < 0 || kd < 0 || ldab <= kd || limit < 3) {
    return Code::kInvalidArgument;
  }
  if (n == 0) {
    return Code::kOk;
  }
  Machine machine(limit);
  for (const auto value : {n, kd, ldab}) {
    machine.Native(value);
  }
  Vectors(machine, n);
  Norm(machine, n, kd, upper, unit, one_norm);
  ScaledSolve(machine, n, kd, upper);
  return machine.Result();
}

Code ErrorOracle(extent_t n, extent_t kd, extent_t nrhs, extent_t ldab,
                 extent_t ldb, extent_t ldx, bool upper, bool unit,
                 extent_t limit) {
  if (n < 0 || kd < 0 || nrhs < 0 || ldab <= kd || ldb < 1 || ldx < 1 ||
      limit < 3 || (n != 0 && nrhs != 0 && (ldb < n || ldx < n))) {
    return Code::kInvalidArgument;
  }
  if (n == 0 || nrhs == 0) {
    return Code::kOk;
  }
  Machine machine(limit);
  for (const auto value : {n, kd, nrhs, ldab, ldb, ldx}) {
    machine.Native(value);
  }
  Vectors(machine, n);
  static_cast<void>(machine.Add(kd, 2));  // NZ, even for a scalar matrix.
  machine.Walk(1, nrhs, [](extent_t) {});
  machine.Walk(1, n, [&](extent_t k) {
    extent_t first = 0;
    extent_t last = 0;
    if (upper) {
      first = std::max<extent_t>(1, machine.Sub(k, kd));
      last = unit ? machine.Sub(k, 1) : k;
    } else {
      first = unit ? machine.Add(k, 1) : k;
      last = std::min(n, machine.Add(k, kd));
    }
    machine.Walk(first, last, [&](extent_t i) {
      if (upper) {
        static_cast<void>(machine.Sub(machine.Add(machine.Add(kd, 1), i), k));
      } else {
        static_cast<void>(machine.Sub(machine.Add(1, i), k));
      }
    });
    // TBMV/TBSV use these same selected bounds in either orientation.
    if (!upper) {
      static_cast<void>(machine.Add(k, kd));
    }
  });
  return machine.Result();
}

void ExhaustiveCondition(Checks& checks) {
  for (extent_t limit : {3, 7, 15, 31}) {
    for (extent_t n = 0; n <= limit + 1; ++n) {
      for (extent_t kd = 0; kd <= limit + 1; ++kd) {
        for (extent_t ldab : {kd + 1, limit + 1}) {
          for (auto triangle : {kUpper, kLower}) {
            for (bool unit : {false, true}) {
              for (bool one_norm : {false, true}) {
                checks.Equal(
                    counts::Condition(n, kd, ldab, triangle, one_norm, limit)
                        .code(),
                    ConditionOracle(n, kd, ldab, triangle == kUpper, unit,
                                    one_norm, limit));
              }
            }
          }
        }
      }
    }
  }
}

void ExhaustiveErrors(Checks& checks) {
  for (extent_t limit : {3, 7, 15, 31}) {
    for (extent_t n = 0; n <= limit + 1; ++n) {
      for (extent_t kd = 0; kd <= limit + 1; ++kd) {
        for (extent_t nrhs : {extent_t{0}, extent_t{1}, limit - 1, limit}) {
          for (extent_t leading : {std::max<extent_t>(1, n), limit + 1}) {
            for (auto triangle : {kUpper, kLower}) {
              for (auto diagonal : {kUnit, kNonUnit}) {
                checks.Equal(
                    counts::Errors(n, kd, nrhs, kd + 1, leading, leading,
                                   triangle, diagonal, limit)
                        .code(),
                    ErrorOracle(n, kd, nrhs, kd + 1, leading, leading,
                                triangle == kUpper, diagonal == kUnit, limit));
              }
            }
          }
        }
      }
    }
  }
}

void RealIntegerLimits(Checks& checks, extent_t limit) {
  const auto condition = [&](extent_t n, extent_t kd, auto triangle,
                             bool one_norm) {
    return counts::Condition(n, kd, kd + 1, triangle, one_norm, limit).code();
  };
  const auto errors = [&](extent_t n, extent_t kd, auto triangle,
                          auto diagonal) {
    return counts::Errors(n, kd, 1, kd + 1, n, n, triangle, diagonal, limit)
        .code();
  };
  checks.Equal(condition(limit / 3, 0, kUpper, true), Code::kOk);
  checks.Equal(condition(limit / 3 + 1, 0, kUpper, true), Code::kOverflow);
  checks.Equal(condition(1, limit - 1, kUpper, false), Code::kOk);
  checks.Equal(condition(1, limit - 1, kUpper, true), Code::kOverflow);
  checks.Equal(condition(3, limit - 2, kUpper, false), Code::kOk);
  checks.Equal(condition(3, limit - 1, kUpper, false), Code::kOverflow);
  checks.Equal(condition(3, limit - 3, kLower, true), Code::kOk);
  checks.Equal(condition(3, limit - 2, kLower, true), Code::kOverflow);
  checks.Equal(errors(3, limit - 4, kUpper, kNonUnit), Code::kOk);
  checks.Equal(errors(3, limit - 3, kUpper, kNonUnit), Code::kOverflow);
  checks.Equal(errors(3, limit - 3, kUpper, kUnit), Code::kOk);
  checks.Equal(errors(3, limit - 2, kUpper, kUnit), Code::kOverflow);
  checks.Equal(errors(3, limit - 3, kLower, kUnit), Code::kOk);
  checks.Equal(errors(3, limit - 2, kLower, kUnit), Code::kOverflow);
  checks.Equal(errors(1, limit - 2, kUpper, kUnit), Code::kOk);
  checks.Equal(errors(1, limit - 1, kUpper, kUnit), Code::kOverflow);
  checks.Equal(
      counts::Errors(limit, 0, 0, 1, 1, 1, kUpper, kUnit, limit).code(),
      Code::kOk);
  checks.Equal(
      counts::Condition(0, limit - 1, limit, kUpper, true, limit).code(),
      Code::kOk);
  checks.Equal(
      counts::Errors(0, limit - 1, limit, limit, 1, 1, kUpper, kUnit, limit)
          .code(),
      Code::kOk);
}

void InvalidMetadata(Checks& checks) {
  checks.Equal(counts::Condition(-1, 0, 1, kUpper, true, 31).code(),
               Code::kInvalidArgument);
  checks.Equal(counts::Condition(1, -1, 1, kUpper, true, 31).code(),
               Code::kInvalidArgument);
  checks.Equal(counts::Condition(1, 1, 1, kUpper, true, 31).code(),
               Code::kInvalidArgument);
  checks.Equal(counts::Condition(0, 0, 1, kUpper, true, 2).code(),
               Code::kInvalidArgument);
  for (const auto& values : {std::array<extent_t, 6>{-1, 0, 1, 1, 1, 1},
                             {1, -1, 1, 1, 1, 1},
                             {1, 0, -1, 1, 1, 1},
                             {1, 1, 1, 1, 1, 1},
                             {1, 0, 1, 1, 0, 1},
                             {1, 0, 1, 1, 1, 0},
                             {2, 0, 1, 1, 1, 2},
                             {2, 0, 1, 1, 2, 1}}) {
    checks.Equal(counts::Errors(values[0], values[1], values[2], values[3],
                                values[4], values[5], kUpper, kUnit, 31)
                     .code(),
                 Code::kInvalidArgument);
  }
}
}  // namespace

int main() {
  Checks checks;
  ExhaustiveCondition(checks);
  ExhaustiveErrors(checks);
  RealIntegerLimits(checks, std::numeric_limits<std::int32_t>::max());
  RealIntegerLimits(checks, std::numeric_limits<std::int64_t>::max());
  InvalidMetadata(checks);
  return checks.Finish();
}
