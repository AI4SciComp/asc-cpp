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
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_condition_native.h"
#include "indefinite_rk_condition_test_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_rook_test::EqualBytes;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kPivot;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kScalar;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Matrix;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::Raw;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::Value;
using asc_indefinite_rook_test::WithoutAllocation;
using asc_rk_condition_test::Condition;
using asc_rk_condition_test::Query;
using asc_rk_condition_test::Scratch;
using asc_rk_test::Factor;
using asc_rk_test::QueryFactor;

template <typename T, std::size_t Size>
void PoisonExtra(std::array<T, 72>& e,
                 const std::array<asc::index_t, Size>& pivots, int n,
                 asc::DenseBlasTriangle triangle) {
  for (int i = 0; i < n;) {
    const bool pair = pivots[static_cast<std::size_t>(i) + 1] < 0;
    const int ignored = pair && triangle == kLower ? i + 1 : i;
    e[static_cast<std::size_t>(ignored) + 1] = Value<T>(
        std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN(), 19);
    i += pair ? 2 : 1;
  }
}
template <typename T>
void Fidelity(TestContext& test, bool hermitian,
              asc::DenseBlasTriangle triangle,
              asc::DenseBlasMatrixView<const T> factors,
              asc::DenseBlasVectorView<const T> extra,
              asc::RawLapackPivotView pivots, asc::DenseBlasRealType<T> norm,
              asc::DenseBlasRealType<T> condition) {
  using Real = asc::DenseBlasRealType<T>;
  const auto n = static_cast<lapack_int>(factors.rows());
  const lapack_int ld = std::max<lapack_int>(1, n);
  std::array<T, 5000> a{};
  std::array<T, 136> work{};
  std::array<T, 72> e{};
  std::array<lapack_int, 72> p{};
  std::array<lapack_int, 72> iw{};
  for (lapack_int j = 0; j < n; ++j) {
    p[static_cast<std::size_t>(j)] =
        static_cast<lapack_int>(pivots.values()[static_cast<std::size_t>(j)]);
    e[static_cast<std::size_t>(j)] = extra.data()[j];
    for (lapack_int i = 0; i < n; ++i) {
      if (triangle == kUpper ? i <= j : i >= j) {
        const auto offset = factors.layout() == kColumn
                                ? j * factors.leading_dimension() + i
                                : i * factors.leading_dimension() + j;
        a[static_cast<std::size_t>(j) * static_cast<std::size_t>(n) +
          static_cast<std::size_t>(i)] = factors.data()[offset];
      }
    }
  }
  const auto old_a = a;
  const auto old_e = e;
  const auto old_p = p;
  std::array<lapack_int, 3> info{113, std::numeric_limits<lapack_int>::min(),
                                 117};
  std::array<Real, 3> result{Real{127}, Real{-31}, Real{131}};
  const char uplo = triangle == kUpper ? 'U' : 'L';
  asc_rk_condition_test::Native(hermitian, &uplo, &n, a.data(), &ld, e.data(),
                                p.data(), &norm, &result[1], work.data(),
                                iw.data(), &info[1]);
  const std::array<lapack_int, 3> expected_info{113, 0, 117};
  ASC_DENSE_TEST_EQ(test, info, expected_info);
  ASC_DENSE_TEST_EQ(test, result.front(), Real{127});
  ASC_DENSE_TEST_EQ(test, result.back(), Real{131});
  ASC_DENSE_TEST_CHECK(test, EqualBytes(&condition, &result[1], sizeof(Real)));
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(a.data(), old_a.data(), sizeof(a)) &&
                EqualBytes(e.data(), old_e.data(), sizeof(e)) && p == old_p);
}
template <typename T>
std::array<asc::DenseBlasRealType<T>, 2> Initialize(
    std::array<T, 5000>& a, bool hermitian, asc::DenseBlasTriangle triangle,
    asc::DenseBlasLayout layout, int n, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  a.fill(Value<T>(-109, 5));
  const int ld = n + 2;
  const Real scale = std::ldexp(Real{1}, exponent);
  Real smallest = std::numeric_limits<Real>::infinity();
  Real largest = 0;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const bool selected = triangle == kUpper ? i <= j : i >= j;
      const auto offset = 1U + static_cast<std::size_t>(
                                   layout == kColumn ? j * ld + i : i * ld + j);
      a[offset] =
          selected ? T{} : Value<T>(std::numeric_limits<Real>::quiet_NaN());
      if (selected && i == j && (i % 3 == 2 || (i == n - 1 && i % 3 == 0))) {
        const long double diagonal = i % 2 == 0 ? 2 : -8;
        a[offset] = Value<T>(diagonal, hermitian ? 0 : diagonal / 2) * scale;
        const Real magnitude = std::abs(a[offset]);
        smallest = std::min(smallest, magnitude);
        largest = std::max(largest, magnitude);
      }
      if (selected && i != j && std::min(i, j) % 3 == 0 &&
          std::max(i, j) == std::min(i, j) + 1) {
        a[offset] =
            Value<T>(4, hermitian && triangle == kUpper ? -2 : 2) * scale;
        smallest = std::min(smallest, std::abs(a[offset]));
        largest = std::max(largest, std::abs(a[offset]));
      }
      if constexpr (asc::DenseBlasComplex<T>) {
        if (selected && i == j && hermitian) {
          a[offset].imag(std::numeric_limits<Real>::quiet_NaN());
        }
      }
    }
  }
  return {smallest, largest};
}

template <typename T>
void DiagonalBlocks(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    bool hermitian, asc::DenseBlasTriangle triangle,
                    asc::DenseBlasLayout layout, int n, int exponent,
                    bool singular, bool blocked, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 5000> a;
  const auto [smallest, largest] =
      Initialize(a, hermitian, triangle, layout, n, exponent);
  std::array<asc::index_t, 72> pivot;
  pivot.fill(-111);
  const int ld = n + 2;
  if (singular) {
    // The last block is deliberately 1-by-1 in orders 1 and 7.
    a[1U + static_cast<std::size_t>((n - 1) * (ld + 1))] = T{};
  }
  std::array<T, 72> extra;
  extra.fill(Value<T>(-115, 17));
  const auto extra_output = asc_rk_test::OffDiagonal(extra, n);
  auto matrix = Matrix(a, n, n, layout, ld);
  auto pivots = Pivots(pivot, n);
  auto factor_plan = Take(QueryFactor(provider, triangle, hermitian, blocked,
                                      matrix, extra_output, pivots));
  Scratch<T> scratch;
  auto factor_workspace = scratch.Workspace(factor_plan);
  asc::LapackReport report;
  const auto factor_status = WithoutAllocation(test, [&] {
    return Factor(provider, triangle, hermitian, blocked, matrix, extra_output,
                  pivots, factor_plan, factor_workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, factor_status.ok(), !singular);
  if (singular) {
    ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), n);
  }
  scratch.Guards(test, factor_workspace);
  PoisonExtra(extra, pivot, n, triangle);
  const auto saved_e = extra;
  const auto immutable_e = asc::DenseBlasVectorView<const T>(extra_output);
  const auto saved_a = a;
  const auto saved_pivots = pivot;
  const auto immutable = Matrix(saved_a, n, n, layout, ld);
  const auto raw = Raw(pivot, n);
  Real condition = -13;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, triangle, hermitian, immutable, immutable_e, raw,
                 largest, condition);
  }));
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries, 2 * n);
  const int integer_factor = asc::DenseBlasComplex<T> ? 1 : 2;
  ASC_DENSE_TEST_EQ(test, plan.regions[kPivot].minimum_entries,
                    integer_factor * n);
  scratch = Scratch<T>{};
  auto workspace = scratch.Workspace(plan);
  const auto status = WithoutAllocation(test, [&] {
    return Condition(provider, triangle, hermitian, immutable, immutable_e, raw,
                     largest, condition, plan, workspace, report);
  });
  scratch.Guards(test, workspace);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
  Real expected = 1;
  if (n != 0) {
    expected = singular ? Real{} : smallest / largest;
  }
  if (fidelity) {
    Fidelity(test, hermitian, triangle, immutable, immutable_e, raw, largest,
             condition);
  } else {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
    ASC_DENSE_TEST_CHECK(test, std::abs(condition - expected) <=
                                   32 * std::numeric_limits<Real>::epsilon() *
                                       std::max(condition, expected));
  }
  ASC_DENSE_TEST_CHECK(test,
                       EqualBytes(extra.data(), saved_e.data(), sizeof(extra)));
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_CHECK(test, EqualBytes(a.data(), saved_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(pivot.data(), saved_pivots.data(), sizeof(pivot)));
}

std::size_t Index3(int row, int column) {
  return 3 * static_cast<std::size_t>(row) + static_cast<std::size_t>(column);
}

// Independent 3-by-3 adjugate oracle, evaluated in wider complex arithmetic.
long double ReciprocalOneNorm(
    const std::array<asc_indefinite_rook_test::Wide, 9>& a) {
  using Wide = asc_indefinite_rook_test::Wide;
  const Wide determinant = a[0] * (a[4] * a[8] - a[5] * a[7]) -
                           a[1] * (a[3] * a[8] - a[5] * a[6]) +
                           a[2] * (a[3] * a[7] - a[4] * a[6]);
  long double norm = 0;
  long double inverse_norm = 0;
  for (int j = 0; j < 3; ++j) {
    long double column = 0;
    long double inverse_column = 0;
    for (int i = 0; i < 3; ++i) {
      column += std::abs(a[Index3(i, j)]);
      std::array<int, 2> rows{};
      std::array<int, 2> columns{};
      int r = 0;
      int c = 0;
      for (int k = 0; k < 3; ++k) {
        if (k != j) {
          rows[static_cast<std::size_t>(r++)] = k;
        }
        if (k != i) {
          columns[static_cast<std::size_t>(c++)] = k;
        }
      }
      auto at = [&](int row, int col) {
        return a[Index3(rows[static_cast<std::size_t>(row)],
                        columns[static_cast<std::size_t>(col)])];
      };
      inverse_column +=
          std::abs((at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0)) / determinant);
    }
    norm = std::max(norm, column);
    inverse_norm = std::max(inverse_norm, inverse_column);
  }
  return 1 / (norm * inverse_norm);
}

template <typename T>
asc::DenseBlasRealType<T> InterchangeMatrix(
    std::array<T, 18>& a, std::array<asc_indefinite_rook_test::Wide, 9>& full,
    bool hermitian, asc::DenseBlasTriangle triangle,
    asc::DenseBlasLayout layout) {
  namespace base = asc_indefinite_rook_test;
  for (int j = 0; j < 3; ++j) {
    for (int i = j + 1; i < 3; ++i) {
      const bool strong = triangle == kUpper ? i == 1 : j == 1;
      long double magnitude = i - j == 2 ? 0.5L : 1;
      if (strong) {
        magnitude = 4;
      }
      const T value = Value<T>(magnitude, 0.125L);
      full[Index3(i, j)] = base::ToWide(value);
      full[Index3(j, i)] = base::Adjoint(base::ToWide(value), hermitian);
    }
  }
  long double norm = 0;
  for (int j = 0; j < 3; ++j) {
    long double column = 0;
    for (int i = 0; i < 3; ++i) {
      column += std::abs(full[Index3(i, j)]);
      if ((triangle == kUpper && i <= j) || (triangle == kLower && i >= j)) {
        const auto value = full[Index3(i, j)];
        a[1U +
          static_cast<std::size_t>(layout == kColumn ? j * 5 + i : i * 5 + j)] =
            Value<T>(value.real(), value.imag());
      }
    }
    norm = std::max(norm, column);
  }
  return static_cast<asc::DenseBlasRealType<T>>(norm);
}
template <typename T>
void BothInterchanges(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      bool hermitian, asc::DenseBlasTriangle triangle,
                      asc::DenseBlasLayout layout, bool blocked,
                      bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 18> a{};
  a.fill(Value<T>(-211, 19));
  std::array<asc_indefinite_rook_test::Wide, 9> full{};
  const Real original_norm =
      InterchangeMatrix(a, full, hermitian, triangle, layout);
  std::array<asc::index_t, 5> pivots{};
  std::array<T, 72> extra;
  extra.fill(Value<T>(-215, 17));
  const auto extra_output = asc_rk_test::OffDiagonal(extra, 3);
  auto matrix = Matrix(a, 3, 3, layout, 5);
  auto pivot = Pivots(pivots, 3);
  const auto fp = Take(QueryFactor(provider, triangle, hermitian, blocked,
                                   matrix, extra_output, pivot));
  Scratch<T> scratch;
  auto workspace = scratch.Workspace(fp);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, Factor(provider, triangle, hermitian, blocked, matrix, extra_output,
                   pivot, fp, workspace, report)
                .ok());
  const int first = triangle == kUpper ? 2 : 1;
  ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(first)],
                    triangle == kUpper ? -1 : -2);
  ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(first + 1)],
                    triangle == kUpper ? -2 : -3);
  scratch.Guards(test, workspace);
  PoisonExtra(extra, pivots, 3, triangle);
  const auto saved_e = extra;
  const auto immutable_e = asc::DenseBlasVectorView<const T>(extra_output);
  const auto saved_a = a;
  const auto saved_p = pivots;
  const auto factors = Matrix(saved_a, 3, 3, layout, 5);
  const auto raw = Raw(pivots, 3);
  Real condition = -13;
  const auto plan = Take(Query(provider, triangle, hermitian, factors,
                               immutable_e, raw, original_norm, condition));
  scratch = Scratch<T>{};
  workspace = scratch.Workspace(plan);
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Condition(provider, triangle, hermitian,
                                                factors, immutable_e, raw,
                                                original_norm, condition, plan,
                                                workspace, report);
                             }).ok());
  scratch.Guards(test, workspace);
  if (fidelity) {
    Fidelity(test, hermitian, triangle, factors, immutable_e, raw,
             original_norm, condition);
  } else {
    const long double expected = ReciprocalOneNorm(full);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
    ASC_DENSE_TEST_CHECK(
        test, std::abs(condition - expected) <=
                  64 * std::numeric_limits<Real>::epsilon() * expected);
  }
  ASC_DENSE_TEST_CHECK(test,
                       EqualBytes(extra.data(), saved_e.data(), sizeof(extra)));
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_CHECK(test, EqualBytes(a.data(), saved_a.data(), sizeof(a)) &&
                                 pivots == saved_p);
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        BothInterchanges<T>(test, provider, hermitian, triangle, layout,
                            blocked, fidelity);
        ++cases;
        for (const int n : {0, 1, 2, 7, 67}) {
          DiagonalBlocks<T>(test, provider, hermitian, triangle, layout, n, 0,
                            false, blocked, fidelity);
          ++cases;
        }
        for (const int n : {1, 7, 67}) {
          DiagonalBlocks<T>(test, provider, hermitian, triangle, layout, n, 0,
                            true, blocked, fidelity);
          ++cases;
        }
        const int exponent =
            sizeof(asc::DenseBlasRealType<T>) == sizeof(float) ? 100 : 800;
        for (const int n : {7, 67}) {
          for (const int scale : {-exponent, exponent}) {
            DiagonalBlocks<T>(test, provider, hermitian, triangle, layout, n,
                              scale, false, blocked, fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("RK condition cases=%d mode=%s\n", cases,
              fidelity ? "fidelity" : "mathematical");
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
