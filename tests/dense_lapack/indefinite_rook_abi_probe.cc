#if defined(ASC_INDEFINITE_ROOK_EMITTED_PROTOTYPES)
// The build selects the untouched compiler emissions with test-only symbol
// renames. Stop those renames before including ASC's independently derived
// facade; compare actual function types without redundant declarations.
#include ASC_INDEFINITE_ROOK_EMITTED_SSYTF2_ROOK_HEADER
#include ASC_INDEFINITE_ROOK_EMITTED_DSYTF2_ROOK_HEADER
#include ASC_INDEFINITE_ROOK_EMITTED_CSYTF2_ROOK_HEADER
#include ASC_INDEFINITE_ROOK_EMITTED_ZSYTF2_ROOK_HEADER
#include ASC_INDEFINITE_ROOK_EMITTED_CHETF2_ROOK_HEADER
#include ASC_INDEFINITE_ROOK_EMITTED_ZHETF2_ROOK_HEADER
#undef ssytf2_rook_
#undef dsytf2_rook_
#undef csytf2_rook_
#undef zsytf2_rook_
#undef chetf2_rook_
#undef zhetf2_rook_
#endif

#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_prototypes.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

#if defined(ASC_INDEFINITE_ROOK_EMITTED_PROTOTYPES)
static_assert(std::is_same_v<decltype(ssytf2_rook_),
                             decltype(asc_probe_emitted_ssytf2_rook)>);
static_assert(std::is_same_v<decltype(dsytf2_rook_),
                             decltype(asc_probe_emitted_dsytf2_rook)>);
static_assert(std::is_same_v<decltype(csytf2_rook_),
                             decltype(asc_probe_emitted_csytf2_rook)>);
static_assert(std::is_same_v<decltype(zsytf2_rook_),
                             decltype(asc_probe_emitted_zsytf2_rook)>);
static_assert(std::is_same_v<decltype(chetf2_rook_),
                             decltype(asc_probe_emitted_chetf2_rook)>);
static_assert(std::is_same_v<decltype(zhetf2_rook_),
                             decltype(asc_probe_emitted_zhetf2_rook)>);
#endif

namespace {

template <typename T>
T Value(int real, int imaginary = 0) {
  if constexpr (asc::DenseBlasComplex<T>) {
    using Real = asc::DenseBlasRealType<T>;
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
bool Probe(bool hermitian) {
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  for (const char triangle : {'U', 'L'}) {
    for (const bool singular : {false, true}) {
      std::array<char, 3> uplo{'a', triangle, 'z'};
      std::array<lapack_int, 3> n{kGuard, singular ? 1 : 2, kGuard};
      std::array<lapack_int, 3> lda{kGuard, 2, kGuard};
      std::array<lapack_int, 3> info{kGuard, kGuard, kGuard};
      std::array<lapack_int, 4> pivots{kGuard, kGuard, kGuard, kGuard};
      std::array<T, 6> matrix{Value<T>(-173, 23),
                              T{},
                              Value<T>(1, 1),
                              Value<T>(1, hermitian ? -1 : 1),
                              T{},
                              Value<T>(-173, 23)};
      // Invoke the compiler-emitted prototypes directly, not an ASC wrapper
      // which could conceal a mismatch by copying scalar argument sentinels.
      if constexpr (std::is_same_v<T, float>) {
        ssytf2_rook_(&uplo[1], &n[1], &matrix[1], &lda[1], &pivots[1], &info[1],
                     std::size_t{1});
      } else if constexpr (std::is_same_v<T, double>) {
        dsytf2_rook_(&uplo[1], &n[1], &matrix[1], &lda[1], &pivots[1], &info[1],
                     std::size_t{1});
      } else if constexpr (std::is_same_v<T, std::complex<float>>) {
        if (hermitian) {
          chetf2_rook_(&uplo[1], &n[1], &matrix[1], &lda[1], &pivots[1],
                       &info[1], std::size_t{1});
        } else {
          csytf2_rook_(&uplo[1], &n[1], &matrix[1], &lda[1], &pivots[1],
                       &info[1], std::size_t{1});
        }
      } else {
        if (hermitian) {
          zhetf2_rook_(&uplo[1], &n[1], &matrix[1], &lda[1], &pivots[1],
                       &info[1], std::size_t{1});
        } else {
          zsytf2_rook_(&uplo[1], &n[1], &matrix[1], &lda[1], &pivots[1],
                       &info[1], std::size_t{1});
        }
      }
      lapack_int pivot = 1;
      if (!singular) {
        pivot = -1;
      }
      if (uplo != std::array{'a', triangle, 'z'} || n[0] != kGuard ||
          n[1] != (singular ? 1 : 2) || n[2] != kGuard || lda[0] != kGuard ||
          lda[1] != 2 || lda[2] != kGuard || info[0] != kGuard ||
          info[1] != (singular ? 1 : 0) || info[2] != kGuard ||
          pivots[0] != kGuard || pivots[1] != pivot ||
          pivots[2] != (singular ? kGuard : -2) || pivots[3] != kGuard ||
          matrix.front() != Value<T>(-173, 23) ||
          matrix.back() != Value<T>(-173, 23)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (!Probe<float>(false) || !Probe<double>(false) ||
      !Probe<std::complex<float>>(false) ||
      !Probe<std::complex<double>>(false) ||
      !Probe<std::complex<float>>(true) || !Probe<std::complex<double>>(true)) {
    std::fputs("Indefinite ABI probe failed\n", stderr);
    return 1;
  }
  std::printf("24 direct TF2 ABI cases passed; INTEGER bits=%d\n",
              ASC_LAPACK_INTEGER_BITS);
}
