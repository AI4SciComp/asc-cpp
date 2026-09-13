#ifndef ASC_TESTS_DENSE_LAPACK_LU_PIVOT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_PIVOT_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_pivot_faults.h"

namespace asc_lu_pivot_test {
using asc_dense_test::TestContext;
using Fault = asc_lapack_test::LuPivotFault;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kLu = asc::LapackFactorFamily::kLuPartialPivot;
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr std::int64_t kSentinel =
    ASC_LAPACK_INTEGER_BITS == 64 ? std::numeric_limits<std::int64_t>::min()
                                  : std::numeric_limits<std::int32_t>::min();
static_assert(std::endian::native == std::endian::little);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "LU INFO setup failed: %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

template <typename T>
T Value(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
auto Wide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::complex<long double>{value.real(), value.imag()};
  } else {
    return static_cast<long double>(value);
  }
}

template <typename T>
struct Matrix {
  int rows;
  int columns;
  int ld;
  asc::DenseBlasLayout layout;
  std::array<T, 4900> data;
  Matrix(int m, int n, asc::DenseBlasLayout order)
      : rows(m), columns(n), ld((order == kRow ? n : m) + 2), layout(order) {
    data.fill(Value<T>(-791, 37));
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return static_cast<std::size_t>(layout == kRow ? i * ld + j : j * ld + i);
  }
  T& At(int i, int j) { return data[Offset(i, j)]; }
  [[nodiscard]] T At(int i, int j) const { return data[Offset(i, j)]; }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data(), rows, columns, layout, ld,
        {data.data(), sizeof(data), kHost}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data(), rows, columns, layout, ld,
        {data.data(), sizeof(data), kHost}));
  }
  void CheckPadding(TestContext& test) const {
    for (std::size_t i = 0; i < data.size(); ++i) {
      if (i / ld >= static_cast<std::size_t>(layout == kRow ? rows : columns) ||
          i % ld >= static_cast<std::size_t>(layout == kRow ? columns : rows)) {
        ASC_DENSE_TEST_EQ(test, data[i], Value<T>(-791, 37));
      }
    }
  }
};

template <typename T>
struct Scratch {
  alignas(std::max_align_t) std::array<std::byte, 1024> integers;
  std::array<T, 10000> packing;
  std::array<T, 1026> scalar;
  std::size_t scalar_entries = 0;
  std::size_t integer_bytes = 0;
  std::size_t packing_entries = 0;
  Scratch() {
    integers.fill(std::byte{0x5a});
    packing.fill(Value<T>(-797, 41));
    scalar.fill(Value<T>(-809, 43));
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan,
                                 bool preferred = false) {
    asc::LapackWorkspace result;
    integer_bytes =
        static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
        plan.regions[kInteger].entry_bytes;
    packing_entries =
        static_cast<std::size_t>(plan.regions[kLayout].minimum_entries);
    result.regions[kInteger] = {integers.data() + 16, integer_bytes, kHost};
    if (packing_entries != 0) {
      result.regions[kLayout] = {packing.data() + 1,
                                 packing_entries * sizeof(T), kHost};
    }
    scalar_entries = static_cast<std::size_t>(
        preferred ? plan.regions[kScalar].preferred_entries
                  : plan.regions[kScalar].minimum_entries);
    if (scalar_entries != 0) {
      result.regions[kScalar] = {scalar.data() + 1, scalar_entries * sizeof(T),
                                 kHost};
    }
    return result;
  }
  asc::LapackWorkspace QueryWorkspace(int n) {
    integer_bytes = static_cast<std::size_t>(n) * (ASC_LAPACK_INTEGER_BITS / 8);
    asc::LapackWorkspace result;
    result.regions[kInteger] = {integers.data() + 16, integer_bytes, kHost};
    return result;
  }
  void CheckGuards(TestContext& test) const {
    for (std::size_t i = 0; i < scalar.size(); ++i) {
      if (i == 0 || i > scalar_entries) {
        ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-809, 43));
      }
    }
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + integer_bytes) {
        ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
      }
    }
    for (std::size_t i = 0; i < packing.size(); ++i) {
      if (i == 0 || i > packing_entries) {
        ASC_DENSE_TEST_EQ(test, packing[i], Value<T>(-797, 41));
      }
    }
  }
};

inline auto Pivots(std::array<asc::index_t, 69>& values, int count) {
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      values.data() + 1, count, 1, {values.data(), sizeof(values), kHost}));
}
inline auto Raw(const std::array<asc::index_t, 69>& values, int count) {
  return Take(asc::RawLapackPivotView::Create(
      values.data() + 1, count, kLu, {values.data(), sizeof(values), kHost}));
}

template <typename Operation>
auto Observe(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto count = asc_lapack_test::EndAllocationAudit();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp.count(), 0));
  return result;
}

template <typename T>
void Fill(Matrix<T>& matrix, bool singular = false) {
  if (singular) {
    for (int i = 0; i < matrix.rows; ++i) {
      for (int j = 0; j < matrix.columns; ++j) {
        matrix.At(i, j) =
            i == j && i + 1 < std::min(matrix.rows, matrix.columns)
                ? Value<T>(i + 2)
                : T{};
      }
    }
    return;
  }
  if (matrix.rows > 4 || matrix.columns > 4) {
    for (int i = 0; i < matrix.rows; ++i) {
      for (int j = 0; j < matrix.columns; ++j) {
        matrix.At(i, j) = i == j ? Value<T>(2, 1) : T{};
      }
    }
    for (int j = 0; j < matrix.columns; ++j) {
      std::swap(matrix.At(0, j), matrix.At(matrix.rows - 1, j));
    }
    return;
  }
  for (int i = 0; i < matrix.rows; ++i) {
    for (int j = 0; j < matrix.columns; ++j) {
      matrix.At(i, j) = Value<T>((i == j ? 11 : 0) + (i * 3 + j * 2) % 5 - 2,
                                 (i + j * 2) % 3 - 1);
    }
  }
  if (matrix.rows > 1 && matrix.columns > 0) {
    matrix.At(0, 0) = T{};
    matrix.At(matrix.rows - 1, 0) = Value<T>(21, 2);
  }
}

template <typename T>
void Reconstruction(TestContext& test, const Matrix<T>& original,
                    const Matrix<T>& factor,
                    const std::array<asc::index_t, 69>& pivots) {
  auto permuted = original;
  const int k = std::min(original.rows, original.columns);
  for (int i = 0; i < k; ++i) {
    ASC_DENSE_TEST_CHECK(
        test, pivots[i + 1] >= i + 1 && pivots[i + 1] <= original.rows);
    if (pivots[i + 1] < i + 1 || pivots[i + 1] > original.rows) {
      return;
    }
    for (int j = 0; j < original.columns; ++j) {
      std::swap(permuted.At(i, j),
                permuted.At(static_cast<int>(pivots[i + 1]) - 1, j));
    }
  }
  for (int i = 0; i < original.rows; ++i) {
    for (int j = 0; j < original.columns; ++j) {
      decltype(Wide(T{})) sum{};
      for (int x = 0; x < k; ++x) {
        T l{};
        if (i == x) {
          l = T{1};
        } else if (i > x) {
          l = factor.At(i, x);
        }
        const T u = x <= j ? factor.At(x, j) : T{};
        sum += Wide(l) * Wide(u);
      }
      ASC_DENSE_TEST_CHECK(
          test,
          std::abs(sum - Wide(permuted.At(i, j))) <=
              512.L *
                  std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
    }
  }
}

}  // namespace asc_lu_pivot_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_PIVOT_TEST_SUPPORT_H_
