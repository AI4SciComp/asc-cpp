#ifndef ASC_TESTS_DENSE_LAPACK_SCALAR_SORT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_SCALAR_SORT_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack_scalar_sort.h"
#include "scalar_norm_test_support.h"  // IWYU pragma: export
namespace asc_scalar_sort_test {
using asc_scalar_norm_test::Bits;
using asc_scalar_norm_test::Take;
using asc_scalar_norm_test::TestContext;
using asc_scalar_norm_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr std::array<asc::extent_t, 10> kLengths{0,  1,  2,  5,  20,
                                                 21, 22, 33, 65, 257};
constexpr std::array kOrders{asc::LapackSortOrder::kIncreasing,
                             asc::LapackSortOrder::kDecreasing};
template <typename T>
T Value(std::size_t i, std::size_t n, unsigned pattern, std::size_t seed) {
  const T tiny = std::numeric_limits<T>::denorm_min();
  const T small = std::numeric_limits<T>::min();
  const T large = std::numeric_limits<T>::max();
  const T infinity = std::numeric_limits<T>::infinity();
  const T nan = std::numeric_limits<T>::quiet_NaN();
  const std::array<T, 10> special{-large, -small, -tiny, -T{0},    0,
                                  tiny,   small,  large, infinity, -infinity};
  const std::size_t midpoint =
      n / 2;  // Integer index, intentionally rounded down.
  switch (pattern) {
    case 0:
      return static_cast<T>(i) - static_cast<T>(midpoint);
    case 1:
      return static_cast<T>(n - i) - static_cast<T>(midpoint);
    case 2:
      return (i + seed) % 2 != 0 ? -T{0} : T{0};
    case 3:
      return static_cast<T>((i * 37 + seed * 17) % 7) - 3;
    case 4:
      return special[(i * 37 + seed * 17) % special.size()];
    case 5:
      return (i + seed) % 5 == 0 ? nan : special[(i + seed) % special.size()];
    case 6:
      return i == 0 || i == n / 2 || i + 1 == n
                 ? nan
                 : static_cast<T>((i * 37 + seed) % 101) - 50;
    default:
      return static_cast<T>(i <= n / 2 ? i : n - i) * (seed % 2 != 0 ? -1 : 1);
  }
}
template <typename T>
void Native(const char* direction, const lapack_int* n, T* values,
            lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_slasrt(direction, n, values, info);
  } else {
    LAPACK_dlasrt(direction, n, values, info);
  }
}
template <typename T>
struct Buffer {
  std::array<T, 259> values{};
  asc::extent_t n = 0;
  Buffer(asc::extent_t size, unsigned pattern, unsigned seed) : n(size) {
    values.fill(T{79});
    values.front() = -T{71};
    values.back() = -T{73};
    for (asc::extent_t i = 0; i < n; ++i) {
      values[static_cast<std::size_t>(i) + 1] =
          Value<T>(static_cast<std::size_t>(i), static_cast<std::size_t>(n),
                   pattern, seed);
    }
  }
  auto View() {
    return Take(asc::DenseBlasVectorView<T>::Create(
        values.data() + 1, n, 1, {values.data(), sizeof(values), kHost}));
  }
  [[nodiscard]] bool HasNan() const {
    return std::any_of(values.begin() + 1, values.begin() + 1 + n,
                       [](T x) { return std::isnan(x); });
  }
};
template <typename T>
bool Same(const std::array<T, 259>& a, const std::array<T, 259>& b) {
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (Bits(a[i]) != Bits(b[i])) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool Permutation(const Buffer<T>& current, const std::array<T, 259>& before) {
  std::array<decltype(Bits(T{})), 257> a{};
  std::array<decltype(Bits(T{})), 257> b{};
  for (asc::extent_t i = 0; i < current.n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    a[index] = Bits(before[index + 1]);
    b[index] = Bits(current.values[index + 1]);
  }
  std::sort(a.begin(), a.begin() + current.n);
  std::sort(b.begin(), b.begin() + current.n);
  if (a != b) {
    return false;
  }
  for (std::size_t i = 0; i < before.size(); ++i) {
    if ((i == 0 || i > static_cast<std::size_t>(current.n)) &&
        Bits(current.values[i]) != Bits(before[i])) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool Ordered(const Buffer<T>& buffer, asc::LapackSortOrder order) {
  for (asc::extent_t i = 1; i < buffer.n; ++i) {
    const auto index = static_cast<std::size_t>(i);
    if (!(order == asc::LapackSortOrder::kIncreasing
              ? buffer.values[index] <= buffer.values[index + 1]
              : buffer.values[index] >= buffer.values[index + 1])) {
      return false;
    }
  }
  return true;
}
}  // namespace asc_scalar_sort_test
#endif  // ASC_TESTS_DENSE_LAPACK_SCALAR_SORT_TEST_SUPPORT_H_
