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
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook_condition.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_rook_test::EqualBytes;
using asc_indefinite_rook_test::Factor;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kHost;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kPivot;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kScalar;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Matrix;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::QueryFactor;
using asc_indefinite_rook_test::Raw;
using asc_indefinite_rook_test::Scratch;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::Value;
using asc_indefinite_rook_test::WithoutAllocation;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> a, asc::RawLapackPivotView pivots,
           asc::DenseBlasRealType<T> norm,
           const asc::DenseBlasRealType<T>& condition) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHeconRookWorkspace(provider, triangle, a, pivots, norm,
                                          condition);
    }
  }
  return asc::QuerySyconRookWorkspace(provider, triangle, a, pivots, norm,
                                      condition);
}

template <typename T>
asc::Status Condition(const asc::ReferenceLapackProvider& provider,
                      asc::DenseBlasTriangle triangle, bool hermitian,
                      asc::DenseBlasMatrixView<const T> a,
                      asc::RawLapackPivotView pivots,
                      asc::DenseBlasRealType<T> norm,
                      asc::DenseBlasRealType<T>& condition,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HeconRook(provider, triangle, a, pivots, norm, condition,
                            plan, workspace, report);
    }
  }
  return asc::SyconRook(provider, triangle, a, pivots, norm, condition, plan,
                        workspace, report);
}

template <typename T>
std::array<asc::DenseBlasRealType<T>, 2> Initialize(
    std::array<T, 90>& a, bool hermitian, asc::DenseBlasTriangle triangle,
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
                    bool singular) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 90> a;
  const auto [smallest, largest] =
      Initialize(a, hermitian, triangle, layout, n, exponent);
  std::array<asc::index_t, 12> pivot;
  pivot.fill(-111);
  const int ld = n + 2;
  if (singular) {
    // The last block is deliberately 1-by-1 in orders 1 and 7.
    a[1U + static_cast<std::size_t>((n - 1) * (ld + 1))] = T{};
  }
  auto matrix = Matrix(a, n, n, layout, ld);
  auto pivots = Pivots(pivot, n);
  auto factor_plan =
      Take(QueryFactor(provider, triangle, hermitian, true, matrix, pivots));
  Scratch<T> scratch;
  auto factor_workspace = scratch.Workspace(factor_plan);
  asc::LapackReport report;
  const auto factor_status = WithoutAllocation(test, [&] {
    return Factor(provider, triangle, hermitian, true, matrix, pivots,
                  factor_plan, factor_workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, factor_status.ok(), !singular);
  if (singular) {
    ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), n);
  }
  const auto saved_a = a;
  const auto saved_pivots = pivot;
  const auto immutable = Matrix(saved_a, n, n, layout, ld);
  const auto raw = Raw(pivot, n);
  Real condition = -13;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, triangle, hermitian, immutable, raw, largest,
                 condition);
  }));
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries, 2 * n);
  const int integer_factor = asc::DenseBlasComplex<T> ? 1 : 2;
  ASC_DENSE_TEST_EQ(test, plan.regions[kPivot].minimum_entries,
                    integer_factor * n);
  scratch = Scratch<T>{};
  auto workspace = scratch.Workspace(plan);
  const auto status = WithoutAllocation(test, [&] {
    return Condition(provider, triangle, hermitian, immutable, raw, largest,
                     condition, plan, workspace, report);
  });
  scratch.Guards(test, workspace);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
  Real expected = 1;
  if (n != 0) {
    expected = singular ? Real{} : smallest / largest;
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
  ASC_DENSE_TEST_CHECK(test, std::abs(condition - expected) <=
                                 32 * std::numeric_limits<Real>::epsilon() *
                                     std::max(condition, expected));
  ASC_DENSE_TEST_CHECK(test, EqualBytes(a.data(), saved_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(pivot.data(), saved_pivots.data(), sizeof(pivot)));
}

template <typename T>
void Preflight(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 12> a{};
  a[3] = T{1};
  const auto view = Matrix(std::as_const(a), 2, 2, kColumn, 2);
  std::array<asc::index_t, 4> pivot{101, -1, -1, 103};
  const auto raw = Raw(pivot, 2);
  Real condition = -41;
  const auto plan =
      Take(Query(provider, kUpper, hermitian, view, raw, Real{1}, condition));
  Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  const auto scalar_before = scratch.scalar;
  const auto integer_before = scratch.pivot;
  const auto packed_before = scratch.packed;
  const auto a_before = a;
  auto reject = [&](const asc::LapackWorkspacePlan& supplied,
                    const asc::LapackWorkspace& work, asc::ErrorCode code) {
    const auto pivots_before = pivot;
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 99;
    const auto status = WithoutAllocation(test, [&] {
      return Condition(provider, kUpper, hermitian, view, raw, Real{1},
                       condition, supplied, work, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), code);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, condition, Real{-41});
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(a.data(), a_before.data(), sizeof(a)));
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(pivot.data(), pivots_before.data(), sizeof(pivot)));
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(scratch.scalar.data(), scalar_before.data(),
                                    sizeof(scratch.scalar)));
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(scratch.pivot.data(), integer_before.data(),
                                    sizeof(scratch.pivot)));
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(scratch.packed.data(), packed_before.data(),
                                    sizeof(scratch.packed)));
  };
  auto stale = plan;
  ++stale.regions[kScalar].preferred_entries;
  reject(stale, workspace, asc::ErrorCode::kInvalidState);
  auto short_work = workspace;
  auto& integers = short_work.regions[kPivot];
  integers = {integers.data(), integers.size() - 1, kHost};
  reject(plan, short_work, asc::ErrorCode::kInvalidArgument);
  auto inaccessible = workspace;
  auto& scalar = inaccessible.regions[kScalar];
  scalar = {scalar.data(), scalar.size(), asc::MemorySpace::kPinnedHost};
  reject(plan, inaccessible, asc::ErrorCode::kMemoryAccess);
  pivot[1] = 0;
  reject(plan, workspace, asc::ErrorCode::kInvalidArgument);
  pivot[1] = -1;
  pivot[2] = 2;
  reject(plan, workspace, asc::ErrorCode::kInvalidArgument);
  pivot[2] = -1;
  a[3] = T{};
  // This separate check observes the numeric preflight, not a foreign INFO.
  asc::LapackReport report;
  const auto singular = WithoutAllocation(test, [&] {
    return Condition(provider, kUpper, hermitian, view, raw, Real{1}, condition,
                     plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, singular.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), 0);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, condition, Real{-41});
}

template <typename T>
void QuickAndWide(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 3> a{T{-11}, T{4}, T{-13}};
  std::array<asc::index_t, 3> pivot{101, 1, 103};
  const auto raw = Raw(pivot, 1);
  for (const auto layout : {kRow, kColumn}) {
    const auto view = Matrix(std::as_const(a), 1, 1, layout,
                             std::numeric_limits<asc::extent_t>::max());
    Real condition = -41;
    const auto plan =
        Take(Query(provider, kUpper, hermitian, view, raw, Real{4}, condition));
    Scratch<T> scratch;
    auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Condition(provider, kUpper, hermitian,
                                                  view, raw, Real{4}, condition,
                                                  plan, workspace, report);
                               }).ok());
    ASC_DENSE_TEST_EQ(test, condition, Real{1});
    pivot[1] = 0;  // Unread on the zero-original-norm branch.
    const auto zero =
        Take(Query(provider, kUpper, hermitian, view, raw, Real{}, condition));
    asc::LapackWorkspace empty;
    const auto unused =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
    empty.regions[unused] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return Condition(provider, kUpper, hermitian,
                                                  view, raw, Real{}, condition,
                                                  zero, empty, report);
                               }).ok());
    ASC_DENSE_TEST_EQ(test, condition, Real{});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    pivot[1] = 1;
  }
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
void BothInterchanges(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      bool hermitian, asc::DenseBlasTriangle triangle,
                      asc::DenseBlasLayout layout, bool blocked) {
  namespace base = asc_indefinite_rook_test;
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 18> a{};
  a.fill(Value<T>(-211, 19));
  std::array<base::Wide, 9> full{};
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
  const Real original_norm = static_cast<Real>(norm);
  std::array<asc::index_t, 5> pivots{};
  auto matrix = Matrix(a, 3, 3, layout, 5);
  auto pivot = Pivots(pivots, 3);
  const auto fp =
      Take(QueryFactor(provider, triangle, hermitian, blocked, matrix, pivot));
  Scratch<T> scratch;
  auto workspace = scratch.Workspace(fp);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, Factor(provider, triangle, hermitian, blocked,
                                    matrix, pivot, fp, workspace, report)
                                 .ok());
  const int first = triangle == kUpper ? 2 : 1;
  ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(first)],
                    triangle == kUpper ? -1 : -2);
  ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(first + 1)],
                    triangle == kUpper ? -2 : -3);
  const auto saved_a = a;
  const auto saved_p = pivots;
  const auto factors = Matrix(saved_a, 3, 3, layout, 5);
  const auto raw = Raw(pivots, 3);
  Real condition = -13;
  const auto plan = Take(Query(provider, triangle, hermitian, factors, raw,
                               original_norm, condition));
  scratch = Scratch<T>{};
  workspace = scratch.Workspace(plan);
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Condition(provider, triangle, hermitian,
                                                factors, raw, original_norm,
                                                condition, plan, workspace,
                                                report);
                             }).ok());
  scratch.Guards(test, workspace);
  const long double expected = ReciprocalOneNorm(full);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
  ASC_DENSE_TEST_CHECK(
      test, std::abs(condition - expected) <=
                64 * std::numeric_limits<Real>::epsilon() * expected);
  ASC_DENSE_TEST_CHECK(test, EqualBytes(a.data(), saved_a.data(), sizeof(a)) &&
                                 pivots == saved_p);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        BothInterchanges<T>(test, provider, hermitian, triangle, layout,
                            blocked);
        ++cases;
      }
      for (const int n : {0, 1, 2, 7}) {
        DiagonalBlocks<T>(test, provider, hermitian, triangle, layout, n, 0,
                          false);
        ++cases;
      }
      for (const int n : {1, 7}) {
        DiagonalBlocks<T>(test, provider, hermitian, triangle, layout, n, 0,
                          true);
        ++cases;
      }
      const int exponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
      for (const int scale : {-exponent, exponent}) {
        DiagonalBlocks<T>(test, provider, hermitian, triangle, layout, 7, scale,
                          false);
        ++cases;
      }
    }
  }
  Preflight<T>(test, provider, hermitian);
  QuickAndWide<T>(test, provider, hermitian);
  std::printf("condition cases=%d\n", cases);
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
