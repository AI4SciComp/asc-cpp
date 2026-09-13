#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rook_inverse_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
using asc_indefinite_rook_test::Adjoint;
using asc_indefinite_rook_test::EqualBytes;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Matrix;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::Raw;
using asc_indefinite_rook_test::Scratch;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::ToWide;
using asc_indefinite_rook_test::Value;
using asc_indefinite_rook_test::Wide;
using asc_indefinite_rook_test::WithoutAllocation;
template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 5000> a{};
  std::array<T, 5000> original{};
  std::array<asc::index_t, 72> pivots{};
  std::array<Wide, 4489> full{};

  Sample(int order, bool he, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout storage, int exponent, bool singular)
      : n(order), hermitian(he), triangle(tri), layout(storage) {
    a.fill(Value<T>(-503, 29));
    pivots.fill(-509);
    const Real scale = std::ldexp(Real{1}, exponent);
    auto permutation = [order](int i) {
      if (order > 3) {
        if (i == 1) {
          return order - 1;
        }
        if (i == order - 1) {
          return 1;
        }
      }
      return i;
    };
    for (int j = 0; j < n; ++j) {
      for (int i = j; i < n; ++i) {
        const int p = permutation(i);
        const int q = permutation(j);
        const int low = std::min(p, q);
        const int high = std::max(p, q);
        T value{};
        if (p == q) {
          value = Value<T>(p % 3 == 0 || p == n - 1 ? 4 : 0,
                           hermitian ? 0 : 0.125L);
        } else if (low % 3 == 1 && high == low + 1) {
          value = Value<T>(2, 0.5L);
        } else {
          value = Value<T>(static_cast<long double>((p + q) % 3 - 1) / 128,
                           static_cast<long double>((p + q) % 5 - 2) / 256);
        }
        if (order == 3) {
          // Force the rook search to leave the current column and choose an
          // internal 2x2 block, requiring BOTH independent interchanges.
          const bool strong = triangle == kUpper ? high == 1 : low == 1;
          long double magnitude = 1;
          if (strong) {
            magnitude = 4;
          } else if (high - low == 2) {
            magnitude = 0.5L;
          }
          value = p == q ? T{} : Value<T>(magnitude, 0.125L);
        }
        value *= scale;
        const Wide wide = ToWide(value);
        full[i * n + j] = wide;
        full[j * n + i] = Adjoint(wide, hermitian);
      }
    }
    if (singular) {
      for (int i = 0; i < n; ++i) {
        full[i * n + n - 1] = Wide{};
        full[(n - 1) * n + i] = Wide{};
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (Selected(i, j)) {
          a[Offset(i, j)] =
              Value<T>(full[i * n + j].real(), full[i * n + j].imag());
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian && i == j) {
              a[Offset(i, j)].imag(std::numeric_limits<Real>::quiet_NaN());
            }
          }
        } else {
          a[Offset(i, j)] = Value<T>(std::numeric_limits<Real>::quiet_NaN());
        }
      }
    }
    original = a;
  }

  [[nodiscard]] int Ld() const { return n + 2; }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + (layout == kColumn ? static_cast<std::size_t>(j) * Ld() + i
                                   : static_cast<std::size_t>(i) * Ld() + j);
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == kUpper ? i <= j : i >= j;
  }
  auto View() { return Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] auto ConstView() const { return Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] Wide Entry(int i, int j) const {
    return ToWide(a[Offset(i, j)]);
  }

  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      const auto relative = static_cast<int>(k) - 1;
      const int i = layout == kColumn ? relative % Ld() : relative / Ld();
      const int j = layout == kColumn ? relative / Ld() : relative % Ld();
      if (k == 0 || i >= n || j >= n || !Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test, EqualBytes(&a[k], &original[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -509);
    for (std::size_t i = static_cast<std::size_t>(n) + 1; i < pivots.size();
         ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[i], -509);
    }
  }
};

// Independent long-double Gauss-Jordan elimination on the original full matrix.
// It uses row partial pivoting, not stored rook factors or a provider solve.
std::array<Wide, 4489> Oracle(TestContext& test, std::array<Wide, 4489> a,
                              int n) {
  std::array<Wide, 4489> result{};
  for (int i = 0; i < n; ++i) {
    result[i * n + i] = 1;
  }
  for (int k = 0; k < n; ++k) {
    int pivot = k;
    for (int i = k + 1; i < n; ++i) {
      if (std::abs(a[i * n + k]) > std::abs(a[pivot * n + k])) {
        pivot = i;
      }
    }
    ASC_DENSE_TEST_CHECK(test, std::abs(a[pivot * n + k]) > 0);
    if (a[pivot * n + k] == Wide{}) {
      return result;
    }
    for (int j = 0; j < n; ++j) {
      std::swap(a[k * n + j], a[pivot * n + j]);
      std::swap(result[k * n + j], result[pivot * n + j]);
    }
    const Wide diagonal = a[k * n + k];
    for (int j = 0; j < n; ++j) {
      a[k * n + j] /= diagonal;
      result[k * n + j] /= diagonal;
    }
    for (int i = 0; i < n; ++i) {
      if (i == k) {
        continue;
      }
      const Wide multiplier = a[i * n + k];
      for (int j = 0; j < n; ++j) {
        a[i * n + j] -= multiplier * a[k * n + j];
        result[i * n + j] -= multiplier * result[k * n + j];
      }
    }
  }
  return result;
}

template <typename T>
void Verify(TestContext& test, const Sample<T>& sample) {
  const int n = sample.n;
  const auto expected = Oracle(test, sample.full, n);
  std::array<Wide, 4489> inverse{};
  long double error = 0;
  long double norm = 0;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const Wide value = sample.Selected(i, j)
                             ? sample.Entry(i, j)
                             : Adjoint(sample.Entry(j, i), sample.hermitian);
      ASC_DENSE_TEST_CHECK(
          test, std::isfinite(value.real()) && std::isfinite(value.imag()));
      inverse[i * n + j] = value;
      error = std::max(error, std::abs(value - expected[i * n + j]));
      norm = std::max(norm, std::abs(expected[i * n + j]));
      if (sample.hermitian && i == j) {
        ASC_DENSE_TEST_EQ(test, value.imag(), 0);
      }
    }
  }
  const long double epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test, error <= 256 * std::max(1, n) * epsilon * norm);
  long double residual = 0;
  // Both multiplication orders detect incorrect permutation or symmetry.
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      Wide left{};
      Wide right{};
      for (int k = 0; k < n; ++k) {
        left += sample.full[i * n + k] * inverse[k * n + j];
        right += inverse[i * n + k] * sample.full[k * n + j];
      }
      residual = std::max({residual, std::abs(left - Wide{i == j ? 1.L : 0.L}),
                           std::abs(right - Wide{i == j ? 1.L : 0.L})});
    }
  }
  ASC_DENSE_TEST_CHECK(test, residual <= 128 * std::max(1, n) * epsilon);
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Sample<T> sample, bool blocked, bool singular) {
  auto a = sample.View();
  auto ipiv = Pivots(sample.pivots, sample.n);
  const auto factor_plan = Take(asc_indefinite_rook_test::QueryFactor(
      provider, sample.triangle, sample.hermitian, blocked, a, ipiv));
  Scratch<T> factor_scratch;
  const auto factor_work = factor_scratch.Workspace(factor_plan);
  asc::LapackReport factor_report;
  const auto factored = asc_indefinite_rook_test::Factor(
      provider, sample.triangle, sample.hermitian, blocked, a, ipiv,
      factor_plan, factor_work, factor_report);
  ASC_DENSE_TEST_EQ(test, factored.ok(), !singular);
  if (sample.n == 3 && !singular) {
    const auto first = sample.triangle == kUpper ? 2U : 1U;
    ASC_DENSE_TEST_CHECK(
        test, sample.pivots[first] < 0 && sample.pivots[first + 1] < 0);
    ASC_DENSE_TEST_CHECK(test,
                         sample.pivots[first] != sample.pivots[first + 1]);
  }
  const auto raw = Raw(sample.pivots, sample.n);
  const auto before = sample.a;
  const auto pivots_before = sample.pivots;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc_rook_inverse_test::Query(provider, sample.triangle,
                                        sample.hermitian, a, raw);
  }));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc_rook_inverse_test::Inverse(provider, sample.triangle,
                                          sample.hermitian, a, raw, plan,
                                          workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), sample.n != 0);
  if (singular) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), sample.n);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-999),
                      sample.n - 1);
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(sample.a.data(), before.data(), sizeof(before)));
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    if (sample.n != 0) {
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
    }
    Verify(test, sample);
  }
  ASC_DENSE_TEST_EQ(test, sample.pivots, pivots_before);
  sample.Guards(test);
  scratch.Guards(test, workspace);
  factor_scratch.Guards(test, factor_work);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        for (const int n : {0, 1, 2, 3, 7, 67}) {
          Case(test, provider,
               Sample<T>(n, hermitian, triangle, layout, 0, false), blocked,
               false);
          ++cases;
        }
        for (const int n : {1, 7, 67}) {
          Case(test, provider,
               Sample<T>(n, hermitian, triangle, layout, 0, true), blocked,
               true);
          ++cases;
        }
        const int exponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
        for (const int scale : {-exponent, exponent}) {
          Case(test, provider,
               Sample<T>(7, hermitian, triangle, layout, scale, false), blocked,
               false);
          ++cases;
        }
      }
    }
  }
  std::printf("rook inverse cases=%d\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
