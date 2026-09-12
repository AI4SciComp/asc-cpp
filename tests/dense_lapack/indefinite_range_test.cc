#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_prototypes.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_indefinite_test;
using support::Take;
using support::TestContext;

template <typename T, bool Hermitian>
struct Native;
template <>
struct Native<float, false> {
  static constexpr auto kTrf = LAPACK_ssytrf_base;
  static constexpr auto kTf2 = ssytf2_;
  static constexpr auto kTrs = LAPACK_ssytrs_base;
};
template <>
struct Native<double, false> {
  static constexpr auto kTrf = LAPACK_dsytrf_base;
  static constexpr auto kTf2 = dsytf2_;
  static constexpr auto kTrs = LAPACK_dsytrs_base;
};
template <bool Hermitian>
struct Native<std::complex<float>, Hermitian> {
  static constexpr auto kTrf =
      Hermitian ? LAPACK_chetrf_base : LAPACK_csytrf_base;
  static constexpr auto kTf2 = Hermitian ? chetf2_ : csytf2_;
  static constexpr auto kTrs =
      Hermitian ? LAPACK_chetrs_base : LAPACK_csytrs_base;
};
template <bool Hermitian>
struct Native<std::complex<double>, Hermitian> {
  static constexpr auto kTrf =
      Hermitian ? LAPACK_zhetrf_base : LAPACK_zsytrf_base;
  static constexpr auto kTf2 = Hermitian ? zhetf2_ : zsytf2_;
  static constexpr auto kTrs =
      Hermitian ? LAPACK_zhetrs_base : LAPACK_zsytrs_base;
};

template <typename T>
bool SameValue(T left, T right) {
  const auto a = support::ToWide(left);
  const auto b = support::ToWide(right);
  auto component = [](long double x, long double y) {
    return x == y || (std::isnan(x) && std::isnan(y));
  };
  return component(a.real(), b.real()) && component(a.imag(), b.imag());
}

template <typename T>
bool Finite(T value) {
  const auto wide = support::ToWide(value);
  return std::isfinite(wide.real()) && std::isfinite(wide.imag());
}

template <typename T, bool Hermitian>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  Real scale;
  std::array<T, 5000> a{};
  std::array<T, 5000> original{};
  std::array<asc::index_t, 72> pivots{};

  Fixture(int order, asc::DenseBlasTriangle selected,
          asc::DenseBlasLayout storage, Real value,
          bool ignored_diagonal = true)
      : n(order), triangle(selected), layout(storage), scale(value) {
    a.fill(support::Value<T>(-31, 17));
    pivots.fill(-37);
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        T entry = i == j ? T{scale} : T{};
        if (!Selected(i, j)) {
          entry = support::Value<T>(std::numeric_limits<Real>::quiet_NaN());
        }
        if constexpr (Hermitian) {
          if (i == j && ignored_diagonal) {
            entry.imag(std::numeric_limits<Real>::quiet_NaN());
          }
        }
        a[Index(i, j)] = entry;
      }
    }
    original = a;
  }
  [[nodiscard]] int Ld() const { return n + 2; }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == support::kUpper ? i <= j : i >= j;
  }
  [[nodiscard]] std::size_t Index(int i, int j) const {
    return 1U + static_cast<std::size_t>(
                    layout == support::kColumn ? j * Ld() + i : i * Ld() + j);
  }
  auto View() { return support::Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] auto ConstView() const {
    return support::Matrix(a, n, n, layout, Ld());
  }
  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i =
          layout == support::kColumn ? relative % Ld() : relative / Ld();
      const int j =
          layout == support::kColumn ? relative / Ld() : relative % Ld();
      if (k == 0 || i >= n || j >= n || !Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(
            test, support::EqualBytes(&a[k], &original[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -37);
    for (std::size_t k = static_cast<std::size_t>(n) + 1; k < pivots.size();
         ++k) {
      ASC_DENSE_TEST_EQ(test, pivots[k], -37);
    }
  }
};

template <typename T, bool Hermitian>
lapack_int DirectFactor(TestContext& test, Fixture<T, Hermitian>& direct,
                        bool blocked) {
  lapack_int n = direct.n;
  lapack_int lda = direct.Ld();
  const lapack_int lwork = 64 * n;
  char uplo = direct.triangle == support::kUpper ? 'U' : 'L';
  constexpr auto kGuard = std::numeric_limits<lapack_int>::max() - 19;
  std::array<lapack_int, 72> pivots;
  pivots.fill(kGuard);
  std::array<lapack_int, 3> info{kGuard, kGuard, kGuard};
  std::array<T, 4500> work;
  work.fill(T{-43});
  if (blocked) {
    Native<T, Hermitian>::kTrf(&uplo, &n, direct.a.data() + 1, &lda,
                               pivots.data() + 1, work.data() + 1, &lwork,
                               info.data() + 1, std::size_t{1});
  } else {
    Native<T, Hermitian>::kTf2(&uplo, &n, direct.a.data() + 1, &lda,
                               pivots.data() + 1, info.data() + 1,
                               std::size_t{1});
  }
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, pivots.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, work.front(), T{-43});
  const auto first_unused = blocked ? static_cast<std::size_t>(lwork) + 1 : 1U;
  for (std::size_t k = first_unused; k < work.size(); ++k) {
    ASC_DENSE_TEST_EQ(test, work[k], T{-43});
  }
  for (std::size_t k = static_cast<std::size_t>(n) + 1; k < pivots.size();
       ++k) {
    ASC_DENSE_TEST_EQ(test, pivots[k], kGuard);
  }
  for (int k = 0; k < n; ++k) {
    direct.pivots[static_cast<std::size_t>(k) + 1] =
        pivots[static_cast<std::size_t>(k) + 1];
  }
  direct.Guards(test);
  return info[1];
}

template <typename T, bool Hermitian>
void FactorCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Fixture<T, Hermitian>& sample, bool blocked,
                bool mathematical) {
  const auto pivot = support::Pivots(sample.pivots, sample.n);
  const auto plan = Take(support::QueryFactor(
      provider, sample.triangle, Hermitian, blocked, sample.View(), pivot));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return support::Factor(provider, sample.triangle, Hermitian, blocked,
                           sample.View(), pivot, plan, workspace, report);
  });
  // Direct LAPACK receives the same mathematical Hermitian input after the
  // documented ignored-diagonal normalization, with independent column storage.
  Fixture<T, Hermitian> direct(sample.n, sample.triangle, support::kColumn,
                               sample.scale, false);
  const auto info = DirectFactor(test, direct, blocked);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, info);
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      info == 0 ? asc::ErrorCode::kOk : asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, sample.pivots, direct.pivots);
  bool finite = true;
  bool exact = true;
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (!sample.Selected(i, j)) {
        continue;
      }
      const T value = sample.a[sample.Index(i, j)];
      ASC_DENSE_TEST_CHECK(test,
                           SameValue(value, direct.a[direct.Index(i, j)]));
      finite = Finite(value) && finite;
      exact = (value == (i == j ? T{sample.scale} : T{})) && exact;
    }
    exact = sample.pivots[static_cast<std::size_t>(j) + 1] == j + 1 && exact;
  }
  std::printf(
      "indefinite factor n=%d hermitian=%d blocked=%d triangle=%d layout=%d "
      "scale=%La info=%lld finite=%d exact=%d\n",
      sample.n, static_cast<int>(Hermitian), static_cast<int>(blocked),
      static_cast<int>(sample.triangle), static_cast<int>(sample.layout),
      static_cast<long double>(sample.scale), static_cast<long long>(info),
      static_cast<int>(finite), static_cast<int>(exact));
  if (mathematical) {
    // For scale*I, the exact classic factors are D=scale*I, U/L=I and identity
    // pivots. This oracle rejects every nonfinite coefficient explicitly.
    ASC_DENSE_TEST_EQ(test, info, 0);
    ASC_DENSE_TEST_CHECK(test, finite);
    ASC_DENSE_TEST_CHECK(test, exact);
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T, bool Hermitian>
void SolveMode(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const Fixture<T, Hermitian>& sample,
               const asc::ReferenceBunchKaufmanFactorView<T>& factor,
               asc::DenseBlasLayout layout, bool mathematical) {
  const T coefficient{sample.scale};
  std::array<T, 8> rhs;
  rhs.fill(T{-47});
  const std::size_t second = layout == support::kColumn ? 4 : 2;
  rhs[1] = coefficient;
  rhs[second] = coefficient;
  const auto before = rhs;
  const auto view = support::Matrix(rhs, 1, 2, layout, 3);
  const auto plan =
      Take(support::QuerySolve(provider, Hermitian, factor, view));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return support::Solve(provider, Hermitian, factor, view, plan, workspace,
                          report);
  });
  const char uplo = sample.triangle == support::kUpper ? 'U' : 'L';
  const lapack_int n = 1;
  const lapack_int count = 2;
  const lapack_int pivot = 1;
  constexpr auto kGuard = std::numeric_limits<lapack_int>::max() - 19;
  std::array<lapack_int, 3> info{kGuard, kGuard, kGuard};
  std::array<T, 4> direct{T{-47}, coefficient, coefficient, T{-47}};
  Native<T, Hermitian>::kTrs(&uplo, &n, &count, &coefficient, &n, &pivot,
                             direct.data() + 1, &n, info.data() + 1,
                             std::size_t{1});
  ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, info[1], 0);
  ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
  ASC_DENSE_TEST_EQ(test, direct.front(), T{-47});
  ASC_DENSE_TEST_EQ(test, direct.back(), T{-47});
  for (const std::size_t index : {std::size_t{1}, second}) {
    ASC_DENSE_TEST_CHECK(test,
                         SameValue(rhs[index], direct[index == 1 ? 1 : 2]));
    if (mathematical) {
      using Real = asc::DenseBlasRealType<T>;
      const auto x = support::ToWide(rhs[index]);
      const auto a = support::ToWide(coefficient);
      ASC_DENSE_TEST_CHECK(test, Finite(rhs[index]));
      ASC_DENSE_TEST_CHECK(test, std::abs(x - support::Wide{1}) <=
                                     64 * std::numeric_limits<Real>::epsilon());
      ASC_DENSE_TEST_CHECK(test, std::abs(a * x - a) / std::abs(a) <=
                                     64 * std::numeric_limits<Real>::epsilon());
    }
  }
  for (std::size_t k = 0; k < rhs.size(); ++k) {
    if (k != 1 && k != second) {
      ASC_DENSE_TEST_EQ(test, rhs[k], before[k]);
    }
  }
  const auto x = support::ToWide(rhs[1]);
  std::printf(
      "indefinite solve hermitian=%d triangle=%d factor_layout=%d "
      "rhs_layout=%d scale=%La X=(%La,%La)\n",
      static_cast<int>(Hermitian), static_cast<int>(sample.triangle),
      static_cast<int>(sample.layout), static_cast<int>(layout),
      static_cast<long double>(sample.scale), x.real(), x.imag());
  scratch.Guards(test, workspace);
}

template <typename T, bool Hermitian>
void SolveCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Fixture<T, Hermitian>& sample, bool blocked, bool mathematical) {
  const auto pivots = support::Pivots(sample.pivots, 1);
  const auto plan = Take(support::QueryFactor(
      provider, sample.triangle, Hermitian, blocked, sample.View(), pivots));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, support::Factor(provider, sample.triangle, Hermitian, blocked,
                            sample.View(), pivots, plan, workspace, report)
                .ok());
  const auto factor = Take(asc::ReferenceBunchKaufmanFactorView<T>::Create(
      provider, sample.ConstView(), sample.triangle,
      Hermitian ? support::kHermitian : support::kSymmetric,
      support::Raw(sample.pivots, 1), report));
  const auto saved = sample.a;
  const auto saved_pivots = sample.pivots;
  for (const auto layout : {support::kColumn, support::kRow}) {
    SolveMode(test, provider, sample, factor, layout, mathematical);
  }
  ASC_DENSE_TEST_CHECK(
      test, support::EqualBytes(sample.a.data(), saved.data(), sizeof(saved)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, saved_pivots);
  sample.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T, bool Hermitian = false>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         std::string_view mode) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array scales{2 * std::numeric_limits<Real>::min(),
                          std::numeric_limits<Real>::min() / 1024,
                          2 * std::numeric_limits<Real>::denorm_min(),
                          std::numeric_limits<Real>::max()};
  const bool solve = mode.starts_with("solve");
  const bool mathematical = mode.ends_with("mathematical");
  for (const Real scale : scales) {
    for (const auto triangle : {support::kUpper, support::kLower}) {
      for (const auto layout : {support::kColumn, support::kRow}) {
        for (const bool blocked : {false, true}) {
          if (solve) {
            Fixture<T, Hermitian> sample(1, triangle, layout, scale);
            SolveCase(test, provider, sample, blocked, mathematical);
          } else {
            for (const int n : {2, 67}) {
              Fixture<T, Hermitian> sample(n, triangle, layout, scale);
              FactorCase(test, provider, sample, blocked, mathematical);
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "factor_mathematical" && mode != "factor_fidelity" &&
      mode != "solve_mathematical" && mode != "solve_fidelity") {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
  if (scalar == "s") {
    Run<float>(test, provider, mode);
  } else if (scalar == "d") {
    Run<double>(test, provider, mode);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, mode);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, mode);
  } else if (scalar == "ch") {
    Run<std::complex<float>, true>(test, provider, mode);
  } else if (scalar == "zh") {
    Run<std::complex<double>, true>(test, provider, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
