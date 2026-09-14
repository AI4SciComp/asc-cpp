#ifndef ASC_TESTS_DENSE_LAPACK_TRIANGULAR_STORAGE_CONVERSION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_TRIANGULAR_STORAGE_CONVERSION_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <span>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_triangular_storage_conversion.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export

namespace asc_triangular_storage_test {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kScratch = asc_tridiagonal_test::kScratch;
constexpr auto kLayout = asc_tridiagonal_test::kLayout;
constexpr std::array<asc::extent_t, 8> kOrders{0, 1, 2, 3, 5, 8, 17, 33};

template <typename T>
bool Same(const T& a, const T& b) {
  const auto left = std::as_bytes(std::span(&a, 1));
  const auto right = std::as_bytes(std::span(&b, 1));
  return std::equal(left.begin(), left.end(), right.begin());
}

inline bool Selected(asc::DenseBlasTriangle triangle, asc::extent_t i,
                     asc::extent_t j) {
  return triangle == kUpper ? i <= j : i >= j;
}

// Independent closed-form specification oracle, not a native K-loop replay.
inline std::size_t PackedIndex(asc::extent_t n, asc::DenseBlasTriangle triangle,
                               asc::DenseBlasLayout layout, asc::extent_t i,
                               asc::extent_t j) {
  const auto row = static_cast<std::size_t>(i);
  const auto column = static_cast<std::size_t>(j);
  const auto order = static_cast<std::size_t>(n);
  if (layout == kColumn) {
    return triangle == kUpper
               ? column * (column + 1) / 2 + row
               : column * (2 * order - column + 1) / 2 + (row - column);
  }
  return triangle == kLower ? row * (row + 1) / 2 + column
                            : row * (2 * order - row + 1) / 2 + (column - row);
}

template <typename T>
T Fixture(asc::extent_t i, asc::extent_t j, std::size_t seed) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array<Real, 12> values{Real{0},
                                    -Real{0},
                                    Real{1},
                                    Real{-3},
                                    Real{0.25},
                                    Real{-0.125},
                                    std::numeric_limits<Real>::denorm_min(),
                                    std::numeric_limits<Real>::min(),
                                    std::numeric_limits<Real>::max(),
                                    std::numeric_limits<Real>::infinity(),
                                    -std::numeric_limits<Real>::infinity(),
                                    std::numeric_limits<Real>::quiet_NaN()};
  const auto row = static_cast<std::size_t>(i);
  const auto column = static_cast<std::size_t>(j);
  const auto real = values[(row + 3 * column + seed) % values.size()];
  if constexpr (std::is_same_v<T, Real>) {
    return real;
  } else {
    return {real, values[(2 * row + column + seed + 5) % values.size()]};
  }
}

template <typename T>
struct Problem {
  std::array<T, 1280> dense;
  std::array<T, 564> packed;
  asc::extent_t n;
  asc::extent_t leading;
  asc::DenseBlasLayout dense_layout;
  asc::DenseBlasLayout packed_layout;
  asc::DenseBlasTriangle triangle;
  bool to_packed;

  Problem(asc::extent_t order, asc::extent_t padding, asc::DenseBlasLayout dl,
          asc::DenseBlasLayout pl, asc::DenseBlasTriangle part, bool pack,
          std::size_t seed)
      : n(order),
        leading(std::max(asc::extent_t{1}, order) + padding),
        dense_layout(dl),
        packed_layout(pl),
        triangle(part),
        to_packed(pack) {
    dense.fill(T{-97});
    packed.fill(T{-101});
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j < n; ++j) {
        if (to_packed) {
          dense[DenseIndex(i, j)] = Fixture<T>(i, j, seed);
        } else if (Selected(triangle, i, j)) {
          packed[1 + PackedIndex(n, triangle, packed_layout, i, j)] =
              Fixture<T>(i, j, seed);
        }
      }
    }
  }

  [[nodiscard]] std::size_t DenseIndex(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (dense_layout == kColumn ? j * leading + i : i * leading + j));
  }
  [[nodiscard]] auto Dense() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        dense.data() + 1, n, n, dense_layout, leading,
        {dense.data(), sizeof(dense), kHost}));
  }
  [[nodiscard]] auto Packed() {
    return Take(asc::DenseBlasPackedMatrixView<T>::Create(
        packed.data() + 1, n, packed_layout,
        {packed.data(), sizeof(packed), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if (to_packed) {
      return asc::QueryTrttpWorkspace(provider, triangle, ConstDense(),
                                      Packed());
    }
    return asc::QueryTpttrWorkspace(provider, triangle, ConstPacked(), Dense());
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
    if (to_packed) {
      return asc::Trttp(provider, triangle, ConstDense(), Packed(), plan,
                        workspace, report);
    }
    return asc::Tpttr(provider, triangle, ConstPacked(), Dense(), plan,
                      workspace, report);
  }
  [[nodiscard]] asc::DenseBlasMatrixView<const T> ConstDense() {
    return Dense();
  }
  [[nodiscard]] asc::DenseBlasPackedMatrixView<const T> ConstPacked() {
    return Packed();
  }
};

template <typename T>
struct Scratch {
  std::array<T, 1280> layout;
  std::array<T, 1280> staged;
  Scratch() {
    layout.fill(T{-109});
    staged.fill(T{-113});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    for (auto role : {kLayout, kScratch}) {
      auto& data = role == kLayout ? layout : staged;
      const auto count = plan.regions[role].minimum_entries;
      if (count < 0 || static_cast<std::size_t>(count) > data.size() - 2) {
        std::abort();
      }
      if (count > 0) {
        workspace.regions[role] = {data.data() + 1,
                                   static_cast<std::size_t>(count) * sizeof(T),
                                   kHost};
      }
    }
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    for (auto role : {kLayout, kScratch}) {
      const auto& data = role == kLayout ? layout : staged;
      const auto count = workspace.regions[role].size() / sizeof(T);
      for (std::size_t i = 0; i < data.size(); ++i) {
        if (i == 0 || i > count) {
          ASC_DENSE_TEST_EQ(test, data[i], role == kLayout ? T{-109} : T{-113});
        }
      }
    }
  }
};

template <typename T>
void Native(bool pack, char* uplo, lapack_int* n, T* a, lapack_int* ld, T* ap,
            lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    if (pack) {
      LAPACK_strttp(uplo, n, a, ld, ap, info);
    } else {
      LAPACK_stpttr(uplo, n, ap, a, ld, info);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (pack) {
      LAPACK_dtrttp(uplo, n, a, ld, ap, info);
    } else {
      LAPACK_dtpttr(uplo, n, ap, a, ld, info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (pack) {
      LAPACK_ctrttp(uplo, n, a, ld, ap, info);
    } else {
      LAPACK_ctpttr(uplo, n, ap, a, ld, info);
    }
  } else {
    if (pack) {
      LAPACK_ztrttp(uplo, n, a, ld, ap, info);
    } else {
      LAPACK_ztpttr(uplo, n, ap, a, ld, info);
    }
  }
}
}  // namespace asc_triangular_storage_test

#endif  // ASC_TESTS_DENSE_LAPACK_TRIANGULAR_STORAGE_CONVERSION_TEST_SUPPORT_H_
