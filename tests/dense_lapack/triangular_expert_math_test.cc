#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_condition.h"
#include "asc/dense/providers/lapack_triangular_error_bounds.h"
#include "factorization_support.h"
#include "internal_indefinite.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kNonUnit = asc::DenseBlasDiagonal::kNonUnit;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;

template <typename T>
struct Direct;
template <>
struct Direct<float> {
  static constexpr const char* kName = "s";
  static lapack_int Run(bool condition, const float* a, const float* b,
                        const float* x, float& rcond, float& ferr,
                        float& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<float, 3> work{};
    [[maybe_unused]] std::array<float, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_strcon(&norm, &uplo, &diag, &n, a, &n, &rcond, work.data(),
                    integer.data(), &info);
    } else {
      LAPACK_strrfs(&uplo, &trans, &diag, &n, &n, a, &n, b, &n, x, &n, &ferr,
                    &berr, work.data(), integer.data(), &info);
    }
    return info;
  }
};
template <>
struct Direct<double> {
  static constexpr const char* kName = "d";
  static lapack_int Run(bool condition, const double* a, const double* b,
                        const double* x, double& rcond, double& ferr,
                        double& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<double, 3> work{};
    [[maybe_unused]] std::array<double, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_dtrcon(&norm, &uplo, &diag, &n, a, &n, &rcond, work.data(),
                    integer.data(), &info);
    } else {
      LAPACK_dtrrfs(&uplo, &trans, &diag, &n, &n, a, &n, b, &n, x, &n, &ferr,
                    &berr, work.data(), integer.data(), &info);
    }
    return info;
  }
};
template <>
struct Direct<std::complex<float>> {
  static constexpr const char* kName = "c";
  static lapack_int Run(bool condition, const std::complex<float>* a,
                        const std::complex<float>* b,
                        const std::complex<float>* x, float& rcond, float& ferr,
                        float& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<std::complex<float>, 3> work{};
    [[maybe_unused]] std::array<float, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_ctrcon(&norm, &uplo, &diag, &n, a, &n, &rcond, work.data(),
                    real.data(), &info);
    } else {
      LAPACK_ctrrfs(&uplo, &trans, &diag, &n, &n, a, &n, b, &n, x, &n, &ferr,
                    &berr, work.data(), real.data(), &info);
    }
    return info;
  }
};
template <>
struct Direct<std::complex<double>> {
  static constexpr const char* kName = "z";
  static lapack_int Run(bool condition, const std::complex<double>* a,
                        const std::complex<double>* b,
                        const std::complex<double>* x, double& rcond,
                        double& ferr, double& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<std::complex<double>, 3> work{};
    [[maybe_unused]] std::array<double, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_ztrcon(&norm, &uplo, &diag, &n, a, &n, &rcond, work.data(),
                    real.data(), &info);
    } else {
      LAPACK_ztrrfs(&uplo, &trans, &diag, &n, &n, a, &n, b, &n, x, &n, &ferr,
                    &berr, work.data(), real.data(), &info);
    }
    return info;
  }
};
struct Checks {
  std::size_t total = 0;
  std::size_t failed = 0;
  void Expect(bool value, const char* message) {
    ++total;
    if (!value) {
      ++failed;
      std::fprintf(stderr, "Required triangular mathematics: %s\n", message);
    }
  }
};

template <typename Real>
struct Estimates {
  Real rcond = Real{-1};
  std::array<Real, 1> ferr{Real{-1}};
  std::array<Real, 1> berr{Real{-1}};
};

template <typename Real>
bool Same(Real a, Real b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}

template <typename Real>
void Mathematics(Checks& checks, bool condition, Real scale,
                 const Estimates<Real>& values, const asc::Status& status) {
  const long double tolerance = 64 * std::numeric_limits<Real>::epsilon();
  if (condition) {
    checks.Expect(
        status.ok() && std::isfinite(values.rcond) &&
            std::abs(static_cast<long double>(values.rcond) - 1) <= tolerance,
        "any nonzero scalar has exact condition number one");
    return;
  }
  // The scalar weighted inverse is evaluated in wider arithmetic without
  // separately forming 1/A. DLAMCH E is half the C++ epsilon on this pin.
  const long double denominator = 2 * static_cast<long double>(scale);
  const long double epsilon = std::numeric_limits<Real>::epsilon() / 2;
  const long double safe1 =
      2 * static_cast<long double>(std::numeric_limits<Real>::min());
  const bool protected_denominator = denominator <= safe1 / epsilon;
  const long double expected_berr =
      protected_denominator ? safe1 / (denominator + safe1) : 0;
  const long double expected_ferr =
      (2 * epsilon * denominator + (protected_denominator ? safe1 : 0)) /
      static_cast<long double>(scale);
  checks.Expect(
      std::isfinite(values.berr[0]) &&
          std::abs(static_cast<long double>(values.berr[0]) - expected_berr) <=
              tolerance * (expected_berr == 0 ? 1 : expected_berr),
      "componentwise estimate includes pinned safe-denominator term");
  checks.Expect(status.ok() && std::isfinite(values.ferr[0]) &&
                    std::abs(static_cast<long double>(values.ferr[0]) -
                             expected_ferr) <= tolerance * expected_ferr,
                "finite analytic scalar weighted inverse error bound");
}

template <typename T>
void Case(Checks& checks, const asc::ReferenceLapackProvider& provider,
          bool condition, asc::DenseBlasRealType<T> scale, const char* label) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 1> a{T{scale}};
  std::array<T, 1> b{T{scale}};
  std::array<T, 1> x{T{1}};
  const auto view = [](std::array<T, 1>& data) {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data(), 1, 1, asc::DenseBlasLayout::kColumnMajor, 1,
        {data.data(), sizeof(data), kHost}));
  };
  Estimates<Real> values;
  const auto fv = installed_internal::Vector(values.ferr);
  const auto bv = installed_internal::Vector(values.berr);
  const auto norm = asc::LapackConditionNorm::kOne;
  const auto plan = Take(
      condition ? asc::QueryTrconWorkspace(provider, norm, kUpper, kNonUnit,
                                           view(a), values.rcond)
                : asc::QueryTrrfsWorkspace(provider, kUpper, kNonUnit, kNone,
                                           view(a), view(b), view(x), fv, bv));
  std::array<T, 3> work{};
  std::array<Real, 1> real{};
  alignas(std::max_align_t) std::array<std::byte, 8> integer{};
  asc::LapackWorkspace workspace;
  constexpr auto kScalar =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  constexpr auto kInteger =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
  workspace.regions[kScalar] = {
      work.data(),
      static_cast<std::size_t>(plan.regions[kScalar].minimum_entries) *
          sizeof(T),
      kHost};
  if constexpr (asc::DenseBlasComplex<T>) {
    workspace.regions[kReal] = {real.data(), sizeof(real), kHost};
  } else {
    workspace.regions[kInteger] = {integer.data(),
                                   plan.regions[kInteger].entry_bytes, kHost};
  }
  asc::LapackReport report;
  const auto status =
      condition ? asc::Trcon(provider, norm, kUpper, kNonUnit, view(a),
                             values.rcond, plan, workspace, report)
                : asc::Trrfs(provider, kUpper, kNonUnit, kNone, view(a),
                             view(b), view(x), fv, bv, plan, workspace, report);
  Estimates<Real> raw;
  const lapack_int info =
      Direct<T>::Run(condition, a.data(), b.data(), x.data(), raw.rcond,
                     raw.ferr[0], raw.berr[0]);
  std::printf(
      "%s %s %s A=%.21Lg wrapper=(%.21Lg,%.21Lg,%.21Lg) "
      "raw=(%.21Lg,%.21Lg,%.21Lg) rawINFO=%.0Lf\n",
      Direct<T>::kName, condition ? "TRCON" : "TRRFS", label,
      static_cast<long double>(scale), static_cast<long double>(values.rcond),
      static_cast<long double>(values.ferr[0]),
      static_cast<long double>(values.berr[0]),
      static_cast<long double>(raw.rcond),
      static_cast<long double>(raw.ferr[0]),
      static_cast<long double>(raw.berr[0]), static_cast<long double>(info));
  checks.Expect(
      info == 0 && report.called_provider && report.native_info == info,
      "native INFO completion");
  checks.Expect(condition ? Same(values.rcond, raw.rcond)
                          : Same(values.ferr[0], raw.ferr[0]) &&
                                Same(values.berr[0], raw.berr[0]),
                "wrapper matches independently called exact raw routine");
  checks.Expect(a[0] == T{scale} && b[0] == T{scale} && x[0] == T{1},
                "immutable original scalar equation");
  Mathematics(checks, condition, scale, values, status);
}

template <typename T>
int Scalar(bool condition) {
  using Real = asc::DenseBlasRealType<T>;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Case<T>(checks, provider, condition,
          std::numeric_limits<Real>::min() / Real{8}, "min-normal/8");
  Case<T>(checks, provider, condition, std::numeric_limits<Real>::min(),
          "min-normal");
  Case<T>(checks, provider, condition, Real{1}, "one");
  Case<T>(checks, provider, condition,
          std::numeric_limits<Real>::max() / Real{4}, "max/4");
  Case<T>(checks, provider, condition, std::numeric_limits<Real>::max(), "max");
  std::printf("Required triangular scalar math: %zu checks, %zu failures\n",
              checks.total, checks.failed);
  return checks.failed == 0 ? 0 : 1;
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const bool condition = std::strcmp(argv[2], "trcon") == 0;
  if (!condition && std::strcmp(argv[2], "trrfs") != 0) {
    return 2;
  }
  if (std::strcmp(argv[1], "s") == 0) {
    return Scalar<float>(condition);
  }
  if (std::strcmp(argv[1], "d") == 0) {
    return Scalar<double>(condition);
  }
  if (std::strcmp(argv[1], "c") == 0) {
    return Scalar<std::complex<float>>(condition);
  }
  if (std::strcmp(argv[1], "z") == 0) {
    return Scalar<std::complex<double>>(condition);
  }
  return 2;
}
