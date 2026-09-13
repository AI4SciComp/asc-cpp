#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_refinement.h"
#include "factorization_support.h"
#include "internal_indefinite.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
constexpr auto kHost = asc::MemorySpace::kHost;

struct Checks {
  int checks = 0;
  int failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      ++failures;
    }
  }
};

template <typename T>
struct Values {
  using Real = asc::DenseBlasRealType<T>;
  T solution{1};
  Real forward = std::numeric_limits<Real>::quiet_NaN();
  Real backward = std::numeric_limits<Real>::quiet_NaN();
};

template <typename T>
struct DirectResult {
  Values<T> values;
  lapack_int info = std::numeric_limits<lapack_int>::min();
};

template <typename T>
DirectResult<T> Direct(char triangle, const T& original, const T& factor) {
  using Real = asc::DenseBlasRealType<T>;
  DirectResult<T> result;
  const lapack_int n = 1;
  const T rhs = original;
  std::array<T, 3> work{};
  std::array<Real, 1> real{};
  std::array<lapack_int, 1> integer{};
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_spprfs(&triangle, &n, &n, &original, &factor, &rhs, &n,
                  &result.values.solution, &n, &result.values.forward,
                  &result.values.backward, work.data(), integer.data(),
                  &result.info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dpprfs(&triangle, &n, &n, &original, &factor, &rhs, &n,
                  &result.values.solution, &n, &result.values.forward,
                  &result.values.backward, work.data(), integer.data(),
                  &result.info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cpprfs(&triangle, &n, &n, &original, &factor, &rhs, &n,
                  &result.values.solution, &n, &result.values.forward,
                  &result.values.backward, work.data(), real.data(),
                  &result.info);
  } else {
    LAPACK_zpprfs(&triangle, &n, &n, &original, &factor, &rhs, &n,
                  &result.values.solution, &n, &result.values.forward,
                  &result.values.backward, work.data(), real.data(),
                  &result.info);
  }
  return result;
}

template <typename Real>
bool Same(Real a, Real b) {
  return (std::isnan(a) && std::isnan(b)) || a == b;
}

bool Near(long double actual, long double expected, long double tolerance) {
  if (!std::isfinite(actual)) {
    return false;
  }
  return expected == 0
             ? actual == 0
             : std::abs(actual - expected) / std::abs(expected) <= tolerance;
}

template <typename T>
void Compare(asc::DenseBlasRealType<T> norm, const Values<T>& actual,
             const DirectResult<T>& direct, bool success,
             const asc::LapackReport& report, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  // Scalar A=[norm], B=[norm], X=[1] has exact zero residual and condition one.
  // In these attested IEEE builds LAMCH(E)=epsilon/2 and SAFE1=2*min_normal.
  // W=2*EPS*(2*norm), plus SAFE1 only when the denominator <= SAFE2.
  // For N1 LACN2 estimates the scalar exactly apart from factor/solve rounding.
  const long double eps = std::numeric_limits<Real>::epsilon() / 2.0L;
  const long double safe = 2.0L * std::numeric_limits<Real>::min();
  const long double denominator = 2.0L * norm;
  const bool safeguarded = denominator <= safe / eps;
  const long double expected_forward =
      (2 * eps * denominator + (safeguarded ? safe : 0)) / norm;
  const long double expected_backward =
      safeguarded ? safe / (denominator + safe) : 0;
  const long double tolerance = 64 * std::numeric_limits<Real>::epsilon();
  const auto solution = static_cast<std::complex<long double>>(actual.solution);
  const bool correct =
      std::isfinite(std::abs(solution)) &&
      std::abs(solution - std::complex<long double>{1, 0}) <= tolerance &&
      Near(actual.forward, expected_forward, tolerance) &&
      Near(actual.backward, expected_backward, tolerance);
  checks.Expect(success && correct &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  // Direct agreement establishes argument mapping only; it never waives
  // the independent finite scalar error-estimate formulas above.
  checks.Expect(
      direct.info == 0 &&
      Same(std::real(direct.values.solution), std::real(actual.solution)) &&
      Same(std::imag(direct.values.solution), std::imag(actual.solution)) &&
      Same(direct.values.forward, actual.forward) &&
      Same(direct.values.backward, actual.backward));
  if (!correct || !success) {
    std::printf(
        "Required scalar PPRFS failure: A=%La X=(%La,%La) FERR=%La "
        "expected=%La BERR=%La expected=%La directFERR=%La INFO=%jd\n",
        static_cast<long double>(norm), solution.real(), solution.imag(),
        static_cast<long double>(actual.forward), expected_forward,
        static_cast<long double>(actual.backward), expected_backward,
        static_cast<long double>(direct.values.forward),
        static_cast<std::intmax_t>(direct.info));
  }
}

template <typename T>
void Case(const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
          asc::DenseBlasRealType<T> norm, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const Real root = std::sqrt(norm);
  const long double reconstruction =
      static_cast<long double>(root) * root / norm;
  checks.Expect(std::isfinite(norm) && norm > 0 && std::isfinite(root) &&
                root > 0);
  checks.Expect(std::abs(reconstruction - 1) <=
                2 * std::numeric_limits<Real>::epsilon());
  std::array<T, 3> original{T{19}, T{norm}, T{19}};
  std::array<T, 3> factor{T{23}, T{root}, T{23}};
  std::array<T, 3> rhs{T{29}, T{norm}, T{29}};
  std::array<T, 3> solution{T{31}, T{1}, T{31}};
  std::array<Real, 3> forward{Real{37}, Real{-1}, Real{37}};
  std::array<Real, 3> backward{Real{41}, Real{-1}, Real{41}};
  const auto before_a = original;
  const auto before_af = factor;
  const auto before_b = rhs;
  const auto a = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      original.data() + 1, 1, layout,
      {original.data(), sizeof(original), kHost}));
  const auto af = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      factor.data() + 1, 1, layout, {factor.data(), sizeof(factor), kHost}));
  const auto b = Take(asc::DenseBlasMatrixView<const T>::Create(
      rhs.data() + 1, 1, 1, layout, 3, {rhs.data(), sizeof(rhs), kHost}));
  const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
      solution.data() + 1, 1, 1, layout, 3,
      {solution.data(), sizeof(solution), kHost}));
  const auto f = Take(asc::DenseBlasVectorView<Real>::Create(
      forward.data() + 1, 1, 1, {forward.data(), sizeof(forward), kHost}));
  const auto e = Take(asc::DenseBlasVectorView<Real>::Create(
      backward.data() + 1, 1, 1, {backward.data(), sizeof(backward), kHost}));
  const auto plan =
      Take(asc::QueryPprfsWorkspace(provider, triangle, a, af, b, x, f, e));
  installed_internal::Scratch<T> scratch;
  std::array<Real, 1> real{};
  scratch.workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)] = {
      real.data(), sizeof(real), kHost};
  asc::LapackReport report;
  const auto status = asc::Pprfs(provider, triangle, a, af, b, x, f, e, plan,
                                 scratch.workspace, report);
  checks.Expect(report.called_provider && report.native_info == 0);
  checks.Expect(original == before_a && factor == before_af && rhs == before_b);
  checks.Expect(solution.front() == T{31} && solution.back() == T{31} &&
                forward.front() == Real{37} && forward.back() == Real{37} &&
                backward.front() == Real{41} && backward.back() == Real{41});
  const Values<T> actual{solution[1], forward[1], backward[1]};
  const auto direct = Direct<T>(
      triangle == asc::DenseBlasTriangle::kUpper ? 'U' : 'L', T{norm}, T{root});
  Compare<T>(norm, actual, direct, status.ok(), report, checks);
}

template <typename T>
int Run(const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array norms{Real{1},
                         std::numeric_limits<Real>::min(),
                         std::numeric_limits<Real>::max() / 4,
                         std::numeric_limits<Real>::denorm_min(),
                         2 * std::numeric_limits<Real>::denorm_min(),
                         std::numeric_limits<Real>::max()};
  Checks checks;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto layout : {asc::DenseBlasLayout::kRowMajor,
                              asc::DenseBlasLayout::kColumnMajor}) {
      for (const Real norm : norms) {
        Case<T>(provider, triangle, layout, norm, checks);
      }
    }
  }
  std::printf(
      "Packed Cholesky required scalar refinement: %d checks, %d failures\n",
      checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(provider);
  }
  if (scalar == "d") {
    return Run<double>(provider);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(provider);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(provider);
  }
  return 2;
}
