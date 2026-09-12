#include <algorithm>
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
#include "indefinite_inverse_native.h"
#include "indefinite_inverse_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_indefinite_test;
using support::TestContext;
using support::Wide;

template <typename Real>
bool Same(Real actual, Real expected) {
  return (std::isnan(actual) && std::isnan(expected)) ||
         (actual == expected && std::signbit(actual) == std::signbit(expected));
}
template <typename T>
bool Fidelity(T actual, T expected) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return Same(actual.real(), expected.real()) &&
           Same(actual.imag(), expected.imag());
  } else {
    return Same(actual, expected);
  }
}

int Order(int mode) {
  if (mode == 0) {
    return 1;
  }
  return mode == 1 ? 7 : 2;
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  bool paired;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 80> a{};
  std::array<Wide, 49> original{};
  std::array<Wide, 49> expected{};
  std::array<asc::index_t, 10> pivots{};
  Sample(int mode, bool he, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout storage, Real scale)
      : n(Order(mode)),
        paired(mode == 2),
        hermitian(he),
        triangle(tri),
        layout(storage) {
    a.fill(support::Value<T>(-431, 17));
    pivots.fill(-433);
    const T coefficient = paired ? support::Value<T>(scale, scale) : T{scale};
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        Wide value{};
        if ((paired && i != j) || (!paired && i == j)) {
          value = paired ? support::ToWide(coefficient) : Wide{scale};
        }
        original[i * n + j] = (hermitian && i < j) ? std::conj(value) : value;
        expected[i * n + j] = !paired && i == j ? Wide{1} / value : Wide{};
        if (Selected(i, j)) {
          a[Offset(i, j)] = support::Value<T>(original[i * n + j].real(),
                                              original[i * n + j].imag());
        }
      }
    }
    // Complete all original entries before forming the transposed reciprocal.
    if (paired) {
      expected[1] = Wide{1} / original[2];
      expected[2] = Wide{1} / original[1];
    }
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == support::kUpper ? i <= j : i >= j;
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + static_cast<std::size_t>(layout == support::kColumn
                                             ? j * (n + 2) + i
                                             : i * (n + 2) + j);
  }
  auto View() { return support::Matrix(a, n, n, layout, n + 2); }
  [[nodiscard]] bool Representable() const {
    const long double limit = std::numeric_limits<Real>::max();
    return std::all_of(expected.begin(), expected.end(), [limit](Wide x) {
      return std::isfinite(x.real()) && std::isfinite(x.imag()) &&
             std::abs(x.real()) <= limit && std::abs(x.imag()) <= limit;
    });
  }
};

template <typename T>
void Mathematical(TestContext& test, const Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  const int n = sample.n;
  const long double epsilon = std::numeric_limits<Real>::epsilon();
  const long double underflow = std::numeric_limits<Real>::denorm_min();
  std::array<Wide, 49> inverse{};
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      Wide value =
          sample.Selected(i, j)
              ? support::ToWide(sample.a[sample.Offset(i, j)])
              : support::Adjoint(support::ToWide(sample.a[sample.Offset(j, i)]),
                                 sample.hermitian);
      inverse[i * n + j] = value;
      ASC_DENSE_TEST_CHECK(
          test, std::isfinite(value.real()) && std::isfinite(value.imag()));
      ASC_DENSE_TEST_CHECK(
          test, std::abs(value - sample.expected[i * n + j]) <=
                    64 * epsilon * std::abs(sample.expected[i * n + j]) +
                        4 * underflow);
    }
  }
  long double residual = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      Wide left{};
      Wide right{};
      for (int k = 0; k < n; ++k) {
        left += sample.original[i * n + k] * inverse[k * n + j];
        right += inverse[i * n + k] * sample.original[k * n + j];
      }
      const long double error =
          std::max(std::abs(left - Wide{i == j ? 1.L : 0.L}),
                   std::abs(right - Wide{i == j ? 1.L : 0.L}));
      ASC_DENSE_TEST_CHECK(test, std::isfinite(error));
      residual = std::max(residual, error);
    }
  }
  ASC_DENSE_TEST_CHECK(test, residual <= 128 * n * epsilon);
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool fidelity) {
  const auto initial = sample.a;
  auto a = sample.View();
  const auto pivot_view = support::Pivots(sample.pivots, sample.n);
  const auto factor_plan = support::Take(support::QueryFactor(
      provider, sample.triangle, sample.hermitian, false, a, pivot_view));
  support::Scratch<T> factor_scratch;
  const auto factor_work = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  const auto factor_status =
      support::Factor(provider, sample.triangle, sample.hermitian, false, a,
                      pivot_view, factor_plan, factor_work, factor_report);
  std::printf("factor stage info=%lld status=%d\n",
              static_cast<long long>(factor_report.native_info.value_or(-999)),
              static_cast<int>(factor_status.code()));
  ASC_DENSE_TEST_CHECK(test, factor_status.ok());
  if (!factor_status.ok()) {
    return;
  }
  const auto pivots_before = sample.pivots;
  const auto raw = support::Raw(sample.pivots, sample.n);
  const auto plan = support::Take(support::WithoutAllocation(test, [&] {
    return asc_inverse_test::Query(provider, sample.triangle, sample.hermitian,
                                   a, raw);
  }));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  std::array<T, 49> native{};
  std::array<T, 14> work{};
  std::array<lapack_int, 7> pivots{};
  for (int j = 0; j < sample.n; ++j) {
    pivots[j] = static_cast<lapack_int>(sample.pivots[j + 1]);
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        native[j * sample.n + i] = sample.a[sample.Offset(i, j)];
      }
    }
  }
  lapack_int n = sample.n;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  char triangle = sample.triangle == support::kUpper ? 'U' : 'L';
  asc_inverse_test::Native(sample.hermitian, &triangle, &n, native.data(), &n,
                           pivots.data(), work.data(), &info);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc_inverse_test::Inverse(provider, sample.triangle,
                                     sample.hermitian, a, raw, plan, workspace,
                                     report);
  });
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, info, 0);
  if (fidelity) {
    for (int j = 0; j < sample.n; ++j) {
      for (int i = 0; i < sample.n; ++i) {
        if (sample.Selected(i, j)) {
          ASC_DENSE_TEST_CHECK(test, Fidelity(sample.a[sample.Offset(i, j)],
                                              native[j * sample.n + i]));
        }
      }
    }
  } else if (sample.Representable()) {
    Mathematical(test, sample);
  }
  ASC_DENSE_TEST_EQ(test, sample.pivots, pivots_before);
  for (std::size_t k = 0; k < sample.a.size(); ++k) {
    const int relative = static_cast<int>(k) - 1;
    const int i = sample.layout == support::kColumn ? relative % (sample.n + 2)
                                                    : relative / (sample.n + 2);
    const int j = sample.layout == support::kColumn ? relative / (sample.n + 2)
                                                    : relative % (sample.n + 2);
    if (k == 0 || i >= sample.n || j >= sample.n || !sample.Selected(i, j)) {
      ASC_DENSE_TEST_CHECK(
          test, support::EqualBytes(&sample.a[k], &initial[k], sizeof(T)));
    }
  }
  scratch.Guards(test, workspace);
  factor_scratch.Guards(test, factor_work);
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  const auto provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  int representable = 0;
  const Real small = std::numeric_limits<Real>::min();
  const Real large = std::numeric_limits<Real>::max();
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto layout : {support::kColumn, support::kRow}) {
      for (const int mode : {0, 1, 2}) {
        for (const Real scale : {2 * small, small / 2, small / 8, Real{1},
                                 large / 4, large * Real{0.75}}) {
          Sample<T> sample(mode, hermitian, triangle, layout, scale);
          const bool finite = sample.Representable();
          std::printf(
              "inverse range mode=%d n=%d he=%d tri=%d layout=%d scale=%Lg "
              "exact-representable=%d\n",
              mode, sample.n, static_cast<int>(hermitian),
              static_cast<int>(triangle), static_cast<int>(layout),
              static_cast<long double>(scale), static_cast<int>(finite));
          Case(test, provider, sample, fidelity);
          ++cases;
          representable += static_cast<int>(finite);
        }
      }
    }
  }
  std::printf(
      "classic inverse range cases=%d representable=%d nonrepresentable=%d "
      "fidelity=%d\n",
      cases, representable, cases - representable, static_cast<int>(fidelity));
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
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
