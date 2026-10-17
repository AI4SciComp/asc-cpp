#ifndef ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_REFINEMENT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_REFINEMENT_TEST_SUPPORT_H_
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <utility>

#include "asc/dense/providers/lapack_positive_tridiagonal_refinement.h"
#include "tridiagonal_test_support.h"
namespace asc_ptrfs_test {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::kScratch;
using asc_tridiagonal_test::Narrow;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;
inline constexpr auto kLower = asc::DenseBlasTriangle::kLower;
inline constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
template <typename T>
using Real = asc::DenseBlasRealType<T>;

template <typename T>
struct Storage {
  asc_tridiagonal_test::Scratch<T> native;
  std::array<Real<T>, 32> estimates{};
  Storage() { estimates.fill(Real<T>{-123}); }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    auto workspace = native.Workspace(plan);
    const auto bytes =
        static_cast<std::size_t>(plan.regions[kScratch].preferred_entries) *
        plan.regions[kScratch].entry_bytes;
    if (bytes > (estimates.size() - 2) * sizeof(Real<T>)) {
      std::abort();
    }
    if (bytes != 0) {
      workspace.regions[kScratch] = {estimates.data() + 1, bytes, kHost};
    }
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    native.Guards(test, workspace);
    ASC_DENSE_TEST_EQ(test, estimates.front(), Real<T>{-123});
    for (std::size_t i =
             1 + workspace.regions[kScratch].size() / sizeof(Real<T>);
         i < estimates.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, estimates[i], Real<T>{-123});
    }
  }
};

// Construct ordinary A=L*D*L^H and nominal factors independently, with dyadic
// entries. This fixture does not use PTTRF or a provider solve as its oracle.
template <typename T>
struct Fixture {
  std::array<Real<T>, 11> d{}, df{};
  std::array<T, 11> e{}, ef{};
  std::array<std::array<Wide, 9>, 9> matrix{};
  asc::extent_t n;
  asc::DenseBlasTriangle triangle;
  Fixture(asc::extent_t order, int exponent, asc::DenseBlasTriangle part)
      : n(order), triangle(part) {
    d.fill(Real<T>{-71});
    df.fill(Real<T>{-73});
    e.fill(Value<T>(-75, 3));
    ef.fill(Value<T>(-77, 5));
    std::array<std::array<Wide, 9>, 9> lower{};
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto at = static_cast<std::size_t>(i);
      lower[at][at] = 1;
      df[at + 1] = static_cast<Real<T>>(std::ldexp(2.0L + i % 3, exponent));
      if (i > 0) {
        lower[at][at - 1] =
            ToWide(Value<T>(i % 2 == 0 ? -0.25L : 0.125L, 0.125L));
        ef[at] = Narrow<T>(triangle == kLower ? lower[at][at - 1]
                                              : std::conj(lower[at][at - 1]));
      }
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
      for (std::size_t j = 0; j < static_cast<std::size_t>(n); ++j) {
        for (std::size_t k = 0; k < static_cast<std::size_t>(n); ++k) {
          matrix[i][j] += lower[i][k] * static_cast<long double>(df[k + 1]) *
                          std::conj(lower[j][k]);
        }
      }
      d[i + 1] = static_cast<Real<T>>(matrix[i][i].real());
      if (i > 0) {
        e[i] = Narrow<T>(matrix[i][i - 1]);
      }
    }
  }
  [[nodiscard]] auto Original() const {
    return Take(asc::LapackPositiveDefiniteTridiagonalView<const T>::Create(
        Vector(d, n), Vector(e, n == 0 ? 0 : n - 1)));
  }
  [[nodiscard]] auto Factor(
      const asc::ReferenceLapackProvider& provider) const {
    return Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
        provider, triangle, Vector(df, n), Vector(ef, n == 0 ? 0 : n - 1)));
  }
};
template <typename T>
T Exact(asc::extent_t i, asc::extent_t j) {
  return Value<T>(1 + 0.125L * i - 0.0625L * j, 0.25L + 0.125L * j);
}
template <typename T>
void Initialize(const Fixture<T>& fixture, Rhs<T>& rhs, Rhs<T>& solution) {
  for (asc::extent_t j = 0; j < rhs.columns; ++j) {
    for (asc::extent_t i = 0; i < rhs.rows; ++i) {
      Wide value{};
      for (asc::extent_t k = 0; k < rhs.rows; ++k) {
        value += fixture.matrix[static_cast<std::size_t>(i)]
                               [static_cast<std::size_t>(k)] *
                 ToWide(Exact<T>(k, j));
      }
      rhs.At(i, j) = Narrow<T>(value);
      solution.At(i, j) = Exact<T>(i, j) + Value<T>(0.03125L, -0.015625L);
    }
  }
}
}  // namespace asc_ptrfs_test
#endif  // ASC_TESTS_DENSE_LAPACK_POSITIVE_TRIDIAGONAL_REFINEMENT_TEST_SUPPORT_H_
