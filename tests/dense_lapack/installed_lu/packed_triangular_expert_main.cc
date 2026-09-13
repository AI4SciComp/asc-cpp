#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_packed_condition.h"
#include "asc/dense/providers/lapack_triangular_packed_error_bounds.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kUnit = asc::DenseBlasDiagonal::kUnit;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

class Checks {
 public:
  void Expect(bool value, const char* message) {
    ++checks_;
    if (!value) {
      if (failures_ < 30) {
        std::fprintf(stderr, "Packed triangular expert: %s\n", message);
      }
      ++failures_;
    }
  }
  void Workflow() { ++workflows_; }
  [[nodiscard]] int Finish() const {
    std::printf(
        "Packed triangular expert: %zu workflows, %zu checks, %zu failures\n",
        workflows_, checks_, failures_);
    return failures_ == 0 ? 0 : 1;
  }

 private:
  std::size_t workflows_ = 0;
  std::size_t checks_ = 0;
  std::size_t failures_ = 0;
};

template <typename T, std::size_t N>
bool Bytes(const std::array<T, N>& a, const std::array<T, N>& b) {
  const auto x = std::as_bytes(std::span(a));
  const auto y = std::as_bytes(std::span(b));
  return std::equal(x.begin(), x.end(), y.begin(), y.end());
}

template <typename T>
struct Matrix {
  std::array<T, 128> data;
  asc::extent_t rows;
  asc::extent_t columns;
  asc::DenseBlasLayout layout;
  asc::extent_t leading;
  Matrix(asc::extent_t m, asc::extent_t n, asc::DenseBlasLayout selected)
      : rows(m),
        columns(n),
        layout(selected),
        leading((layout == kRow ? n : m) + 3) {
    data.fill(Value<T>(std::numeric_limits<double>::quiet_NaN()));
  }
  T& At(asc::extent_t i, asc::extent_t j) {
    return data[static_cast<std::size_t>(
        1 + (layout == kRow ? i * leading + j : j * leading + i))];
  }
  auto View() {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data() + 1, rows, columns, layout, leading,
        {data.data(), sizeof(data), kHost}));
  }
};

template <typename T>
struct PackedMatrix {
  std::array<T, 128> data;
  std::array<std::size_t, 64> offsets{};
  asc::extent_t order;
  asc::DenseBlasLayout layout;
  PackedMatrix(asc::extent_t n, asc::DenseBlasLayout selected,
               asc::DenseBlasTriangle triangle)
      : order(n), layout(selected) {
    data.fill(Value<T>(std::numeric_limits<double>::quiet_NaN()));
    // Enumerate documented physical major order, independently of the
    // adapter's packed-offset formula. Orders here are0,1,2,5.
    std::size_t cursor = 1;
    for (asc::extent_t major = 0; major < order; ++major) {
      for (asc::extent_t minor = 0; minor < order; ++minor) {
        const auto i = layout == kRow ? major : minor;
        const auto j = layout == kRow ? minor : major;
        if (triangle == kUpper ? i <= j : i >= j) {
          offsets[static_cast<std::size_t>(i * order + j)] = cursor++;
        }
      }
    }
  }
  T& At(asc::extent_t i, asc::extent_t j) {
    return data[offsets[static_cast<std::size_t>(i * order + j)]];
  }
  auto View() {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        data.data() + 1, order, layout, {data.data(), sizeof(data), kHost}));
  }
};

template <typename T>
struct Workspace {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 130> scalar;
  std::array<T, 130> packing;
  std::array<Real, 130> real;
  alignas(std::max_align_t) std::array<std::byte, 1040> integers;
  asc::LapackWorkspace value;
  explicit Workspace(const asc::LapackWorkspacePlan& plan) {
    scalar.fill(Value<T>(-4096, 1024));
    packing.fill(Value<T>(-4096, 1024));
    real.fill(Real{-4096});
    integers.fill(std::byte{0x5a});
    for (const auto kind : {kScalar, kReal, kInteger, kLayout}) {
      const auto& region = plan.regions[kind];
      const auto entries = static_cast<std::size_t>(region.minimum_entries);
      if (entries > 128) {
        std::abort();
      }
      if (entries == 0) {
        continue;
      }
      void* pointer = nullptr;
      if (kind == kScalar) {
        pointer = scalar.data() + 1;
      }
      if (kind == kReal) {
        pointer = real.data() + 1;
      }
      if (kind == kInteger) {
        pointer = integers.data() + 16;
      }
      if (kind == kLayout) {
        pointer = packing.data() + 1;
      }
      value.regions[kind] = {pointer, entries * region.entry_bytes, kHost};
    }
  }
  void Guards(Checks& checks, const asc::LapackWorkspacePlan& plan) const {
    for (const auto kind : {kScalar, kLayout}) {
      const auto& data = kind == kScalar ? scalar : packing;
      const auto count =
          static_cast<std::size_t>(plan.regions[kind].minimum_entries);
      checks.Expect(
          data.front() == Value<T>(-4096, 1024) &&
              std::all_of(data.begin() + 1 + count, data.end(),
                          [](T x) { return x == Value<T>(-4096, 1024); }),
          "scalar/packing redzones");
    }
    const auto reals =
        static_cast<std::size_t>(plan.regions[kReal].minimum_entries);
    checks.Expect(real.front() == Real{-4096} &&
                      std::all_of(real.begin() + 1 + reals, real.end(),
                                  [](Real x) { return x == Real{-4096}; }),
                  "real redzones");
    const auto bytes =
        static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
        plan.regions[kInteger].entry_bytes;
    checks.Expect(
        std::all_of(integers.begin(), integers.begin() + 16,
                    [](std::byte x) { return x == std::byte{0x5a}; }) &&
            std::all_of(integers.begin() + 16 + bytes, integers.end(),
                        [](std::byte x) { return x == std::byte{0x5a}; }),
        "native integer redzones");
  }
};

void Report(Checks& checks, const asc::Status& status,
            const asc::LapackReport& report, bool active) {
  checks.Expect(status.ok(), "successful numerical status");
  checks.Expect(report.called_provider == active &&
                    (active ? report.native_info == 0 : !report.native_info),
                "actual native INFO/local absence");
  checks.Expect(
      report.outcome == asc::LapackOutcome::kSuccess &&
          report.output_validity == asc::LapackOutputValidity::kComplete &&
          !report.diagnostic_index && !report.native_argument &&
          !report.factor_family,
      "success report without fabricated pivot/factor diagnosis");
}

template <typename T>
T Coefficient(asc::extent_t i, asc::extent_t j, asc::DenseBlasTriangle triangle,
              asc::DenseBlasDiagonal diagonal, int exponent, bool band) {
  const double scale = std::ldexp(1.0, exponent);
  if (i == j) {
    return Value<T>(diagonal == kUnit ? 1
                                      : (2 + static_cast<double>(i)) * scale);
  }
  if (band && ((triangle == kUpper && j == i + 1) ||
               (triangle == kLower && i == j + 1))) {
    return Value<T>(-scale / 4, scale / 8);
  }
  return T{};
}

template <typename T>
PackedMatrix<T> Triangle(asc::extent_t n, asc::DenseBlasLayout layout,
                         asc::DenseBlasTriangle triangle,
                         asc::DenseBlasDiagonal diagonal, int exponent,
                         bool band) {
  PackedMatrix<T> a(n, layout, triangle);
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      if ((triangle == kUpper ? i <= j : i >= j) &&
          (i != j || diagonal == kNonUnit)) {
        a.At(i, j) = Coefficient<T>(i, j, triangle, diagonal, exponent, band);
      }
    }
  }
  return a;
}

template <typename T>
void Condition(Checks& checks, const asc::ReferenceLapackProvider& provider,
               asc::extent_t n, asc::DenseBlasLayout layout,
               asc::DenseBlasTriangle triangle, asc::DenseBlasDiagonal diagonal,
               int exponent, asc::LapackConditionNorm norm) {
  using Real = asc::DenseBlasRealType<T>;
  auto a = Triangle<T>(n, layout, triangle, diagonal, exponent, false);
  const auto before = a.data;
  Real rcond = Real{-91};
  const auto plan = Take(asc::QueryTpconWorkspace(provider, norm, triangle,
                                                  diagonal, a.View(), rcond));
  checks.Expect(rcond == Real{-91} && Bytes(a.data, before),
                "condition query reads/writes no output");
  checks.Expect(plan.regions[kLayout].minimum_entries ==
                    (layout == kRow ? n * (n + 1) / 2 : 0),
                "exact packed condition layout capacity");
  Workspace<T> work(plan);
  asc::LapackReport report;
  const auto status = asc::Tpcon(provider, norm, triangle, diagonal, a.View(),
                                 rcond, plan, work.value, report);
  Report(checks, status, report, n != 0);
  const long double expected = n == 0 || diagonal == kUnit ? 1 : 2.0L / (n + 1);
  const long double tolerance =
      64 * std::numeric_limits<Real>::epsilon() * std::max<asc::extent_t>(1, n);
  checks.Expect(std::isfinite(rcond) &&
                    std::abs(static_cast<long double>(rcond) - expected) <=
                        tolerance * expected,
                "independent analytic diagonal condition estimate");
  checks.Expect(
      Bytes(a.data, before),
      "all condition inputs including ignored NaNs remain byte-identical");
  work.Guards(checks, plan);
  // Reusing a plan for the other norm must reject before touching values.
  const Real old_rcond = rcond;
  const auto old_scalar = work.scalar;
  const auto old_packing = work.packing;
  const auto other = norm == asc::LapackConditionNorm::kOne
                         ? asc::LapackConditionNorm::kInfinity
                         : asc::LapackConditionNorm::kOne;
  const auto stale = asc::Tpcon(provider, other, triangle, diagonal, a.View(),
                                rcond, plan, work.value, report);
  checks.Expect(stale.code() == asc::ErrorCode::kInvalidState &&
                    !report.called_provider && !report.native_info,
                "condition stale option plan rejected before native entry");
  checks.Expect(rcond == old_rcond && Bytes(work.scalar, old_scalar) &&
                    Bytes(work.packing, old_packing),
                "condition stale plan preserves outputs and scratch");
  checks.Workflow();
}

long double Abs1(Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}

template <typename T>
void VerifyEstimates(Checks& checks, asc::extent_t n, asc::extent_t nrhs,
                     asc::DenseBlasTriangle triangle,
                     asc::DenseBlasDiagonal diagonal,
                     asc::DenseBlasTranspose operation, int exponent,
                     Matrix<T>& b, Matrix<T>& x,
                     const std::array<asc::DenseBlasRealType<T>, 5>& ferr,
                     const std::array<asc::DenseBlasRealType<T>, 5>& berr) {
  using Real = asc::DenseBlasRealType<T>;
  const long double tolerance = 128 * std::numeric_limits<Real>::epsilon() *
                                std::max<asc::extent_t>(1, n);
  for (asc::extent_t j = 0; j < nrhs; ++j) {
    long double expected_berr = 0;
    long double error = 0;
    long double maximum_x = 0;
    for (asc::extent_t i = 0; i < n; ++i) {
      Wide residual = -Widen(b.At(i, j));
      long double denominator = Abs1(Widen(b.At(i, j)));
      for (asc::extent_t k = 0; k < n; ++k) {
        T coefficient =
            operation == kNone
                ? Coefficient<T>(i, k, triangle, diagonal, exponent, true)
                : Coefficient<T>(k, i, triangle, diagonal, exponent, true);
        if (operation == asc::DenseBlasTranspose::kConjugateTranspose) {
          coefficient = installed_internal::Conjugate(coefficient);
        }
        residual += Widen(coefficient) * Widen(x.At(k, j));
        denominator += Abs1(Widen(coefficient)) * Abs1(Widen(x.At(k, j)));
      }
      // These exactly representable fixtures have normal, positive
      // denominators.
      checks.Expect(denominator > 0, "independent nonzero BERR denominator");
      expected_berr = std::max(expected_berr, Abs1(residual) / denominator);
      const double truth = 1 + static_cast<double>(i + j) / 4;
      error = std::max(error, std::abs(Widen(x.At(i, j)) -
                                       Widen(Value<T>(truth, truth / 8))));
      maximum_x = std::max(maximum_x, Abs1(Widen(x.At(i, j))));
    }
    const auto f = ferr[static_cast<std::size_t>(j + 1)];
    const auto v = berr[static_cast<std::size_t>(j + 1)];
    checks.Expect(std::isfinite(v) &&
                      std::abs(static_cast<long double>(v) - expected_berr) <=
                          tolerance * (expected_berr == 0 ? 1 : expected_berr),
                  "independent componentwise BERR equation");
    if (n == 0) {
      checks.Expect(f == 0 && v == 0, "empty error bounds are exact zero");
    } else {
      checks.Expect(
          std::isfinite(f) && f >= 0 &&
              static_cast<long double>(f) * maximum_x >=
                  error * (1 - tolerance),
          "forward estimate covers independently known fixture error");
    }
  }
}

template <typename T>
void Errors(Checks& checks, const asc::ReferenceLapackProvider& provider,
            asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout al,
            asc::DenseBlasLayout bl, asc::DenseBlasLayout xl,
            asc::DenseBlasTriangle triangle, asc::DenseBlasDiagonal diagonal,
            asc::DenseBlasTranspose operation, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  auto a = Triangle<T>(n, al, triangle, diagonal, exponent, true);
  Matrix<T> b(n, nrhs, bl);
  Matrix<T> x(n, nrhs, xl);
  for (asc::extent_t j = 0; j < nrhs; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      const double value = 1 + static_cast<double>(i + j) / 4;
      x.At(i, j) = Value<T>(value + 1.0 / 32, value / 8 - 1.0 / 64);
      Wide rhs{};
      for (asc::extent_t k = 0; k < n; ++k) {
        T coefficient =
            operation == kNone
                ? Coefficient<T>(i, k, triangle, diagonal, exponent, true)
                : Coefficient<T>(k, i, triangle, diagonal, exponent, true);
        if (operation == asc::DenseBlasTranspose::kConjugateTranspose) {
          coefficient = installed_internal::Conjugate(coefficient);
        }
        const double truth = 1 + static_cast<double>(k + j) / 4;
        rhs += Widen(coefficient) * Widen(Value<T>(truth, truth / 8));
      }
      b.At(i, j) = Value<T>(static_cast<double>(rhs.real()),
                            static_cast<double>(rhs.imag()));
    }
  }
  std::array<Real, 5> ferr;
  std::array<Real, 5> berr;
  ferr.fill(Real{-71});
  berr.fill(Real{-73});
  auto fv = Take(asc::DenseBlasVectorView<Real>::Create(
      ferr.data() + 1, nrhs, 1, {ferr.data(), sizeof(ferr), kHost}));
  auto bv = Take(asc::DenseBlasVectorView<Real>::Create(
      berr.data() + 1, nrhs, 1, {berr.data(), sizeof(berr), kHost}));
  const auto before_a = a.data;
  const auto before_b = b.data;
  const auto before_x = x.data;
  const auto plan =
      Take(asc::QueryTprfsWorkspace(provider, triangle, diagonal, operation,
                                    a.View(), b.View(), x.View(), fv, bv));
  checks.Expect(ferr[1] == Real{-71} && berr[1] == Real{-73},
                "error query leaves output untouched");
  const asc::extent_t packing = n == 0 || nrhs == 0
                                    ? 0
                                    : (al == kRow ? n * (n + 1) / 2 : 0) +
                                          (bl == kRow ? n * nrhs : 0) +
                                          (xl == kRow ? n * nrhs : 0);
  checks.Expect(plan.regions[kLayout].minimum_entries == packing,
                "exact independently laid out packed A/full B/X capacities");
  Workspace<T> work(plan);
  asc::LapackReport report;
  const auto status =
      asc::Tprfs(provider, triangle, diagonal, operation, a.View(), b.View(),
                 x.View(), fv, bv, plan, work.value, report);
  Report(checks, status, report, n != 0 && nrhs != 0);
  VerifyEstimates(checks, n, nrhs, triangle, diagonal, operation, exponent, b,
                  x, ferr, berr);
  checks.Expect(ferr.front() == Real{-71} && berr.front() == Real{-73} &&
                    std::all_of(ferr.begin() + 1 + nrhs, ferr.end(),
                                [](Real v) { return v == Real{-71}; }) &&
                    std::all_of(berr.begin() + 1 + nrhs, berr.end(),
                                [](Real v) { return v == Real{-73}; }),
                "output vector redzones");
  checks.Expect(Bytes(a.data, before_a) && Bytes(b.data, before_b) &&
                    Bytes(x.data, before_x),
                "TPRFS preserves all A/B/X bytes including ignored operands");
  work.Guards(checks, plan);
  checks.Workflow();
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  for (const asc::extent_t n : {0, 1, 2, 5}) {
    for (const auto al : {kColumn, kRow}) {
      for (const auto triangle : {kUpper, kLower}) {
        for (const auto diagonal : {kUnit, kNonUnit}) {
          for (const int exponent : {-20, 0, 20}) {
            if (diagonal == kUnit && exponent != 0) {
              continue;
            }
            for (const auto norm : {asc::LapackConditionNorm::kOne,
                                    asc::LapackConditionNorm::kInfinity}) {
              Condition<T>(checks, provider, n, al, triangle, diagonal,
                           exponent, norm);
            }
            for (const auto bl : {kColumn, kRow}) {
              for (const auto xl : {kColumn, kRow}) {
                for (const auto operation :
                     {kNone, asc::DenseBlasTranspose::kTranspose,
                      asc::DenseBlasTranspose::kConjugateTranspose}) {
                  for (const asc::extent_t nrhs : {0, 3}) {
                    Errors<T>(checks, provider, n, nrhs, al, bl, xl, triangle,
                              diagonal, operation, exponent);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  return checks.Finish();
}
