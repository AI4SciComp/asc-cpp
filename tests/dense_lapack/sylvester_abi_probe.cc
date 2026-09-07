#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <type_traits>

#include "asc/dense/blas.h"
#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

namespace {
constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
using Integer = std::array<lapack_int, 3>;
using Character = std::array<char, 3>;

template <typename T>
T Value(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
auto Function() {
  if constexpr (std::is_same_v<T, float>) {
    return &LAPACK_strsyl_base;
  } else if constexpr (std::is_same_v<T, double>) {
    return &LAPACK_dtrsyl_base;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return &LAPACK_ctrsyl_base;
  } else {
    return &LAPACK_ztrsyl_base;
  }
}

template <typename T>
T Entry(const std::array<T, 8>& matrix, std::size_t row, std::size_t column,
        char operation) {
  if (operation != 'N') {
    const auto saved = row;
    row = column;
    column = saved;
  }
  const auto value = matrix[1 + row + 3 * column];
  if constexpr (asc::DenseBlasComplex<T>) {
    return operation == 'C' ? std::conj(value) : value;
  } else {
    return value;
  }
}

template <typename T>
T Known(std::size_t row, std::size_t column) {
  return Value<T>(1 + static_cast<int>(row), 1 + static_cast<int>(column));
}

template <typename T>
void Initialize(std::array<T, 8>& a, std::array<T, 8>& b, std::array<T, 8>& c,
                char operation_a, char operation_b, lapack_int sign) {
  a.fill(Value<T>(-173, 23));
  b.fill(Value<T>(-173, 23));
  c.fill(Value<T>(-173, 23));
  a[1] = Value<T>(2, 1);
  a[2] = T{};
  a[4] = Value<T>(1, -1);
  a[5] = Value<T>(3, 1);
  b[1] = Value<T>(5, -1);
  b[2] = T{};
  b[4] = Value<T>(2, 1);
  b[5] = Value<T>(7, -1);
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      T value{};
      for (std::size_t k = 0; k < 2; ++k) {
        value += Entry(a, i, k, operation_a) * Known<T>(k, j) +
                 static_cast<asc::DenseBlasRealType<T>>(sign) * Known<T>(i, k) *
                     Entry(b, k, j, operation_b);
      }
      c[1 + i + 3 * j] = value;
    }
  }
}

template <typename T>
bool One(char operation_a, char operation_b, lapack_int sign, lapack_int rows,
         lapack_int columns, bool warning) {
  using Real = asc::DenseBlasRealType<T>;
  const Character original_a{'a', operation_a, 'z'};
  const Character original_b{'b', operation_b, 'y'};
  Character op_a = original_a;
  Character op_b = original_b;
  Integer isgn{kGuard, sign, kGuard};
  Integer m{kGuard, rows, kGuard};
  Integer n{kGuard, columns, kGuard};
  Integer lda{kGuard, 3, kGuard};
  Integer ldb{kGuard, 3, kGuard};
  Integer ldc{kGuard, 3, kGuard};
  Integer info{kGuard, kGuard, kGuard};
  std::array<Real, 3> scale{Real{-79}, Real{-91}, Real{-83}};
  std::array<T, 8> a{};
  std::array<T, 8> b{};
  std::array<T, 8> c{};
  Initialize(a, b, c, operation_a, operation_b, sign);
  if (warning) {
    a[1] = b[1] = T{1};
    c[1] = T{2};
  }
  const auto saved_a = a;
  const auto saved_b = b;
  const auto saved_c = c;
  // This is the actual foreign symbol with two explicit trailing lengths,
  // not an ASC adapter or a fault wrapper that could mask scalar overwrites.
  Function<T>()(&op_a[1], &op_b[1], &isgn[1], &m[1], &n[1], &a[1], &lda[1],
                &b[1], &ldb[1], &c[1], &ldc[1], &scale[1], &info[1],
                std::size_t{1}, std::size_t{1});
  if (op_a != original_a || op_b != original_b ||
      isgn != Integer{kGuard, sign, kGuard} ||
      m != Integer{kGuard, rows, kGuard} ||
      n != Integer{kGuard, columns, kGuard} ||
      lda != Integer{kGuard, 3, kGuard} || ldb != Integer{kGuard, 3, kGuard} ||
      ldc != Integer{kGuard, 3, kGuard} ||
      info != Integer{kGuard, warning ? 1 : 0, kGuard} ||
      scale[0] != Real{-79} || scale[2] != Real{-83} || !(scale[1] > 0) ||
      scale[1] > 1 || a != saved_a || b != saved_b) {
    return false;
  }
  if (rows == 0) {
    return c == saved_c && scale[1] == 1;
  }
  for (std::size_t index = 0; index < c.size(); ++index) {
    const bool output =
        index == 1 || (!warning && (index == 2 || index == 4 || index == 5));
    if (!output && c[index] != saved_c[index]) {
      return false;
    }
  }
  if (warning) {
    return std::isfinite(std::abs(c[1]));
  }
  const Real tolerance = 64 * std::numeric_limits<Real>::epsilon();
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      const auto expected = scale[1] * Known<T>(i, j);
      if (std::abs(c[1 + i + 3 * j] - expected) >
          tolerance * std::abs(expected)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Run() {
  for (const char a : {'N', 'T', 'C'}) {
    for (const char b : {'N', 'T', 'C'}) {
      if (asc::DenseBlasComplex<T> && (a == 'T' || b == 'T')) {
        continue;
      }
      for (const lapack_int sign : {lapack_int{-1}, lapack_int{1}}) {
        if (!One<T>(a, b, sign, 2, 2, false)) {
          return false;
        }
      }
    }
  }
  return One<T>('N', 'N', -1, 1, 1, true) && One<T>('N', 'N', 1, 0, 2, false);
}
}  // namespace

int main() {
  static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
  if (!Run<float>() || !Run<double>() || !Run<std::complex<float>>() ||
      !Run<std::complex<double>>()) {
    std::fputs("Sylvester direct ABI probe failed\n", stderr);
    return 1;
  }
  std::printf("60 direct TRSYL ABI cases passed; INTEGER bits=%d\n",
              ASC_LAPACK_INTEGER_BITS);
}
