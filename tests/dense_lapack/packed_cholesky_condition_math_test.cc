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
#include "asc/dense/providers/lapack_cholesky_packed_condition.h"
#include "factorization_support.h"
#include "internal_indefinite.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
constexpr auto kHost = asc::MemorySpace::kHost;

template <typename T>
struct DirectResult {
  asc::DenseBlasRealType<T> condition;
  lapack_int info;
};

template <typename T>
DirectResult<T> Direct(char triangle, const T& factor,
                       asc::DenseBlasRealType<T> norm) {
  using Real = asc::DenseBlasRealType<T>;
  DirectResult<T> result{std::numeric_limits<Real>::quiet_NaN(),
                         std::numeric_limits<lapack_int>::min()};
  const lapack_int n = 1;
  std::array<T, 3> work{};
  std::array<Real, 1> real{};
  std::array<lapack_int, 1> integer{};
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_sppcon(&triangle, &n, &factor, &norm, &result.condition, work.data(),
                  integer.data(), &result.info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dppcon(&triangle, &n, &factor, &norm, &result.condition, work.data(),
                  integer.data(), &result.info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_cppcon(&triangle, &n, &factor, &norm, &result.condition, work.data(),
                  real.data(), &result.info);
  } else {
    LAPACK_zppcon(&triangle, &n, &factor, &norm, &result.condition, work.data(),
                  real.data(), &result.info);
  }
  return result;
}

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
void Compare(asc::DenseBlasRealType<T> norm, asc::DenseBlasRealType<T> root,
             asc::DenseBlasRealType<T> condition,
             asc::DenseBlasTriangle triangle, const T& factor, bool success,
             const asc::LapackReport& report, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  // Independent required mathematical gate, unchanged for every scale.
  const bool finite = std::isfinite(condition);
  const bool correct =
      finite && std::abs(static_cast<long double>(condition) - 1) <=
                    64 * std::numeric_limits<Real>::epsilon();
  checks.Expect(success && correct &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  const auto direct = Direct(
      triangle == asc::DenseBlasTriangle::kUpper ? 'U' : 'L', factor, norm);
  // Direct comparison is argument-mapping evidence, never a replacement
  // for the independently required scalar condition-one assertion.
  checks.Expect(direct.info == 0 &&
                ((std::isnan(direct.condition) && std::isnan(condition)) ||
                 direct.condition == condition));
  if (!correct || !success) {
    std::printf(
        "Required scalar PPCON failure: norm=%La factor=%La rcond=%La "
        "direct=%La INFO=%jd\n",
        static_cast<long double>(norm), static_cast<long double>(root),
        static_cast<long double>(condition),
        static_cast<long double>(direct.condition),
        static_cast<std::intmax_t>(direct.info));
  }
}

template <typename T>
void Case(const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
          asc::DenseBlasRealType<T> norm, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  // The original one-by-one SPD matrix [norm] has exact condition one.
  // Its rounded square-root factor has at most two rounding units of
  // reconstruction error. Widen before squaring, including subnormals.
  const Real root = std::sqrt(norm);
  const long double reconstruction =
      static_cast<long double>(root) * root / norm;
  checks.Expect(std::isfinite(norm) && norm > 0 && std::isfinite(root) &&
                root > 0);
  checks.Expect(std::abs(reconstruction - 1) <=
                2 * std::numeric_limits<Real>::epsilon());
  std::array<T, 3> factor{T{19}, T{root}, T{19}};
  const auto original = factor;
  const auto a = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      factor.data() + 1, 1, layout, {factor.data(), sizeof(factor), kHost}));
  Real condition = -1;
  const auto plan =
      Take(asc::QueryPpconWorkspace(provider, triangle, a, norm, condition));
  installed_internal::Scratch<T> scratch;
  std::array<Real, 1> real{};
  scratch.workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal)] = {
      real.data(), sizeof(real), kHost};
  asc::LapackReport report;
  const auto status = asc::Ppcon(provider, triangle, a, norm, condition, plan,
                                 scratch.workspace, report);
  checks.Expect(report.called_provider && report.native_info == 0);
  checks.Expect(factor == original);
  Compare<T>(norm, root, condition, triangle, factor[1], status.ok(), report,
             checks);
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
      "Packed Cholesky required scalar condition: %d checks, %d failures\n",
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
