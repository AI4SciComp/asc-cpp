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
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_solve_native.h"
#include "indefinite_rk_solve_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace support = asc_indefinite_rook_test;
using support::Take;
using support::TestContext;
template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  bool hermitian;
  bool paired;
  bool representable;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout b_layout;
  Real scale;
  std::array<T, 14> a{};
  std::array<T, 14> b{};
  std::array<T, 5> e{};
  std::array<T, 14> original_a{};
  std::array<T, 14> original_b{};
  std::array<asc::index_t, 5> pivots{};
  Sample(bool he, bool pair, bool exact, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout al, asc::DenseBlasLayout bl, Real value)
      : hermitian(he),
        paired(pair),
        representable(exact),
        triangle(tri),
        a_layout(al),
        b_layout(bl),
        scale(value) {
    a.fill(support::Value<T>(-79, 19));
    b.fill(support::Value<T>(-83, 23));
    pivots.fill(-89);
    e.fill(support::Value<T>(-91, 27));
    const T off = support::Value<T>(scale, paired ? scale : 0);
    for (int j = 0; j < N(); ++j) {
      for (int i = 0; i < N(); ++i) {
        if (Selected(i, j)) {
          T value_a{scale};
          if (paired) {
            value_a = i == j ? T{} : off;
          }
          a[AIndex(i, j)] = value_a;
        }
      }
    }
    for (int i = 0; i < N(); ++i) {
      T value_b{1};
      if (representable) {
        value_b = paired ? off : T{scale};
      }
      if constexpr (asc::DenseBlasComplex<T>) {
        if (paired && hermitian &&
            ((triangle == support::kUpper && i == 1) ||
             (triangle == support::kLower && i == 0))) {
          value_b = std::conj(value_b);
        }
      }
      b[BIndex(i)] = value_b;
    }
    original_a = a;
    original_b = b;
  }
  [[nodiscard]] int N() const { return paired ? 2 : 1; }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == support::kUpper ? i <= j : i >= j;
  }
  [[nodiscard]] std::size_t AIndex(int i, int j) const {
    return 1U + static_cast<std::size_t>(
                    a_layout == support::kColumn ? 4 * j + i : 4 * i + j);
  }
  [[nodiscard]] std::size_t BIndex(int i) const {
    return 1U +
           static_cast<std::size_t>(b_layout == support::kColumn ? i : 3 * i);
  }
  auto AView() { return support::Matrix(a, N(), N(), a_layout, 4); }
  [[nodiscard]] auto ConstAView() const {
    return support::Matrix(a, N(), N(), a_layout, 4);
  }
  auto BView() {
    return support::Matrix(b, N(), 1, b_layout,
                           b_layout == support::kColumn ? 4 : 3);
  }
  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      bool selected = false;
      for (int j = 0; j < N(); ++j) {
        for (int i = 0; i < N(); ++i) {
          selected = selected || (Selected(i, j) && k == AIndex(i, j));
        }
      }
      if (!selected) {
        ASC_DENSE_TEST_CHECK(
            test, support::EqualBytes(&a[k], &original_a[k], sizeof(T)));
      }
      bool rhs = false;
      for (int i = 0; i < N(); ++i) {
        rhs = rhs || k == BIndex(i);
      }
      if (!rhs) {
        ASC_DENSE_TEST_CHECK(
            test, support::EqualBytes(&b[k], &original_b[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, e.front(), support::Value<T>(-91, 27));
    for (std::size_t k = static_cast<std::size_t>(N()) + 1; k < e.size(); ++k) {
      ASC_DENSE_TEST_EQ(test, e[k], support::Value<T>(-91, 27));
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -89);
    for (std::size_t k = static_cast<std::size_t>(N()) + 1; k < pivots.size();
         ++k) {
      ASC_DENSE_TEST_EQ(test, pivots[k], -89);
    }
  }
};
template <typename T>
void Fidelity(TestContext& test, const Sample<T>& sample) {
  std::array<T, 6> a{};
  std::array<T, 4> b{};
  const auto extra = sample.e;
  const auto original_e = extra;
  std::array<lapack_int, 4> pivots{};
  const T guard = support::Value<T>(-97, 29);
  a.fill(guard);
  b.fill(guard);
  pivots.fill(-101);
  for (int j = 0; j < sample.N(); ++j) {
    pivots[j + 1] = static_cast<lapack_int>(sample.pivots[j + 1]);
    for (int i = 0; i < sample.N(); ++i) {
      if (sample.Selected(i, j)) {
        a[1 + j * sample.N() + i] = sample.a[sample.AIndex(i, j)];
      }
    }
  }
  for (int i = 0; i < sample.N(); ++i) {
    b[i + 1] = sample.original_b[sample.BIndex(i)];
  }
  const auto original_a = a;
  const auto original_pivots = pivots;
  const char uplo = sample.triangle == support::kUpper ? 'U' : 'L';
  const lapack_int n = sample.N();
  const lapack_int nrhs = 1;
  std::array<lapack_int, 3> info{-103, std::numeric_limits<lapack_int>::min(),
                                 -103};
  asc_rk_solve_test::Native(sample.hermitian, &uplo, &n, &nrhs, a.data() + 1,
                            &n, extra.data() + 1, pivots.data() + 1,
                            b.data() + 1, &n, info.data() + 1);
  ASC_DENSE_TEST_EQ(test, info, (std::array<lapack_int, 3>{-103, 0, -103}));
  ASC_DENSE_TEST_CHECK(
      test, support::EqualBytes(a.data(), original_a.data(), sizeof(a)));
  ASC_DENSE_TEST_EQ(test, pivots, original_pivots);
  ASC_DENSE_TEST_EQ(test, b.front(), guard);
  ASC_DENSE_TEST_CHECK(
      test,
      support::EqualBytes(extra.data(), original_e.data(), sizeof(extra)));
  for (std::size_t i = static_cast<std::size_t>(n) + 1; i < b.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, b[i], guard);
  }
  for (int i = 0; i < sample.N(); ++i) {
    ASC_DENSE_TEST_CHECK(test, support::EqualBytes(&sample.b[sample.BIndex(i)],
                                                   &b[i + 1], sizeof(T)));
  }
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool mathematical) {
  const auto factor_plan = Take(asc_rk_test::QueryFactor(
      provider, sample.triangle, sample.hermitian, false, sample.AView(),
      asc_rk_test::OffDiagonal(sample.e, sample.N()),
      support::Pivots(sample.pivots, sample.N())));
  support::Scratch<T> factor_scratch;
  const auto factor_workspace = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  const auto factored = asc_rk_test::Factor(
      provider, sample.triangle, sample.hermitian, false, sample.AView(),
      asc_rk_test::OffDiagonal(sample.e, sample.N()),
      support::Pivots(sample.pivots, sample.N()), factor_plan, factor_workspace,
      factor_report);
  ASC_DENSE_TEST_CHECK(test, factored.ok());
  ASC_DENSE_TEST_EQ(test, factor_report.native_info.value_or(-999), 0);
  const auto before_a = sample.a;
  const auto before_pivots = sample.pivots;
  const auto before_e = sample.e;
  const auto extra = asc_rk_solve_test::OffDiagonal(sample.e, sample.N());
  const auto raw = support::Raw(sample.pivots, sample.N());
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return asc_rk_solve_test::Query(provider, sample.triangle, sample.hermitian,
                                    sample.ConstAView(), extra, raw,
                                    sample.BView());
  }));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc_rk_solve_test::Solve(provider, sample.triangle, sample.hermitian,
                                    sample.ConstAView(), extra, raw,
                                    sample.BView(), plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(
      test,
      support::EqualBytes(sample.a.data(), before_a.data(), sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.pivots, before_pivots);
  ASC_DENSE_TEST_CHECK(
      test,
      support::EqualBytes(sample.e.data(), before_e.data(), sizeof(before_e)));
  Fidelity(test, sample);
  bool finite = true;
  bool accurate = true;
  for (int i = 0; i < sample.N(); ++i) {
    const auto value = support::ToWide(sample.b[sample.BIndex(i)]);
    finite =
        std::isfinite(value.real()) && std::isfinite(value.imag()) && finite;
    accurate =
        (std::abs(value - support::Wide{1}) <=
         64 * std::numeric_limits<typename Sample<T>::Real>::epsilon()) &&
        accurate;
  }
  if (!sample.representable) {
    ASC_DENSE_TEST_CHECK(
        test, 1 / static_cast<long double>(sample.scale) >
                  std::numeric_limits<typename Sample<T>::Real>::max());
  }
  if (mathematical && sample.representable) {
    ASC_DENSE_TEST_CHECK(test, finite);
    ASC_DENSE_TEST_CHECK(test, accurate);
  }
  std::printf(
      "RK solve range n=%d he=%d tri=%d A_layout=%d B_layout=%d scale=%La "
      "INFO=%lld finite=%d accurate=%d exact-representable=%d\n",
      sample.N(), static_cast<int>(sample.hermitian),
      static_cast<int>(sample.triangle), static_cast<int>(sample.a_layout),
      static_cast<int>(sample.b_layout), static_cast<long double>(sample.scale),
      static_cast<long long>(report.native_info.value_or(-999)),
      static_cast<int>(finite), static_cast<int>(accurate),
      static_cast<int>(sample.representable));
  sample.Guards(test);
  scratch.Guards(test, workspace);
  factor_scratch.Guards(test, factor_workspace);
}
template <typename T>
int Run(bool hermitian, bool mathematical) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  int representable = 0;
  const auto tiny = std::numeric_limits<Real>::min();
  const auto large = std::numeric_limits<Real>::max();
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto al : {support::kColumn, support::kRow}) {
      for (const auto bl : {support::kColumn, support::kRow}) {
        for (const bool paired : {false, true}) {
          for (const auto scale : {tiny / 2, tiny / 8, tiny, Real{1}, large / 4,
                                   large * Real{0.75}}) {
            Case(test, provider,
                 Sample<T>(hermitian, paired, true, triangle, al, bl, scale),
                 mathematical);
            ++cases;
            ++representable;
          }
        }
        for (const auto scale : {tiny / 8}) {
          Case(test, provider,
               Sample<T>(hermitian, false, false, triangle, al, bl, scale),
               mathematical);
          ++cases;
        }
      }
    }
  }
  std::printf("RK solve range cases=%d representable=%d nonrepresentable=%d\n",
              cases, representable, cases - representable);
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
  const bool mathematical = mode == "mathematical";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false, mathematical);
  }
  if (scalar == "d") {
    return Run<double>(false, mathematical);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, mathematical);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, mathematical);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, mathematical);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, mathematical);
  }
  return 2;
}
