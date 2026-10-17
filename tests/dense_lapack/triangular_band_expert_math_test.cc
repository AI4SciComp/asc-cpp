#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_triangular_band_condition.h"
#include "asc/dense/providers/lapack_triangular_band_error_bounds.h"
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
  static lapack_int BandCondition(lapack_int n, char norm, char uplo,
                                  const float* a, float& rcond) {
    const char diag = 'N';
    const lapack_int kd = n - 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<float, 9> work{};
    [[maybe_unused]] std::array<float, 3> real{};
    [[maybe_unused]] std::array<lapack_int, 3> integer{};
    LAPACK_stbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                  integer.data(), &info);
    return info;
  }
  static lapack_int Run(bool condition, const float* a, const float* b,
                        const float* x, float& rcond, float& ferr,
                        float& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    const lapack_int kd = 0;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<float, 3> work{};
    [[maybe_unused]] std::array<float, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_stbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                    integer.data(), &info);
    } else {
      LAPACK_stbrfs(&uplo, &trans, &diag, &n, &kd, &n, a, &n, b, &n, x, &n,
                    &ferr, &berr, work.data(), integer.data(), &info);
    }
    return info;
  }
};
template <>
struct Direct<double> {
  static constexpr const char* kName = "d";
  static lapack_int BandCondition(lapack_int n, char norm, char uplo,
                                  const double* a, double& rcond) {
    const char diag = 'N';
    const lapack_int kd = n - 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<double, 9> work{};
    [[maybe_unused]] std::array<double, 3> real{};
    [[maybe_unused]] std::array<lapack_int, 3> integer{};
    LAPACK_dtbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                  integer.data(), &info);
    return info;
  }
  static lapack_int Run(bool condition, const double* a, const double* b,
                        const double* x, double& rcond, double& ferr,
                        double& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    const lapack_int kd = 0;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<double, 3> work{};
    [[maybe_unused]] std::array<double, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_dtbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                    integer.data(), &info);
    } else {
      LAPACK_dtbrfs(&uplo, &trans, &diag, &n, &kd, &n, a, &n, b, &n, x, &n,
                    &ferr, &berr, work.data(), integer.data(), &info);
    }
    return info;
  }
};
template <>
struct Direct<std::complex<float>> {
  static constexpr const char* kName = "c";
  static lapack_int BandCondition(lapack_int n, char norm, char uplo,
                                  const std::complex<float>* a, float& rcond) {
    const char diag = 'N';
    const lapack_int kd = n - 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<std::complex<float>, 9> work{};
    [[maybe_unused]] std::array<float, 3> real{};
    [[maybe_unused]] std::array<lapack_int, 3> integer{};
    LAPACK_ctbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                  real.data(), &info);
    return info;
  }
  static lapack_int Run(bool condition, const std::complex<float>* a,
                        const std::complex<float>* b,
                        const std::complex<float>* x, float& rcond, float& ferr,
                        float& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    const lapack_int kd = 0;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<std::complex<float>, 3> work{};
    [[maybe_unused]] std::array<float, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_ctbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                    real.data(), &info);
    } else {
      LAPACK_ctbrfs(&uplo, &trans, &diag, &n, &kd, &n, a, &n, b, &n, x, &n,
                    &ferr, &berr, work.data(), real.data(), &info);
    }
    return info;
  }
};
template <>
struct Direct<std::complex<double>> {
  static constexpr const char* kName = "z";
  static lapack_int BandCondition(lapack_int n, char norm, char uplo,
                                  const std::complex<double>* a,
                                  double& rcond) {
    const char diag = 'N';
    const lapack_int kd = n - 1;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<std::complex<double>, 9> work{};
    [[maybe_unused]] std::array<double, 3> real{};
    [[maybe_unused]] std::array<lapack_int, 3> integer{};
    LAPACK_ztbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                  real.data(), &info);
    return info;
  }
  static lapack_int Run(bool condition, const std::complex<double>* a,
                        const std::complex<double>* b,
                        const std::complex<double>* x, double& rcond,
                        double& ferr, double& berr) {
    const char norm = '1';
    const char uplo = 'U';
    const char trans = 'N';
    const char diag = 'N';
    const lapack_int n = 1;
    const lapack_int kd = 0;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    std::array<std::complex<double>, 3> work{};
    [[maybe_unused]] std::array<double, 1> real{};
    [[maybe_unused]] std::array<lapack_int, 1> integer{};
    if (condition) {
      LAPACK_ztbcon(&norm, &uplo, &diag, &n, &kd, a, &n, &rcond, work.data(),
                    real.data(), &info);
    } else {
      LAPACK_ztbrfs(&uplo, &trans, &diag, &n, &kd, &n, a, &n, b, &n, x, &n,
                    &ferr, &berr, work.data(), real.data(), &info);
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
  const auto av = Take(asc::LapackTriangularBandView<const T>::Create(
      a.data(), 1, 0, kUpper, asc::DenseBlasLayout::kColumnMajor, 1,
      {a.data(), sizeof(a), kHost}));
  Estimates<Real> values;
  const auto fv = installed_internal::Vector(values.ferr);
  const auto bv = installed_internal::Vector(values.berr);
  const auto norm = asc::LapackConditionNorm::kOne;
  const auto plan =
      Take(condition ? asc::QueryTbconWorkspace(provider, norm, kNonUnit, av,
                                                values.rcond)
                     : asc::QueryTbrfsWorkspace(provider, kNonUnit, kNone, av,
                                                view(b), view(x), fv, bv));
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
      condition ? asc::Tbcon(provider, norm, kNonUnit, av, values.rcond, plan,
                             workspace, report)
                : asc::Tbrfs(provider, kNonUnit, kNone, av, view(b), view(x),
                             fv, bv, plan, workspace, report);
  Estimates<Real> raw;
  const lapack_int info =
      Direct<T>::Run(condition, a.data(), b.data(), x.data(), raw.rcond,
                     raw.ferr[0], raw.berr[0]);
  std::printf(
      "%s %s %s A=%.21Lg wrapper=(%.21Lg,%.21Lg,%.21Lg) "
      "raw=(%.21Lg,%.21Lg,%.21Lg) rawINFO=%.0Lf\n",
      Direct<T>::kName, condition ? "TBCON" : "TBRFS", label,
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
struct ConditionWorkspace {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 9> scalar{};
  std::array<T, 16> packing{};
  std::array<Real, 3> real{};
  std::array<lapack_int, 3> integer{};
  asc::LapackWorkspace value;
  explicit ConditionWorkspace(const asc::LapackWorkspacePlan& plan) {
    for (const auto kind :
         {asc::LapackWorkspaceKind::kScalar, asc::LapackWorkspaceKind::kReal,
          asc::LapackWorkspaceKind::kInteger,
          asc::LapackWorkspaceKind::kLayoutConversion}) {
      const auto index = static_cast<std::size_t>(kind);
      const auto& requirement = plan.regions[index];
      if (requirement.minimum_entries == 0) {
        continue;
      }
      void* data = scalar.data();
      if (kind == asc::LapackWorkspaceKind::kReal) {
        data = real.data();
      } else if (kind == asc::LapackWorkspaceKind::kInteger) {
        data = integer.data();
      } else if (kind == asc::LapackWorkspaceKind::kLayoutConversion) {
        data = packing.data();
      }
      value.regions[index] = {
          data,
          static_cast<std::size_t>(requirement.minimum_entries) *
              requirement.entry_bytes,
          kHost};
    }
  }
};

template <typename T>
std::array<T, 16> OverflowMatrix(asc::extent_t n,
                                 asc::DenseBlasRealType<T> scale,
                                 asc::DenseBlasLayout layout,
                                 asc::DenseBlasTriangle triangle) {
  std::array<T, 16> a{};
  a.fill(T{asc::DenseBlasRealType<T>{-9}});
  for (asc::extent_t major = 0; major < n; ++major) {
    for (asc::extent_t minor = 0; minor < n; ++minor) {
      const auto i =
          layout == asc::DenseBlasLayout::kColumnMajor ? minor : major;
      const auto j =
          layout == asc::DenseBlasLayout::kColumnMajor ? major : minor;
      if (triangle == kUpper ? i <= j : i >= j) {
        asc::extent_t slot = major * n + minor - major;
        if ((layout == asc::DenseBlasLayout::kColumnMajor &&
             triangle == kUpper) ||
            (layout == asc::DenseBlasLayout::kRowMajor && triangle != kUpper)) {
          slot += n - 1;
        }
        a[static_cast<std::size_t>(slot)] =
            T{(i + j) % 2 == 0 ? scale : -scale};
      }
    }
  }
  return a;
}

template <typename T>
void BandOverflowCase(Checks& checks,
                      const asc::ReferenceLapackProvider& provider,
                      asc::extent_t n, asc::DenseBlasRealType<T> scale,
                      asc::DenseBlasLayout layout,
                      asc::DenseBlasTriangle triangle,
                      asc::LapackConditionNorm norm) {
  using Real = asc::DenseBlasRealType<T>;
  // A = scale * D*U*D, where U has ones on/above the diagonal and
  // D = diag(1,-1,1), or the transpose. inv(D*U*D) has diagonal1 and
  // first superdiagonal+1, all other entries0. Its nonnegative entries let
  // the norm estimator attain the exact largest column/row sum. Both
  // norm products are 2*n, independently of scale. The earlier all-positive
  // U fixture had a sign-changing inverse, so epsilon equality was not a
  // valid estimator oracle; that executed candidate is preserved separately.
  auto a = OverflowMatrix<T>(n, scale, layout, triangle);
  const auto before = a;
  const auto view = Take(asc::LapackTriangularBandView<const T>::Create(
      a.data(), n, n - 1, triangle, layout, n, {a.data(), sizeof(a), kHost}));
  Real rcond = Real{-1};
  const auto plan =
      Take(asc::QueryTbconWorkspace(provider, norm, kNonUnit, view, rcond));
  ConditionWorkspace<T> workspace(plan);
  asc::LapackReport report;
  const auto status = asc::Tbcon(provider, norm, kNonUnit, view, rcond, plan,
                                 workspace.value, report);
  Real raw = Real{-1};
  const auto column =
      OverflowMatrix<T>(n, scale, asc::DenseBlasLayout::kColumnMajor, triangle);
  const lapack_int info = Direct<T>::BandCondition(
      static_cast<lapack_int>(n),
      norm == asc::LapackConditionNorm::kOne ? '1' : 'I',
      triangle == kUpper ? 'U' : 'L', column.data(), raw);
  const long double expected = 1 / (2 * static_cast<long double>(n));
  checks.Expect(
      info == 0 && report.native_info == info && report.called_provider,
      "band overflow fixture returns native INFO");
  checks.Expect(Same(rcond, raw), "band overflow fixture exact raw mapping");
  checks.Expect(a == before, "band overflow preserves selected data and tail");
  checks.Expect(status.ok() && std::isfinite(rcond) &&
                    std::abs(static_cast<long double>(rcond) - expected) <=
                        128 * std::numeric_limits<Real>::epsilon() * expected,
                "finite band triangular matrix has analytic RCOND 1/(2*n)");
  std::printf(
      "%s band norm n=%lld scale=%.21Lg layout=%d triangle=%d norm=%d "
      "wrapper=%.21Lg raw=%.21Lg expected=%.21Lg INFO=%.0Lf\n",
      Direct<T>::kName, static_cast<long long>(n),
      static_cast<long double>(scale), static_cast<int>(layout),
      static_cast<int>(triangle), static_cast<int>(norm),
      static_cast<long double>(rcond), static_cast<long double>(raw), expected,
      static_cast<long double>(info));
}

template <typename T>
void BandOverflow(Checks& checks,
                  const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const asc::extent_t n : {2, 3}) {
    for (const Real scale :
         {Real{1}, std::numeric_limits<Real>::max() / Real{4},
          std::numeric_limits<Real>::max()}) {
      for (const auto layout : {asc::DenseBlasLayout::kColumnMajor,
                                asc::DenseBlasLayout::kRowMajor}) {
        for (const auto triangle : {kUpper, asc::DenseBlasTriangle::kLower}) {
          for (const auto norm : {asc::LapackConditionNorm::kOne,
                                  asc::LapackConditionNorm::kInfinity}) {
            BandOverflowCase<T>(checks, provider, n, scale, layout, triangle,
                                norm);
          }
        }
      }
    }
  }
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
  if (condition) {
    BandOverflow<T>(checks, provider);
  }
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
  const bool condition = std::strcmp(argv[2], "tbcon") == 0;
  if (!condition && std::strcmp(argv[2], "tbrfs") != 0) {
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
