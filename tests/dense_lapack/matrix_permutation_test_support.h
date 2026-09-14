#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_PERMUTATION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_PERMUTATION_TEST_SUPPORT_H_
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
#include "asc/dense/providers/lapack_matrix_permutation.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export
namespace asc_matrix_permutation_test {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kForward = asc::LapackPermutationDirection::kForward;
constexpr auto kBackward = asc::LapackPermutationDirection::kBackward;
constexpr auto kScratch = asc_tridiagonal_test::kScratch;
constexpr auto kInteger = asc_tridiagonal_test::kInteger;
template <typename T>
bool Same(const T& a, const T& b) {
  const auto x = std::as_bytes(std::span(&a, 1));
  const auto y = std::as_bytes(std::span(&b, 1));
  return std::equal(x.begin(), x.end(), y.begin());
}
template <typename T>
T Value(std::size_t i, std::size_t j, std::size_t seed) {
  using R = asc::DenseBlasRealType<T>;
  const std::array<R, 12> values{R{0},
                                 -R{0},
                                 R{1},
                                 R{-3},
                                 R{0.25},
                                 R{-0.125},
                                 std::numeric_limits<R>::denorm_min(),
                                 std::numeric_limits<R>::min(),
                                 std::numeric_limits<R>::max(),
                                 std::numeric_limits<R>::infinity(),
                                 -std::numeric_limits<R>::infinity(),
                                 std::numeric_limits<R>::quiet_NaN()};
  const auto real = values[(i + 3 * j + seed) % values.size()];
  if constexpr (std::is_same_v<T, R>) {
    return real;
  } else {
    return {real, values[(2 * i + j + seed + 5) % values.size()]};
  }
}
template <typename T>
void Native(bool rows, const lapack_logical* forward, const lapack_int* m,
            const lapack_int* n, T* x, const lapack_int* ld, lapack_int* k) {
  if constexpr (std::is_same_v<T, float>) {
    if (rows) {
      LAPACK_slapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_slapmt(forward, m, n, x, ld, k);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if (rows) {
      LAPACK_dlapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_dlapmt(forward, m, n, x, ld, k);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (rows) {
      LAPACK_clapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_clapmt(forward, m, n, x, ld, k);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (rows) {
      LAPACK_zlapmr(forward, m, n, x, ld, k);
    } else {
      LAPACK_zlapmt(forward, m, n, x, ld, k);
    }
  }
}

template <typename T>
struct Problem {
  std::array<T, 1400> dense;
  std::array<asc::index_t, 35> indices;
  asc::extent_t m, n, leading;
  asc::DenseBlasLayout layout;
  asc::LapackPermutationDirection direction;
  bool rows;
  Problem(asc::extent_t d, asc::extent_t other, asc::extent_t padding,
          asc::DenseBlasLayout storage, bool row_permutation,
          asc::LapackPermutationDirection order, std::size_t seed)
      : m(row_permutation ? d : other),
        n(row_permutation ? other : d),
        leading(std::max(asc::extent_t{1}, storage == kColumn ? m : n) +
                padding),
        layout(storage),
        direction(order),
        rows(row_permutation) {
    dense.fill(T{-97});
    indices.fill(-101);
    for (asc::extent_t i = 0; i < d; ++i) {
      indices[static_cast<std::size_t>(i + 1)] = i + 1;
    }
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < m; ++i) {
        dense[Index(i, j)] = Value<T>(static_cast<std::size_t>(i),
                                      static_cast<std::size_t>(j), seed);
      }
    }
  }
  [[nodiscard]] asc::extent_t Dimension() const { return rows ? m : n; }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * leading + i : i * leading + j));
  }
  [[nodiscard]] auto Matrix() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        dense.data() + 1, m, n, layout, leading,
        {dense.data(), sizeof(dense), kHost}));
  }
  [[nodiscard]] auto Permutation() const {
    return Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
        indices.data() + 1, Dimension(), 1,
        {indices.data(), sizeof(indices), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return rows ? asc::QueryLapmrWorkspace(provider, direction, Matrix(),
                                           Permutation())
                : asc::QueryLapmtWorkspace(provider, direction, Matrix(),
                                           Permutation());
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
    return rows ? asc::Lapmr(provider, direction, Matrix(), Permutation(), plan,
                             workspace, report)
                : asc::Lapmt(provider, direction, Matrix(), Permutation(), plan,
                             workspace, report);
  }
  [[nodiscard]] auto Expected() const {
    auto expected = dense;
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < m; ++i) {
        const auto pi = rows ? indices[static_cast<std::size_t>(i + 1)] - 1 : i;
        const auto pj = rows ? j : indices[static_cast<std::size_t>(j + 1)] - 1;
        if (direction == kForward) {
          expected[Index(i, j)] = dense[Index(pi, pj)];
        } else {
          expected[Index(pi, pj)] = dense[Index(i, j)];
        }
      }
    }
    return expected;
  }
};
template <typename T>
struct Scratch {
  std::array<T, 1400> staged;
  alignas(lapack_int) std::array<std::byte, 35 * sizeof(lapack_int)> integer;
  Scratch() {
    staged.fill(T{-113});
    integer.fill(std::byte{0x5a});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[kScratch].minimum_entries;
    const auto d = plan.regions[kInteger].minimum_entries;
    if (count < 0 || count > 1398 || d < 0 || d > 33) {
      std::abort();
    }
    if (count > 0) {
      workspace.regions[kScratch] = {
          staged.data() + 1, static_cast<std::size_t>(count) * sizeof(T),
          kHost};
    }
    if (d > 0) {
      workspace.regions[kInteger] = {
          integer.data() + sizeof(lapack_int),
          static_cast<std::size_t>(d) * sizeof(lapack_int), kHost};
    }
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    const auto count = workspace.regions[kScratch].size() / sizeof(T);
    for (std::size_t i = 0; i < staged.size(); ++i) {
      if (i == 0 || i > count) {
        ASC_DENSE_TEST_EQ(test, staged[i], T{-113});
      }
    }
    const auto bytes = workspace.regions[kInteger].size();
    for (std::size_t i = 0; i < integer.size(); ++i) {
      if (i < sizeof(lapack_int) || i >= sizeof(lapack_int) + bytes) {
        ASC_DENSE_TEST_EQ(test, integer[i], std::byte{0x5a});
      }
    }
  }
};
template <typename Function>
void Permutations(asc::extent_t d, Function function) {
  std::array<asc::index_t, 33> k{};
  for (asc::extent_t i = 0; i < d; ++i) {
    k[static_cast<std::size_t>(i)] = i + 1;
  }
  std::size_t variant = 0;
  do {
    function(k);
    ++variant;
    if (d <= 5) {
      if (!std::next_permutation(k.begin(), k.begin() + d)) {
        break;
      }
    } else {
      if (variant == 4) {
        break;
      }
      if (variant == 1) {
        std::reverse(k.begin(), k.begin() + d);
      }
      if (variant == 2) {
        for (asc::extent_t i = 0; i < d; ++i) {
          k[static_cast<std::size_t>(i)] = (i + 1) % d + 1;
        }
      }
      if (variant == 3) {
        for (asc::extent_t i = 0; i < d; ++i) {
          k[static_cast<std::size_t>(i)] = i + 1;
        }
        for (asc::extent_t i = 0; i + 2 < d; i += 3) {
          std::rotate(k.begin() + i, k.begin() + i + 1, k.begin() + i + 3);
        }
      }
    }
  } while (true);
}
}  // namespace asc_matrix_permutation_test
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_PERMUTATION_TEST_SUPPORT_H_
