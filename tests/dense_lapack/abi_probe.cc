#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

extern "C" int AscLapackCComplexProbe(void*, std::size_t, void*, std::size_t);
extern "C" void asc_lapack_fortran_probe(std::complex<float>*,
                                         std::complex<double>*, int*, int*);

namespace {

template <typename T>
bool CheckLibrary() {
  using Scalar = std::complex<T>;
  std::array<Scalar, 2> factor{Scalar{2, 3}, Scalar{79, -83}};
  std::array<lapack_int, 3> pivots{43, 47, 53};
  if constexpr (sizeof(lapack_int) == 8) {
    pivots[1] = static_cast<lapack_int>(0x123456780000002fLL);
  }
  const lapack_int n = 1;
  const lapack_int ld = 2;
  lapack_int info = -97;
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_cgetrf(&n, &n, factor.data(), &ld, pivots.data() + 1, &info);
  } else {
    LAPACK_zgetrf(&n, &n, factor.data(), &ld, pivots.data() + 1, &info);
  }
  if (info != 0 || pivots != std::array<lapack_int, 3>{43, 1, 53} ||
      factor[1] != Scalar{79, -83}) {
    return false;
  }
  for (char trans : {'N', 'T', 'C'}) {
    const Scalar a = trans == 'C' ? std::conj(factor[0]) : factor[0];
    std::array<Scalar, 2> rhs{a * Scalar{5, -7}, Scalar{101, -103}};
    if constexpr (std::is_same_v<T, float>) {
      LAPACK_cgetrs(&trans, &n, &n, factor.data(), &ld, pivots.data() + 1,
                    rhs.data(), &ld, &info);
    } else {
      LAPACK_zgetrs(&trans, &n, &n, factor.data(), &ld, pivots.data() + 1,
                    rhs.data(), &ld, &info);
    }
    if (info != 0 || std::abs(rhs[0] - Scalar{5, -7}) > T{0.00001} ||
        rhs[1] != Scalar{101, -103}) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
  static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
  static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
  static_assert(std::is_trivially_copyable_v<std::complex<float>>);
  static_assert(std::is_trivially_copyable_v<std::complex<double>>);
  static_assert(std::is_standard_layout_v<std::complex<float>>);
  static_assert(std::is_standard_layout_v<std::complex<double>>);
  std::array<std::complex<float>, 2> single{{{2, -3}, {-5, 7}}};
  std::array<std::complex<double>, 2> wide{{{11, -13}, {-17, 19}}};
  // Array-oriented access is expressly supported by C++ [complex.numbers].
  const auto* real_parts = reinterpret_cast<const float*>(single.data());
  if (real_parts[0] != 2 || real_parts[1] != -3 || real_parts[2] != -5 ||
      real_parts[3] != 7 ||
      AscLapackCComplexProbe(single.data(), sizeof(single), wide.data(),
                             sizeof(wide)) != 0) {
    return 1;
  }
  if (single[0] != std::complex<float>{3, 3} ||
      single[1] != std::complex<float>{-10, 14} ||
      wide[0] != std::complex<double>{12, 13} ||
      wide[1] != std::complex<double>{-34, 38}) {
    return 2;
  }
  int integer_bytes = 0;
  int logical_bytes = 0;
  asc_lapack_fortran_probe(single.data(), wide.data(), &integer_bytes,
                           &logical_bytes);
  if (integer_bytes * 8 != ASC_LAPACK_INTEGER_BITS ||
      logical_bytes != integer_bytes ||
      single[0] != std::complex<float>{5, -3} ||
      single[1] != std::complex<float>{10, -14} ||
      wide[0] != std::complex<double>{14, -13} ||
      wide[1] != std::complex<double>{34, -38}) {
    return 3;
  }
  if (!CheckLibrary<float>() || !CheckLibrary<double>()) {
    return 4;
  }
  std::printf(
      "C/C++/Fortran complex array ABI and linked C/Z GETRF/GETRS N/T/C "
      "passed; integer=%d logical=%d\n",
      integer_bytes, logical_bytes);
  return 0;
}
