#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_test::Adjoint;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kLower;
using asc_indefinite_test::kRow;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Matrix;
using asc_indefinite_test::Pivots;
using asc_indefinite_test::Scratch;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::ToWide;
using asc_indefinite_test::Value;
using asc_indefinite_test::Wide;
using asc_indefinite_test::WithoutAllocation;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasVectorView<asc::index_t> pivots,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHesvWorkspace(provider, triangle, a, pivots, b);
    }
  }
  return asc::QuerySysvWorkspace(provider, triangle, a, pivots, b);
}

template <typename T>
asc::Status Driver(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasMatrixView<T> a,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasMatrixView<T> b,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hesv(provider, triangle, a, pivots, b, plan, workspace,
                       report);
    }
  }
  return asc::Sysv(provider, triangle, a, pivots, b, plan, workspace, report);
}

std::size_t Offset(int i, int j, asc::DenseBlasLayout layout, int ld) {
  return 1U +
         static_cast<std::size_t>(layout == kColumn ? j * ld + i : i * ld + j);
}

template <typename T>
struct Sample {
  int n;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  int ld;
  std::array<T, 5000> a{};
  std::array<T, 5000> before{};
  std::array<asc::index_t, 72> pivots{};
  std::array<Wide, 4489> full{};

  Sample(int order, bool he, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout storage, int exponent, bool singular)
      : n(order), hermitian(he), triangle(tri), layout(storage), ld(n + 2) {
    using Real = asc::DenseBlasRealType<T>;
    a.fill(Value<T>(-311, 17));
    pivots.fill(-313);
    const Real scale = std::ldexp(Real{1}, exponent);
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j <= i; ++j) {
        T value{};
        if (i == j) {
          value = Value<T>(i % 3 == 0 || i == n - 1 ? 4 : 0,
                           hermitian ? 0 : 0.125L);
        } else if (j % 3 == 1 && i == j + 1) {
          value = Value<T>(2, 0.5L);
        } else {
          value = Value<T>(static_cast<long double>((i + j) % 3 - 1) / 64,
                           static_cast<long double>((i + j) % 5 - 2) / 128);
        }
        if (singular && i == n - 1) {
          value = T{};
        }
        value *= scale;
        full[i * n + j] = ToWide(value);
        full[j * n + i] = Adjoint(ToWide(value), hermitian);
      }
    }
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        const auto k = Offset(i, j, layout, ld);
        const auto value = full[i * n + j];
        a[k] = Selected(i, j)
                   ? Value<T>(value.real(), value.imag())
                   : Value<T>(std::numeric_limits<Real>::quiet_NaN());
        if constexpr (asc::DenseBlasComplex<T>) {
          if (hermitian && i == j) {
            a[k].imag(std::numeric_limits<Real>::quiet_NaN());
          }
        }
      }
    }
    before = a;
  }

  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == kUpper ? i <= j : i >= j;
  }

  [[nodiscard]] Wide Entry(int i, int j) const {
    return ToWide(a[Offset(i, j, layout, ld)]);
  }

  void Reconstruction(TestContext& test) const {
    // Independent reverse Schur-complement block reconstruction, reused from
    // this repository's original ASC factor tests. No provider solve or
    // factorization implementation constructs this oracle.
    std::array<Wide, 4489> reconstructed{};
    int cursor = triangle == kLower ? n - 1 : 0;
    while (cursor >= 0 && cursor < n) {
      const auto raw = pivots[static_cast<std::size_t>(cursor) + 1];
      const bool paired = raw < 0;
      const int size = paired ? 2 : 1;
      const int first = triangle == kLower ? cursor - size + 1 : cursor;
      const int last = first + size;
      const auto target_raw = paired ? -raw : raw;
      if (first < 0 || last > n || target_raw < 1 || target_raw > n) {
        ASC_DENSE_TEST_CHECK(test, false);
        return;
      }
      std::array<Wide, 4> d{};
      d[0] = Entry(first, first);
      if (paired) {
        d[3] = Entry(first + 1, first + 1);
        if (triangle == kLower) {
          d[2] = Entry(first + 1, first);
          d[1] = Adjoint(d[2], hermitian);
        } else {
          d[1] = Entry(first, first + 1);
          d[2] = Adjoint(d[1], hermitian);
        }
      }
      const int begin = triangle == kLower ? last : 0;
      const int end = triangle == kLower ? n : first;
      for (int i = begin; i < end; ++i) {
        for (int j = begin; j < end; ++j) {
          for (int p = 0; p < size; ++p) {
            for (int q = 0; q < size; ++q) {
              reconstructed[i * n + j] +=
                  Entry(i, first + p) * d[2 * p + q] *
                  Adjoint(Entry(j, first + q), hermitian);
            }
          }
        }
        for (int p = 0; p < size; ++p) {
          Wide value{};
          for (int q = 0; q < size; ++q) {
            value += Entry(i, first + q) * d[2 * q + p];
          }
          reconstructed[i * n + first + p] = value;
          reconstructed[(first + p) * n + i] = Adjoint(value, hermitian);
        }
      }
      for (int p = 0; p < size; ++p) {
        for (int q = 0; q < size; ++q) {
          reconstructed[(first + p) * n + first + q] = d[2 * p + q];
        }
      }
      const int swapped = triangle == kLower ? last - 1 : first;
      const int target = static_cast<int>(target_raw) - 1;
      for (int j = 0; j < n; ++j) {
        std::swap(reconstructed[swapped * n + j],
                  reconstructed[target * n + j]);
      }
      for (int i = 0; i < n; ++i) {
        std::swap(reconstructed[i * n + swapped],
                  reconstructed[i * n + target]);
      }
      cursor += triangle == kLower ? -size : size;
    }
    long double error = 0;
    long double norm = 0;
    for (int i = 0; i < n * n; ++i) {
      error = std::max(error, std::abs(reconstructed[i] - full[i]));
      norm = std::max(norm, std::abs(full[i]));
    }
    ASC_DENSE_TEST_CHECK(
        test,
        error <= 32 * std::max(n, 1) * norm *
                     std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
  }

  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i = layout == kColumn ? relative % ld : relative / ld;
      const int j = layout == kColumn ? relative / ld : relative % ld;
      if (k == 0 || i >= n || j >= n || !Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test, EqualBytes(&a[k], &before[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -313);
    for (std::size_t i = static_cast<std::size_t>(n) + 1; i < pivots.size();
         ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[i], -313);
    }
  }
};

Wide Expected(int row, int column) {
  return {static_cast<long double>((row + column) % 3 - 1),
          static_cast<long double>((row + 2 * column) % 5 - 2) / 4};
}

template <typename T>
void SolveCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Sample<T> sample, asc::DenseBlasLayout rhs_layout, int nrhs,
               asc::extent_t capacity, bool singular) {
  using Real = asc::DenseBlasRealType<T>;
  const int ldb = rhs_layout == kColumn ? sample.n + 2 : nrhs + 2;
  std::array<T, 500> b;
  b.fill(Value<T>(-317, 19));
  for (int j = 0; j < nrhs; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      Wide sum{};
      for (int k = 0; k < sample.n; ++k) {
        const auto x = Expected(k, j);
        const Wide actual_x = ToWide(Value<T>(x.real(), x.imag()));
        sum += sample.full[i * sample.n + k] * actual_x;
      }
      b[Offset(i, j, rhs_layout, ldb)] = Value<T>(sum.real(), sum.imag());
    }
  }
  const auto saved_b = b;
  auto a_view = Matrix(sample.a, sample.n, sample.n, sample.layout, sample.ld);
  auto pivots = Pivots(sample.pivots, sample.n);
  auto b_view = Matrix(b, sample.n, nrhs, rhs_layout, ldb);
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, sample.triangle, sample.hermitian, a_view, pivots,
                 b_view);
  }));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, sample.n == 0 ? 0 : capacity);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return Driver(provider, sample.triangle, sample.hermitian, a_view, pivots,
                  b_view, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  if (singular) {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), sample.n);
    ASC_DENSE_TEST_CHECK(test, EqualBytes(b.data(), saved_b.data(), sizeof(b)));
  } else {
    long double residual = 0;
    long double denominator = 0;
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < sample.n; ++i) {
        const auto expected = Expected(i, j);
        const Wide x = ToWide(Value<T>(expected.real(), expected.imag()));
        const Wide actual = ToWide(b[Offset(i, j, rhs_layout, ldb)]);
        ASC_DENSE_TEST_CHECK(
            test, std::abs(actual - x) <=
                      256 * sample.n * std::numeric_limits<Real>::epsilon());
        Wide product{};
        const Wide original = ToWide(saved_b[Offset(i, j, rhs_layout, ldb)]);
        long double bound = std::abs(original);
        for (int k = 0; k < sample.n; ++k) {
          const Wide coefficient = sample.full[i * sample.n + k];
          const Wide solution = ToWide(b[Offset(k, j, rhs_layout, ldb)]);
          product += coefficient * solution;
          bound += std::abs(coefficient) * std::abs(solution);
        }
        residual = std::max(residual, std::abs(product - original));
        denominator = std::max(denominator, bound);
      }
    }
    ASC_DENSE_TEST_CHECK(test,
                         residual <= 128 * std::max(sample.n, 1) *
                                         std::numeric_limits<Real>::epsilon() *
                                         denominator);
  }
  sample.Reconstruction(test);
  sample.Guards(test);
  // Logical RHS entries are the only writable B values, including blocked
  // solves, singular returns and empty dimensions. Preserve every gap and
  // red-zone byte in either independently selected RHS layout.
  for (std::size_t k = 0; k < b.size(); ++k) {
    const int relative = static_cast<int>(k) - 1;
    const int i = rhs_layout == kColumn ? relative % ldb : relative / ldb;
    const int j = rhs_layout == kColumn ? relative / ldb : relative % ldb;
    if (k == 0 || i >= sample.n || j >= nrhs) {
      ASC_DENSE_TEST_CHECK(test, EqualBytes(&b[k], &saved_b[k], sizeof(T)));
    }
  }
  scratch.Guards(test, workspace);
}

template <typename T>
void NanInput(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian, asc::DenseBlasTriangle triangle,
              asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> a{T{-3}, Value<T>(std::numeric_limits<Real>::quiet_NaN()),
                     T{-5}};
  std::array<T, 3> b{T{-7}, T{3}, T{-11}};
  std::array<asc::index_t, 3> pivots{-13, -17, -19};
  const auto old_b = b;
  auto matrix = Matrix(a, 1, 1, layout, 1);
  auto rhs = Matrix(b, 1, 1, layout, 1);
  auto pivot = Pivots(pivots, 1);
  const auto plan =
      Take(Query(provider, triangle, hermitian, matrix, pivot, rhs));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto result = WithoutAllocation(test, [&] {
    return Driver(provider, triangle, hermitian, matrix, pivot, rhs, plan,
                  workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, result.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 1);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_EQ(test, pivots[1], 1);
  ASC_DENSE_TEST_EQ(test, b, old_b);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      NanInput<T>(test, provider, hermitian, triangle, layout);
      for (const auto rhs_layout : {kColumn, kRow}) {
        for (const int n : {0, 1, 2, 7, 67}) {
          const Sample<T> sample(n, hermitian, triangle, layout, 0, false);
          for (const int nrhs : {0, 1, 3}) {
            for (const asc::extent_t work :
                 {1, std::max(1, n), 64 * std::max(1, n)}) {
              SolveCase(test, provider, sample, rhs_layout, nrhs, work, false);
              ++cases;
            }
          }
        }
        for (const int n : {1, 7}) {
          const Sample<T> sample(n, hermitian, triangle, layout, 0, true);
          SolveCase(test, provider, sample, rhs_layout, 3, 1, true);
          SolveCase(test, provider, sample, rhs_layout, 3, 64 * n, true);
          cases += 2;
        }
        const int exponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
        for (const int scale : {-exponent, exponent}) {
          const Sample<T> sample(7, hermitian, triangle, layout, scale, false);
          SolveCase(test, provider, sample, rhs_layout, 3, 1, false);
          SolveCase(test, provider, sample, rhs_layout, 3, 448, false);
          cases += 2;
        }
      }
    }
  }
  std::printf("driver cases=%d\n", cases);
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
