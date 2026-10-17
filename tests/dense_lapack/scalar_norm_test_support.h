#ifndef ASC_TESTS_DENSE_LAPACK_SCALAR_NORM_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_SCALAR_NORM_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/core/memory.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_scalar_norm.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export
namespace asc_scalar_norm_test {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
template <typename T>
auto Bits(T value) {
  using Integer =
      std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
  return std::bit_cast<Integer>(value);
}
template <typename T>
std::array<T, 20> Values() {
  const T tiny = std::numeric_limits<T>::denorm_min();
  const T small = std::numeric_limits<T>::min();
  const T large = std::numeric_limits<T>::max();
  return {0,
          -T{0},
          tiny,
          -tiny,
          small,
          -small,
          1,
          -1,
          3,
          -3,
          4,
          -4,
          large / 4,
          -large / 4,
          large / 2,
          -large / 2,
          large,
          -large,
          std::numeric_limits<T>::infinity(),
          std::numeric_limits<T>::quiet_NaN()};
}
template <typename T>
T Native(T x, T y, T z, bool three) {
  if constexpr (std::is_same_v<T, float>) {
    return three ? LAPACK_slapy3(&x, &y, &z) : LAPACK_slapy2(&x, &y);
  } else {
    return three ? LAPACK_dlapy3(&x, &y, &z) : LAPACK_dlapy2(&x, &y);
  }
}
template <typename T>
auto Execute(const asc::ReferenceLapackProvider& provider, T x, T y, T z,
             bool three, asc::DenseBlasVectorView<T> result,
             asc::LapackReport& report) {
  return three ? asc::Lapy3(provider, x, y, z, result, report)
               : asc::Lapy2(provider, x, y, result, report);
}
template <typename T>
T Expected(T x, T y, T z, bool three) {
  if (std::isnan(x) || std::isnan(y) || (three && std::isnan(z))) {
    return std::numeric_limits<T>::quiet_NaN();
  }
  if (std::isinf(x) || std::isinf(y) || (three && std::isinf(z))) {
    return std::numeric_limits<T>::infinity();
  }
  // The local libstdc++11 three-argument hypot produces NaN for infinity.
  // Classify nonfinite inputs separately; retain the full finite Cartesian
  // grid.
  return three ? std::hypot(x, y, z) : std::hypot(x, y);
}
template <typename T>
bool Accurate(T actual, T expected) {
  if (std::isnan(expected)) {
    return std::isnan(actual);
  }
  if (std::isinf(expected)) {
    return std::isinf(actual) && actual > 0;
  }
  if (!std::isfinite(actual) || actual < 0 || std::signbit(actual)) {
    return false;
  }
  const long double error =
      std::abs(static_cast<long double>(actual) - expected);
  const long double tolerance = std::max(
      4.L * std::numeric_limits<T>::epsilon() * expected,
      static_cast<long double>(std::numeric_limits<T>::denorm_min()) / 2);
  return error <= tolerance;
}
template <typename T>
struct Output {
  std::array<T, 3> values{T{71}, T{-19}, T{73}};
  auto View() {
    return Take(asc::DenseBlasVectorView<T>::Create(
        values.data() + 1, 1, 1, {values.data(), sizeof(values), kHost}));
  }
};
}  // namespace asc_scalar_norm_test
#endif  // ASC_TESTS_DENSE_LAPACK_SCALAR_NORM_TEST_SUPPORT_H_
