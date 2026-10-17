#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk.h"
#include "asc/dense/providers/lapack_indefinite_rk_condition.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;

template <typename T>
struct Scratch {
  static constexpr auto kScalar =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
  static constexpr auto kLayout =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
  static constexpr auto kInteger =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
  std::array<T, 5002> scalar{};
  std::array<T, 5002> packing{};
  alignas(
      16) std::array<std::byte, 2 * 67 * sizeof(asc::index_t) + 32> integers{};
  Scratch() {
    scalar.fill(installed_internal::Value<T>(-107, 11));
    packing.fill(installed_internal::Value<T>(-109, 13));
    integers.fill(std::byte{0x5a});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    for (const auto role : {kScalar, kLayout, kInteger}) {
      const auto& requirement = plan.regions[role];
      const auto bytes =
          static_cast<std::size_t>(requirement.preferred_entries) *
          requirement.entry_bytes;
      if (bytes == 0) {
        continue;
      }
      if (role == kScalar) {
        if (bytes > (scalar.size() - 2) * sizeof(T)) {
          std::abort();
        }
        result.regions[role] = {scalar.data() + 1, bytes,
                                installed_internal::kHost};
      } else if (role == kLayout) {
        if (bytes > (packing.size() - 2) * sizeof(T)) {
          std::abort();
        }
        result.regions[role] = {packing.data() + 1, bytes,
                                installed_internal::kHost};
      } else {
        if (bytes > integers.size() - 32) {
          std::abort();
        }
        result.regions[role] = {integers.data() + 16, bytes,
                                installed_internal::kHost};
      }
    }
    return result;
  }
  [[nodiscard]] bool Guards(const asc::LapackWorkspace& workspace) const {
    bool ok = scalar.front() == installed_internal::Value<T>(-107, 11) &&
              packing.front() == installed_internal::Value<T>(-109, 13);
    for (std::size_t i = 1 + workspace.regions[kScalar].size() / sizeof(T);
         i < scalar.size(); ++i) {
      ok = (scalar[i] == installed_internal::Value<T>(-107, 11)) && ok;
    }
    for (std::size_t i = 1 + workspace.regions[kLayout].size() / sizeof(T);
         i < packing.size(); ++i) {
      ok = (packing[i] == installed_internal::Value<T>(-109, 13)) && ok;
    }
    for (std::size_t i = 0; i < 16; ++i) {
      ok = (integers[i] == std::byte{0x5a}) && ok;
    }
    for (std::size_t i = 16 + workspace.regions[kInteger].size();
         i < integers.size(); ++i) {
      ok = (integers[i] == std::byte{0x5a}) && ok;
    }
    return ok;
  }
};
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool hermitian, bool blocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<T> extra,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::QueryHetrfRkWorkspace(provider, triangle, matrix,
                                                  extra, pivots)
                     : asc::QueryHetf2RkWorkspace(provider, triangle, matrix,
                                                  extra, pivots);
    }
  }
  return blocked ? asc::QuerySytrfRkWorkspace(provider, triangle, matrix, extra,
                                              pivots)
                 : asc::QuerySytf2RkWorkspace(provider, triangle, matrix, extra,
                                              pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   bool blocked, asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> extra,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::HetrfRk(provider, triangle, matrix, extra, pivots,
                                    plan, workspace, report)
                     : asc::Hetf2Rk(provider, triangle, matrix, extra, pivots,
                                    plan, workspace, report);
    }
  }
  return blocked ? asc::SytrfRk(provider, triangle, matrix, extra, pivots, plan,
                                workspace, report)
                 : asc::Sytf2Rk(provider, triangle, matrix, extra, pivots, plan,
                                workspace, report);
}

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> a,
           asc::DenseBlasVectorView<const T> extra,
           asc::RawLapackPivotView pivots, asc::DenseBlasRealType<T> norm,
           const asc::DenseBlasRealType<T>& condition) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHecon3Workspace(provider, triangle, a, extra, pivots,
                                       norm, condition);
    }
  }
  return asc::QuerySycon3Workspace(provider, triangle, a, extra, pivots, norm,
                                   condition);
}

template <typename T>
asc::Status Condition(const asc::ReferenceLapackProvider& provider,
                      asc::DenseBlasTriangle triangle, bool hermitian,
                      asc::DenseBlasMatrixView<const T> a,
                      asc::DenseBlasVectorView<const T> extra,
                      asc::RawLapackPivotView pivots,
                      asc::DenseBlasRealType<T> norm,
                      asc::DenseBlasRealType<T>& condition,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hecon3(provider, triangle, a, extra, pivots, norm, condition,
                         plan, workspace, report);
    }
  }
  return asc::Sycon3(provider, triangle, a, extra, pivots, norm, condition,
                     plan, workspace, report);
}

template <typename T, std::size_t N>
std::array<asc::DenseBlasRealType<T>, 2> Initialize(
    installed_internal::Matrix<T, N, N>& a, bool hermitian,
    asc::DenseBlasTriangle triangle, asc::DenseBlasRealType<T> scale,
    bool singular) {
  using Real = asc::DenseBlasRealType<T>;
  a.data.fill(installed_internal::Value<T>(-71, 13));
  Real largest = 0;
  Real smallest = std::numeric_limits<Real>::infinity();
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      if (triangle == asc::DenseBlasTriangle::kUpper ? i <= j : i >= j) {
        T value{};
        if (i == j && (N == 1 || i >= 2) && (!singular || i != N - 1)) {
          value = N == 1 ? T{4}
                         : installed_internal::Value<T>(i % 2 == 0 ? 8 : -8,
                                                        hermitian ? 0 : 6);
        }
        if (i + j == 1) {
          value = installed_internal::Value<T>(3, hermitian && i == 0 ? -4 : 4);
        }
        value *= scale;
        a.At(i, j) = value;
        if (value != T{}) {
          const Real magnitude = std::abs(value);
          largest = std::max(largest, magnitude);
          smallest = std::min(smallest, magnitude);
        }
      }
    }
  }
  return {smallest, largest};
}
template <typename T, std::size_t N>
void PoisonIgnored(std::array<T, N + 2>& e,
                   const std::array<asc::index_t, N + 2>& pivots,
                   asc::DenseBlasTriangle triangle) {
  using Real = asc::DenseBlasRealType<T>;
  for (std::size_t i = 0; i < N;) {
    const bool pair = pivots[i + 1] < 0;
    const auto ignored =
        pair && triangle == asc::DenseBlasTriangle::kLower ? i + 1 : i;
    e[ignored + 1] = installed_internal::Value<T>(
        std::numeric_limits<Real>::quiet_NaN(), 19);
    i += pair ? 2 : 1;
  }
}
// Compare every input byte, including ignored NaN payloads and signed zeros.
bool EqualBytes(const void* left, const void* right, std::size_t bytes) {
  return std::memcmp(left, right, bytes) == 0;
}
template <typename T, std::size_t N>
bool Check(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           bool blocked, int exponent, bool singular, int& cases) {
  using Real = asc::DenseBlasRealType<T>;
  installed_internal::Matrix<T, N, N> a{{}, layout};
  const auto [smallest, largest] = Initialize(
      a, hermitian, triangle, std::ldexp(Real{1}, exponent), singular);
  std::array<T, N + 2> e;
  e.fill(installed_internal::Value<T>(-73, 17));
  std::array<asc::index_t, N + 2> pivots;
  pivots.fill(-79);
  const auto extra = Take(asc::DenseBlasVectorView<T>::Create(
      e.data() + 1, N, 1, {e.data(), sizeof(e), installed_internal::kHost}));
  const auto pivot = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1,
      {pivots.data(), sizeof(pivots), installed_internal::kHost}));
  const auto fp = Take(QueryFactor(provider, triangle, hermitian, blocked,
                                   a.View(), extra, pivot));
  Scratch<T> factor_scratch;
  const auto fw = factor_scratch.Workspace(fp);
  asc::LapackReport factor_report;
  const auto factored = Factor(provider, triangle, hermitian, blocked, a.View(),
                               extra, pivot, fp, fw, factor_report);
  bool passed = factored.ok() != singular &&
                factor_report.native_info.value_or(0) ==
                    (singular ? static_cast<asc::index_t>(N) : 0) &&
                factor_scratch.Guards(fw);
  PoisonIgnored<T, N>(e, pivots, triangle);
  const auto saved_a = a.data;
  const auto saved_e = e;
  const auto saved_p = pivots;
  const asc::DenseBlasVectorView<const T> immutable_e(extra);
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, N, asc::LapackFactorFamily::kRook,
      {pivots.data(), sizeof(pivots), installed_internal::kHost}));
  for (const bool zero : {false, true}) {
    const Real norm = zero ? Real{} : largest;
    std::array<Real, 3> condition{Real{-101}, Real{-13}, Real{-103}};
    const auto plan = Take(Query(provider, triangle, hermitian, a.ConstView(),
                                 immutable_e, raw, norm, condition[1]));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status =
        Condition(provider, triangle, hermitian, a.ConstView(), immutable_e,
                  raw, norm, condition[1], plan, workspace, report);
    const bool active = N != 0 && norm != 0;
    Real expected = 1;
    if constexpr (N != 0) {
      expected = zero || singular ? Real{} : smallest / largest;
    }
    const bool okay =
        status.ok() && report.called_provider == active &&
        report.native_info.has_value() == active &&
        report.native_info.value_or(0) == 0 &&
        report.output_validity == asc::LapackOutputValidity::kComplete &&
        report.factor_family == asc::LapackFactorFamily::kRook &&
        !report.native_argument.has_value() &&
        !report.diagnostic_index.has_value() && std::isfinite(condition[1]) &&
        std::abs(condition[1] - expected) <=
            32 * std::numeric_limits<Real>::epsilon() *
                std::max(condition[1], expected) &&
        condition.front() == Real{-101} && condition.back() == Real{-103} &&
        EqualBytes(a.data.data(), saved_a.data(), sizeof(saved_a)) &&
        EqualBytes(e.data(), saved_e.data(), sizeof(e)) && pivots == saved_p &&
        scratch.Guards(workspace);
    passed = okay && passed;
    ++cases;
    std::printf(
        "installed RK condition n=%zu he=%d tri=%d layout=%d blocked=%d "
        "exponent=%d singular=%d zero=%d pass=%d\n",
        N, static_cast<int>(hermitian), static_cast<int>(triangle),
        static_cast<int>(layout), static_cast<int>(blocked), exponent,
        static_cast<int>(singular), static_cast<int>(zero),
        static_cast<int>(okay));
  }
  return passed;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool hermitian,
         int& cases) {
  using Real = asc::DenseBlasRealType<T>;
  bool passed = true;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto layout :
         {installed_internal::kColumn, installed_internal::kRow}) {
      for (const bool blocked : {false, true}) {
        passed = Check<T, 0>(provider, hermitian, triangle, layout, blocked, 0,
                             false, cases) &&
                 passed;
        passed = Check<T, 1>(provider, hermitian, triangle, layout, blocked, 0,
                             false, cases) &&
                 passed;
        passed = Check<T, 2>(provider, hermitian, triangle, layout, blocked, 0,
                             false, cases) &&
                 passed;
        passed = Check<T, 3>(provider, hermitian, triangle, layout, blocked, 0,
                             false, cases) &&
                 passed;
        passed = Check<T, 7>(provider, hermitian, triangle, layout, blocked, 0,
                             false, cases) &&
                 passed;
        passed = Check<T, 67>(provider, hermitian, triangle, layout, blocked, 0,
                              false, cases) &&
                 passed;
        passed = Check<T, 1>(provider, hermitian, triangle, layout, blocked, 0,
                             true, cases) &&
                 passed;
        passed = Check<T, 7>(provider, hermitian, triangle, layout, blocked, 0,
                             true, cases) &&
                 passed;
        passed = Check<T, 67>(provider, hermitian, triangle, layout, blocked, 0,
                              true, cases) &&
                 passed;
        for (const int exponent :
             {std::numeric_limits<Real>::max_exponent < 200 ? -100 : -800,
              std::numeric_limits<Real>::max_exponent < 200 ? 100 : 800}) {
          passed = Check<T, 7>(provider, hermitian, triangle, layout, blocked,
                               exponent, false, cases) &&
                   passed;
          passed = Check<T, 67>(provider, hermitian, triangle, layout, blocked,
                                exponent, false, cases) &&
                   passed;
        }
      }
    }
  }
  return passed;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  bool passed = Run<float>(provider, false, cases);
  passed = Run<double>(provider, false, cases) && passed;
  passed = Run<std::complex<float>>(provider, false, cases) && passed;
  passed = Run<std::complex<double>>(provider, false, cases) && passed;
  passed = Run<std::complex<float>>(provider, true, cases) && passed;
  passed = Run<std::complex<double>>(provider, true, cases) && passed;
  std::printf("installed RK condition cases=%d passed=%d\n", cases,
              static_cast<int>(passed));
  return passed ? 0 : 1;
}
