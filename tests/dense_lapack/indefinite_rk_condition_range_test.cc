#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_condition_native.h"
#include "indefinite_rk_condition_test_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
using asc_rk_condition_test::Condition;
using asc_rk_condition_test::Query;
using asc_rk_condition_test::Scratch;
using base::TestContext;
template <class T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  Real scale;
  std::array<T, 16> a{};
  std::array<T, 5> e{};
  std::array<asc::index_t, 5> pivots{};
  Sample(bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout storage,
         bool pair, Real value)
      : n(pair ? 2 : 1),
        hermitian(he),
        triangle(tri),
        layout(storage),
        scale(value) {
    a.fill(base::Value<T>(-71, 13));
    e.fill(base::Value<T>(-73, 17));
    pivots.fill(-79);
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (Selected(i, j)) {
          const T diagonal = pair ? T{} : T{scale};
          a[Offset(i, j)] = i == j ? diagonal : base::Value<T>(scale, scale);
        }
      }
    }
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == base::kUpper ? i <= j : i >= j;
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + static_cast<std::size_t>(layout == base::kColumn
                                             ? j * (n + 2) + i
                                             : i * (n + 2) + j);
  }
  auto View() { return base::Matrix(a, n, n, layout, n + 2); }
  [[nodiscard]] auto ConstView() const {
    return base::Matrix(a, n, n, layout, n + 2);
  }
  [[nodiscard]] long double ExactNorm() const {
    return n == 2 && asc::DenseBlasComplex<T>
               ? std::hypot(static_cast<long double>(scale),
                            static_cast<long double>(scale))
               : static_cast<long double>(scale);
  }
  void PoisonIgnored() {
    const int unused = n == 1 || triangle == base::kUpper ? 0 : 1;
    e[static_cast<std::size_t>(unused) + 1] =
        base::Value<T>(std::numeric_limits<Real>::quiet_NaN(), 19);
  }
};
template <class T>
void Fidelity(TestContext& test, const Sample<T>& sample,
              asc::DenseBlasRealType<T> norm,
              asc::DenseBlasRealType<T> actual) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 101;
  std::array<lapack_int, 3> n{kGuard, sample.n, kGuard};
  std::array<lapack_int, 3> lda{kGuard, sample.n + 1, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 4> piv;
  piv.fill(kGuard);
  std::array<lapack_int, 4> iw;
  iw.fill(kGuard);
  std::array<T, 10> a;
  a.fill(base::Value<T>(-61, 13));
  std::array<T, 6> work;
  work.fill(base::Value<T>(-67, 17));
  auto e = sample.e;
  for (int j = 0; j < sample.n; ++j) {
    piv[static_cast<std::size_t>(j) + 1] =
        static_cast<lapack_int>(sample.pivots[static_cast<std::size_t>(j) + 1]);
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        a[1U +
          static_cast<std::size_t>(j) * static_cast<std::size_t>(sample.n + 1) +
          static_cast<std::size_t>(i)] = sample.a[sample.Offset(i, j)];
      }
    }
  }
  const auto before_a = a;
  const auto before_e = e;
  const auto before_p = piv;
  const auto before_iw = iw;
  const auto before_w = work;
  std::array<Real, 3> native_norm{Real{113}, norm, Real{117}};
  const auto before_norm = native_norm;
  std::array<Real, 3> condition{Real{127}, Real{-1}, Real{131}};
  std::array<char, 3> triangle{'a', sample.triangle == base::kUpper ? 'U' : 'L',
                               'z'};
  const auto old_triangle = triangle;
  asc_rk_condition_test::Native(sample.hermitian, &triangle[1], &n[1], &a[1],
                                &lda[1], e.data() + 1, piv.data() + 1,
                                &native_norm[1], &condition[1], work.data() + 1,
                                iw.data() + 1, &info[1]);
  const std::array<lapack_int, 3> expected_info{kGuard, 0, kGuard};
  const std::array<lapack_int, 3> expected_n{kGuard, sample.n, kGuard};
  const std::array<lapack_int, 3> expected_lda{kGuard, sample.n + 1, kGuard};
  ASC_DENSE_TEST_EQ(test, info, expected_info);
  ASC_DENSE_TEST_EQ(test, n, expected_n);
  ASC_DENSE_TEST_EQ(test, lda, expected_lda);
  ASC_DENSE_TEST_EQ(test, triangle, old_triangle);
  ASC_DENSE_TEST_EQ(test, native_norm, before_norm);
  ASC_DENSE_TEST_EQ(test, condition.front(), Real{127});
  ASC_DENSE_TEST_EQ(test, condition.back(), Real{131});
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(&actual, &condition[1], sizeof(Real)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(a.data(), before_a.data(), sizeof(a)) &&
                base::EqualBytes(e.data(), before_e.data(), sizeof(e)) &&
                piv == before_p);
  for (std::size_t k = 0; k < work.size(); ++k) {
    if (k == 0 || k > 2U * static_cast<std::size_t>(sample.n)) {
      ASC_DENSE_TEST_CHECK(test,
                           base::EqualBytes(&work[k], &before_w[k], sizeof(T)));
    }
  }
  for (std::size_t k = 0; k < iw.size(); ++k) {
    if (asc::DenseBlasComplex<T> || k == 0 ||
        k > static_cast<std::size_t>(sample.n)) {
      ASC_DENSE_TEST_EQ(test, iw[k], before_iw[k]);
    }
  }
}
template <class T>
void Factor(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Sample<T>& sample, bool blocked) {
  const auto extra = asc_rk_test::OffDiagonal(sample.e, sample.n);
  const auto pivots = base::Pivots(sample.pivots, sample.n);
  const auto plan = base::Take(
      asc_rk_test::QueryFactor(provider, sample.triangle, sample.hermitian,
                               blocked, sample.View(), extra, pivots));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test,
      asc_rk_test::Factor(provider, sample.triangle, sample.hermitian, blocked,
                          sample.View(), extra, pivots, plan, workspace, report)
          .ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(i, j)],
                          sample.n == 1 ? T{sample.scale} : T{});
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, sample.pivots[1], sample.n == 1 ? 1 : -1);
  if (sample.n == 2) {
    ASC_DENSE_TEST_EQ(test, sample.pivots[2], -2);
    ASC_DENSE_TEST_EQ(test, sample.e[sample.triangle == base::kUpper ? 2U : 1U],
                      base::Value<T>(sample.scale, sample.scale));
  }
  scratch.Guards(test, workspace);
  sample.PoisonIgnored();
}
template <class T>
void ActiveEstimate(TestContext& test, const Sample<T>& sample,
                    asc::DenseBlasRealType<T> norm,
                    asc::DenseBlasRealType<T> condition, bool fidelity,
                    const asc::Status& status,
                    const asc::LapackReport& report) {
  using Real = asc::DenseBlasRealType<T>;
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
  ASC_DENSE_TEST_EQ(test, status.ok(), std::isfinite(condition));
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    std::isfinite(condition)
                        ? asc::LapackOutcome::kSuccess
                        : asc::LapackOutcome::kAccuracyWarning);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    std::isfinite(condition)
                        ? asc::LapackOutputValidity::kComplete
                        : asc::LapackOutputValidity::kDocumentedPartial);
  if (fidelity) {
    Fidelity(test, sample, norm, condition);
  } else {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
    ASC_DENSE_TEST_CHECK(test, std::abs(condition - Real{1}) <=
                                   64 * std::numeric_limits<Real>::epsilon());
  }
}
template <class T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool blocked, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  Factor(test, provider, sample, blocked);
  const auto before_a = sample.a;
  const auto before_e = sample.e;
  const auto before_p = sample.pivots;
  const auto factors = sample.ConstView();
  const asc::DenseBlasVectorView<const T> extra(
      asc_rk_test::OffDiagonal(sample.e, sample.n));
  const auto pivots = base::Raw(sample.pivots, sample.n);
  const bool representable =
      sample.ExactNorm() <= std::numeric_limits<Real>::max();
  const Real norm = static_cast<Real>(sample.ExactNorm());
  Real condition = -13;
  // Positive norms share a workspace plan. Infinity is checked separately as
  // an unrepresentable true norm, never substituted into a mathematical oracle.
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, sample.triangle, sample.hermitian, factors, extra,
                 pivots, representable ? norm : Real{1}, condition);
  }));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto before_scratch = scratch;
  if (!representable) {
    const auto rejected = base::WithoutAllocation(test, [&] {
      return Query(provider, sample.triangle, sample.hermitian, factors, extra,
                   pivots, norm, condition);
    });
    ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return Condition(provider, sample.triangle, sample.hermitian, factors,
                     extra, pivots, norm, condition, plan, workspace, report);
  });
  if (representable) {
    ActiveEstimate(test, sample, norm, condition, fidelity, status, report);
  } else {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, condition, Real{-13});
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before_scratch.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, before_scratch.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, before_scratch.pivot);
  }
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value() &&
                                 !report.native_argument.has_value());
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(sample.a.data(), before_a.data(), sizeof(before_a)) &&
          base::EqualBytes(sample.e.data(), before_e.data(),
                           sizeof(before_e)) &&
          sample.pivots == before_p);
  scratch.Guards(test, workspace);
  std::printf(
      "RK condition range n=%d he=%d tri=%d layout=%d blocked=%d scale=%La "
      "ANORM=%La RCOND=%La representable_norm=%d called=%d\n",
      sample.n, static_cast<int>(sample.hermitian),
      static_cast<int>(sample.triangle), static_cast<int>(sample.layout),
      static_cast<int>(blocked), static_cast<long double>(sample.scale),
      static_cast<long double>(norm), static_cast<long double>(condition),
      static_cast<int>(representable),
      static_cast<int>(report.called_provider));
}
template <class T>
int Run(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const Real low = std::numeric_limits<Real>::min();
  const Real high = std::numeric_limits<Real>::max();
  int cases = 0;
  int admitted = 0;
  for (const auto tri : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const bool blocked : {false, true}) {
        for (const bool pair : {false, true}) {
          for (const Real scale :
               {low / Real{8}, low / Real{2}, low, Real{1}, high / Real{8},
                high / Real{4}, high / Real{2}, Real{0.75} * high}) {
            Sample<T> sample(hermitian, tri, layout, pair, scale);
            admitted +=
                sample.ExactNorm() <= std::numeric_limits<Real>::max() ? 1 : 0;
            Case(test, provider, sample, blocked, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf(
      "RK condition range cases=%d representable_norm=%d "
      "nonrepresentable_norm=%d\n",
      cases, admitted, cases - admitted);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false, fidelity);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity);
  }
  return 2;
}
