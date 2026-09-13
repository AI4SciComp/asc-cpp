#include "band_expert_driver_fault_support.h"

#include <complex>
#include <cstddef>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "asc/dense/blas.h"

namespace {
asc_band_driver_test::FaultConfig g_config;
std::size_t g_calls = 0;
bool g_valid = true;

template <typename T>
T Scalar(double real, double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}
template <typename T>
void Band(const char* fact, const char* uplo, lapack_int n, lapack_int kd, T* a,
          lapack_int lda, T* af, lapack_int ldaf) {
  for (lapack_int j = 0; j < n; ++j) {
    for (lapack_int offset = 0; offset <= kd; ++offset) {
      const lapack_int i = *uplo == 'U' ? j + offset - kd : j + offset;
      if (i < 0 || i >= n) {
        continue;
      }
      if constexpr (asc::DenseBlasComplex<T>) {
        if (i == j && *fact != 'F') {
          g_valid = g_valid && a[j * lda + offset].imag() == 0;
        }
        if (i == j && *fact == 'F' && g_config.check_raw_factor) {
          g_valid = g_valid && af[j * ldaf + offset].imag() == 53;
        }
      }
      if (*fact != 'F') {
        if (g_config.packed_factor_output) {
          g_valid = g_valid && af[j * ldaf + offset] == Scalar<T>(-97, 43);
        }
        af[j * ldaf + offset] = Scalar<T>(-443, 73);
      }
      if (*fact == 'E' && g_config.equilibration == 'Y') {
        a[j * lda + offset] = Scalar<T>(-449);
      }
    }
  }
}
template <typename T, typename Real, typename Auxiliary>
void Driver(const char* fact, const char* uplo, const lapack_int* n,
            const lapack_int* kd, const lapack_int* nrhs, T* a,
            const lapack_int* lda, T* af, const lapack_int* ldaf, char* equed,
            Real* scales, T* b, const lapack_int* ldb, T* x,
            const lapack_int* ldx, Real* rcond, Real* ferr, Real* berr, T* work,
            Auxiliary* auxiliary, lapack_int* info, std::size_t fact_length,
            std::size_t uplo_length, std::size_t equed_length) {
  ++g_calls;
  if (fact == nullptr || uplo == nullptr || n == nullptr || kd == nullptr ||
      nrhs == nullptr || a == nullptr || lda == nullptr || af == nullptr ||
      ldaf == nullptr || equed == nullptr || scales == nullptr ||
      b == nullptr || ldb == nullptr || x == nullptr || ldx == nullptr ||
      rcond == nullptr || ferr == nullptr || berr == nullptr ||
      work == nullptr || auxiliary == nullptr || info == nullptr) {
    g_valid = false;
    return;
  }
  g_valid = g_valid && fact_length == 1 && uplo_length == 1 &&
            equed_length == 1 &&
            (*fact == 'N' || *fact == 'E' || *fact == 'F') &&
            (*uplo == 'U' || *uplo == 'L') && *n >= 0 && *kd >= 0 &&
            *nrhs >= 0 && *lda > *kd && *ldaf > *kd &&
            *ldb >= (*n > 0 ? *n : 1) && *ldx >= (*n > 0 ? *n : 1);
  Band(fact, uplo, *n, *kd, a, *lda, af, *ldaf);
  for (lapack_int j = 0; j < *nrhs; ++j) {
    for (lapack_int i = 0; i < *n; ++i) {
      if (g_config.packed_solution_output) {
        g_valid = g_valid && x[j * *ldx + i] == Scalar<T>(-97, 43);
      }
      x[j * *ldx + i] = T{-457};
      if (*fact != 'N' && g_config.equilibration == 'Y') {
        b[j * *ldb + i] = T{-461};
      }
    }
    ferr[j] = static_cast<Real>(g_config.forward_error);
    berr[j] = static_cast<Real>(g_config.backward_error);
  }
  for (lapack_int i = 0; i < *n; ++i) {
    if (*fact == 'E') {
      scales[i] = Real{2};
    }
    auxiliary[i] = Auxiliary{79};
  }
  const lapack_int count = (asc::DenseBlasComplex<T> ? 2 : 3) * *n;
  for (lapack_int i = 0; i < count; ++i) {
    work[i] = T{-463};
  }
  *equed = g_config.equilibration;
  *rcond = static_cast<Real>(g_config.reciprocal_condition);
  *info = static_cast<lapack_int>(g_config.info);
}
}  // namespace

// Exact pinned typed PBSVX ABI, including all three trailing character lengths.
// NOLINTBEGIN(bugprone-reserved-identifier)

extern "C" void __wrap_spbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, float* ab,
    const lapack_int* ldab, float* afb, const lapack_int* ldafb, char* equed,
    float* scales, float* b, const lapack_int* ldb, float* x,
    const lapack_int* ldx, float* rcond, float* ferr, float* berr, float* work,
    lapack_int* auxiliary, lapack_int* info, std::size_t fact_length,
    std::size_t uplo_length, std::size_t equed_length) {
  Driver(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales, b, ldb,
         x, ldx, rcond, ferr, berr, work, auxiliary, info, fact_length,
         uplo_length, equed_length);
}

extern "C" void __wrap_dpbsvx_(const char* fact, const char* uplo,
                               const lapack_int* n, const lapack_int* kd,
                               const lapack_int* nrhs, double* ab,
                               const lapack_int* ldab, double* afb,
                               const lapack_int* ldafb, char* equed,
                               double* scales, double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* rcond,
                               double* ferr, double* berr, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t fact_length, std::size_t uplo_length,
                               std::size_t equed_length) {
  Driver(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales, b, ldb,
         x, ldx, rcond, ferr, berr, work, auxiliary, info, fact_length,
         uplo_length, equed_length);
}

extern "C" void __wrap_cpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, std::complex<float>* ab,
    const lapack_int* ldab, std::complex<float>* afb, const lapack_int* ldafb,
    char* equed, float* scales, std::complex<float>* b, const lapack_int* ldb,
    std::complex<float>* x, const lapack_int* ldx, float* rcond, float* ferr,
    float* berr, std::complex<float>* work, float* auxiliary, lapack_int* info,
    std::size_t fact_length, std::size_t uplo_length,
    std::size_t equed_length) {
  Driver(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales, b, ldb,
         x, ldx, rcond, ferr, berr, work, auxiliary, info, fact_length,
         uplo_length, equed_length);
}

extern "C" void __wrap_zpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, std::complex<double>* ab,
    const lapack_int* ldab, std::complex<double>* afb, const lapack_int* ldafb,
    char* equed, double* scales, std::complex<double>* b, const lapack_int* ldb,
    std::complex<double>* x, const lapack_int* ldx, double* rcond, double* ferr,
    double* berr, std::complex<double>* work, double* auxiliary,
    lapack_int* info, std::size_t fact_length, std::size_t uplo_length,
    std::size_t equed_length) {
  Driver(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales, b, ldb,
         x, ldx, rcond, ferr, berr, work, auxiliary, info, fact_length,
         uplo_length, equed_length);
}
// NOLINTEND(bugprone-reserved-identifier)
namespace asc_band_driver_test {
void ResetFault(FaultConfig config) {
  g_config = config;
  g_calls = 0;
  g_valid = true;
}
std::size_t FaultCalls() { return g_calls; }
bool FaultArgumentsValid() { return g_valid; }
}  // namespace asc_band_driver_test
