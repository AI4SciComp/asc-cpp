#ifndef ASC_TESTS_DENSE_LAPACK_BAND_ESTIMATION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_ESTIMATION_TEST_SUPPORT_H_

#include <algorithm>
#include <complex>
#include <cstddef>
#include <vector>

#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"
#include "band_cholesky_test_support.h"

namespace asc_band_estimation_test {
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);

template <typename T>
struct WorkspaceStorage {
  using Real = asc::DenseBlasRealType<T>;
  asc_band_test::Storage<T> packing;
  std::vector<T> scalar;
  std::vector<Real> real;
  std::vector<std::byte> integer;

  explicit WorkspaceStorage(const asc::LapackWorkspacePlan& plan)
      : packing(plan),
        scalar(
            static_cast<std::size_t>(plan.regions[kScalar].minimum_entries) + 2,
            asc_band_test::Value<T>(-211, 23)),
        real(static_cast<std::size_t>(plan.regions[kReal].minimum_entries) + 2,
             Real{-223}),
        integer(
            static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
                    plan.regions[kInteger].entry_bytes +
                16,
            std::byte{0x5a}) {}

  asc::LapackWorkspace View() {
    auto result = packing.View();
    result.regions[kScalar] = {scalar.data() + 1,
                               (scalar.size() - 2) * sizeof(T),
                               asc_band_test::kHost};
    result.regions[kReal] = {real.data() + 1, (real.size() - 2) * sizeof(Real),
                             asc_band_test::kHost};
    result.regions[kInteger] = {integer.data() + 8, integer.size() - 16,
                                asc_band_test::kHost};
    return result;
  }

  void Check(asc_dense_test::TestContext& test) const {
    packing.Check(test);
    ASC_DENSE_TEST_EQ(test, scalar.front(), asc_band_test::Value<T>(-211, 23));
    ASC_DENSE_TEST_EQ(test, scalar.back(), asc_band_test::Value<T>(-211, 23));
    ASC_DENSE_TEST_EQ(test, real.front(), Real{-223});
    ASC_DENSE_TEST_EQ(test, real.back(), Real{-223});
    for (std::size_t i = 0; i < 8; ++i) {
      ASC_DENSE_TEST_EQ(test, integer[i], std::byte{0x5a});
      ASC_DENSE_TEST_EQ(test, integer[integer.size() - 1 - i], std::byte{0x5a});
    }
  }
};

inline long double OneNorm(const std::vector<asc_band_test::Wide>& matrix,
                           std::size_t n) {
  long double result = 0;
  for (std::size_t j = 0; j < n; ++j) {
    long double sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
      sum += std::abs(matrix[i * n + j]);
    }
    result = std::max(result, sum);
  }
  return result;
}

// Independent wide Gauss-Jordan oracle on the known positive-definite fixture.
// Its leading pivots are nonzero; no tested provider routine constructs it.
inline long double InverseOneNorm(std::vector<asc_band_test::Wide> matrix,
                                  std::size_t n) {
  std::vector<asc_band_test::Wide> inverse(n * n);
  for (std::size_t i = 0; i < n; ++i) {
    inverse[i * n + i] = 1;
  }
  for (std::size_t k = 0; k < n; ++k) {
    const auto pivot = matrix[k * n + k];
    for (std::size_t j = 0; j < n; ++j) {
      matrix[k * n + j] /= pivot;
      inverse[k * n + j] /= pivot;
    }
    for (std::size_t i = 0; i < n; ++i) {
      if (i == k) {
        continue;
      }
      const auto multiplier = matrix[i * n + k];
      for (std::size_t j = 0; j < n; ++j) {
        matrix[i * n + j] -= multiplier * matrix[k * n + j];
        inverse[i * n + j] -= multiplier * inverse[k * n + j];
      }
    }
  }
  return OneNorm(inverse, n);
}
}  // namespace asc_band_estimation_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_ESTIMATION_TEST_SUPPORT_H_
