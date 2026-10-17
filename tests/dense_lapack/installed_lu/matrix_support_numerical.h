#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MATRIX_SUPPORT_NUMERICAL_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MATRIX_SUPPORT_NUMERICAL_H_
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <type_traits>
namespace asc_matrix_support_numerical {
template <typename T>
auto Bits(const T& value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}
template <typename T>
auto Markers() {
  if constexpr (std::is_floating_point_v<T>) {
    return std::array<T, 10>{T{0},
                             -T{0},
                             T{1},
                             T{-3},
                             std::numeric_limits<T>::denorm_min(),
                             -std::numeric_limits<T>::denorm_min(),
                             std::numeric_limits<T>::max(),
                             -std::numeric_limits<T>::max(),
                             std::numeric_limits<T>::infinity(),
                             std::numeric_limits<T>::quiet_NaN()};
  } else {
    using R = typename T::value_type;
    return std::array<T, 14>{T{0},
                             T{-R{0}, -R{0}},
                             T{1},
                             T{-3},
                             T{std::numeric_limits<R>::denorm_min()},
                             T{-std::numeric_limits<R>::denorm_min()},
                             T{std::numeric_limits<R>::max()},
                             T{-std::numeric_limits<R>::max()},
                             T{std::numeric_limits<R>::infinity()},
                             T{std::numeric_limits<R>::quiet_NaN()},
                             T{0, 1},
                             T{0, std::numeric_limits<R>::denorm_min()},
                             T{0, std::numeric_limits<R>::quiet_NaN()},
                             T{0, std::numeric_limits<R>::infinity()}};
  }
}

template <typename T>
void Fill(T* data, int rows, int columns, int leading, bool row_major,
          int pattern, T marker) {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      const bool selected = pattern == j * rows + i ||
                            pattern == rows * columns + 1 ||
                            (pattern == rows * columns + 2 && (i + j) % 2 == 0);
      data[row_major ? i * leading + j : j * leading + i] =
          selected ? marker : T{-0.0};
    }
  }
}
template <bool Rows, typename T>
int Expected(const T* data, int rows, int columns, int leading,
             bool row_major) {
  int result = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (data[row_major ? i * leading + j : j * leading + i] != T{0}) {
        result = std::max(result, Rows ? i + 1 : j + 1);
      }
    }
  }
  return result;
}
template <typename T, typename Function>
void ForEachPositive(Function function) {
  for (int m : {1, 2, 3, 5}) {
    for (int n : {1, 2, 3, 5}) {
      for (int padding : {0, 2}) {
        for (T marker : Markers<T>()) {
          for (int pattern = 0; pattern < m * n + 3; ++pattern) {
            function(m, n, padding, pattern, marker);
          }
        }
      }
    }
  }
}
template <typename T, typename Function>
void ForEach(Function function) {
  ForEachPositive<T>(function);
  for (int m : {0, 1, 3}) {
    for (int n : {0, 1, 3}) {
      if (m == 0 || n == 0) {
        for (int padding : {0, 2}) {
          function(m, n, padding, 0, T{1});
        }
      }
    }
  }
}
}  // namespace asc_matrix_support_numerical
#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MATRIX_SUPPORT_NUMERICAL_H_
