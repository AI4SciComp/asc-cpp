#ifndef ASC_TESTS_DENSE_LAPACK_LU_HELPERS_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_HELPERS_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"

namespace asc_helpers_test {
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Unexpected setup failure %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

template <typename Operation>
auto WithoutAllocation(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp_probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto c_calls = asc_lapack_test::EndAllocationAudit();
  const auto cpp_calls = cpp_probe.count();
  ASC_DENSE_TEST_EQ(test, c_calls, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_calls, 0));
  return result;
}

template <typename T>
T Value(long double real, long double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
std::complex<long double> ToWide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

template <typename T>
bool SameBits(const T& first, const T& second) {
  using Bytes = std::array<std::byte, sizeof(T)>;
  return std::bit_cast<Bytes>(first) == std::bit_cast<Bytes>(second);
}

template <typename T, std::size_t Size>
auto Vector(const std::array<T, Size>& storage, asc::extent_t count,
            asc::index_t increment = 1) {
  return Take(asc::DenseBlasVectorView<const T>::Create(
      storage.data() + 1, count, increment,
      {storage.data(), sizeof(storage), kHost}));
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  asc::extent_t m;
  asc::extent_t n;
  asc::DenseBlasLayout layout;
  std::array<T, 700> a{};
  std::array<asc::index_t, 66> pivots{};
  std::array<Real, 10> rows{};
  std::array<Real, 67> columns{};
  std::array<T, 522> packed{};
  alignas(16) std::array<std::byte, 528> integer{};
  asc::LapackEquilibrationStatistics<Real> statistics{1, 1, 1};
  asc::LapackEquilibration applied = asc::LapackEquilibration::kBoth;

  Sample(asc::extent_t row_count, asc::extent_t column_count,
         asc::DenseBlasLayout matrix_layout)
      : m(row_count), n(column_count), layout(matrix_layout) {
    a.fill(Value<T>(-71));
    pivots.fill(-73);
    rows.fill(Real{-79});
    columns.fill(Real{-83});
    packed.fill(Value<T>(-89));
    integer.fill(std::byte{0x5a});
    for (asc::extent_t i = 0; i < m; ++i) {
      rows[i + 1] = 1;
      for (asc::extent_t j = 0; j < n; ++j) {
        a[Offset(i, j)] = Value<T>(100 * i + j + 1, -50 * i + j + 0.5L);
      }
    }
    for (asc::extent_t j = 0; j < n; ++j) {
      columns[j + 1] = 1;
    }
  }

  [[nodiscard]] asc::extent_t Ld() const { return layout == kColumn ? 10 : 67; }
  [[nodiscard]] std::size_t Offset(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * Ld() + i : i * Ld() + j));
  }
  auto Matrix() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        a.data() + 1, m, n, layout, Ld(), {a.data(), sizeof(a), kHost}));
  }
  [[nodiscard]] auto Pivots(
      asc::extent_t count = 64,
      asc::LapackFactorFamily family =
          asc::LapackFactorFamily::kLuPartialPivot) const {
    return Take(asc::RawLapackPivotView::Create(
        pivots.data() + 1, count, family,
        {pivots.data(), sizeof(pivots), kHost}));
  }
  [[nodiscard]] auto Rows() const { return Vector(rows, m); }
  [[nodiscard]] auto Columns() const { return Vector(columns, n); }
  asc::LapackWorkspace Workspace() {
    asc::LapackWorkspace workspace;
    workspace.regions[kInteger] = {integer.data() + 8, 512, kHost};
    workspace.regions[kLayout] = {packed.data() + 1, 520 * sizeof(T), kHost};
    return workspace;
  }
};

template <typename T>
void Unchanged(TestContext& test, const Sample<T>& current,
               const Sample<T>& before) {
  ASC_DENSE_TEST_CHECK(test, SameBits(current.a, before.a));
  ASC_DENSE_TEST_EQ(test, current.pivots, before.pivots);
  ASC_DENSE_TEST_CHECK(test, SameBits(current.rows, before.rows));
  ASC_DENSE_TEST_CHECK(test, SameBits(current.columns, before.columns));
  ASC_DENSE_TEST_CHECK(test, SameBits(current.packed, before.packed));
  ASC_DENSE_TEST_EQ(test, current.integer, before.integer);
  ASC_DENSE_TEST_EQ(test, current.applied, before.applied);
  ASC_DENSE_TEST_CHECK(test, SameBits(current.statistics.row_condition,
                                      before.statistics.row_condition));
  ASC_DENSE_TEST_CHECK(test, SameBits(current.statistics.column_condition,
                                      before.statistics.column_condition));
  ASC_DENSE_TEST_CHECK(test, SameBits(current.statistics.absolute_maximum,
                                      before.statistics.absolute_maximum));
}

template <typename T>
void Padding(TestContext& test, const Sample<T>& current,
             const Sample<T>& before) {
  std::array<bool, 700> used{};
  for (asc::extent_t i = 0; i < current.m; ++i) {
    for (asc::extent_t j = 0; j < current.n; ++j) {
      used[current.Offset(i, j)] = true;
    }
  }
  for (std::size_t i = 0; i < current.a.size(); ++i) {
    if (!used[i]) {
      ASC_DENSE_TEST_EQ(test, current.a[i], before.a[i]);
    }
  }
  const auto count = current.layout == kRow ? current.m * current.n : 0;
  ASC_DENSE_TEST_EQ(test, current.packed[0], before.packed[0]);
  for (std::size_t i = static_cast<std::size_t>(count) + 1;
       i < current.packed.size(); ++i) {
    ASC_DENSE_TEST_EQ(test, current.packed[i], before.packed[i]);
  }
}
}  // namespace asc_helpers_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_HELPERS_TEST_SUPPORT_H_
