#include "band_refinement_fault_support.h"

#include <complex>
#include <cstddef>
#include <cstdint>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "asc/dense/blas.h"

namespace {
lapack_int g_info = 0;
double g_ferr = 0;
double g_berr = 0;
std::size_t g_calls = 0;
bool g_valid = true;
bool g_original_imaginary = false;

template <typename T, typename Real, typename Auxiliary>
void Refinement(const char* uplo, const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const T* a, const lapack_int* lda,
                const T* af, const lapack_int* ldaf, const T* b,
                const lapack_int* ldb, T* x, const lapack_int* ldx, Real* ferr,
                Real* berr, T* work, Auxiliary* auxiliary, lapack_int* info,
                std::size_t length) {
  ++g_calls;
  if (uplo == nullptr || n == nullptr || kd == nullptr || nrhs == nullptr ||
      a == nullptr || lda == nullptr || af == nullptr || ldaf == nullptr ||
      b == nullptr || ldb == nullptr || x == nullptr || ldx == nullptr ||
      ferr == nullptr || berr == nullptr || work == nullptr ||
      auxiliary == nullptr || info == nullptr) {
    g_valid = false;
    return;
  }
  g_valid = g_valid && length == 1 && (*uplo == 'U' || *uplo == 'L') &&
            *n >= 0 && *kd >= 0 && *nrhs >= 0 && *lda > *kd && *ldaf > *kd &&
            *ldb >= (*n > 0 ? *n : 1) && *ldx >= (*n > 0 ? *n : 1);
  if (*n > 0 && *nrhs > 0) {
    if constexpr (asc::DenseBlasComplex<T>) {
      for (lapack_int j = 0; j < *n; ++j) {
        const auto diag = *uplo == 'U' ? *kd : 0;
        if (g_original_imaginary) {
          g_valid = g_valid && a[j * *lda + diag].imag() == 0;
        }
        g_valid = g_valid && af[j * *ldaf + diag].imag() == 53;
      }
    }
    const lapack_int count = (asc::DenseBlasComplex<T> ? 2 : 3) * *n;
    for (lapack_int i = 0; i < count; ++i) {
      work[i] = T{-337};
    }
    for (lapack_int i = 0; i < *n; ++i) {
      auxiliary[i] = Auxiliary{41};
    }
    for (lapack_int j = 0; j < *nrhs; ++j) {
      for (lapack_int i = 0; i < *n; ++i) {
        x[j * *ldx + i] = T{-347};
      }
    }
  }
  for (lapack_int j = 0; j < *nrhs; ++j) {
    ferr[j] = static_cast<Real>(g_ferr);
    berr[j] = static_cast<Real>(g_berr);
  }
  *info = g_info;
}
}  // namespace

// Fault endpoints only: all raw declarations use the audited pinned prototype.
// NOLINTBEGIN(bugprone-reserved-identifier)

extern "C" void __wrap_spbrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const float* a, const lapack_int* lda,
                               const float* af, const lapack_int* ldaf,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               float* work, lapack_int* auxiliary,
                               lapack_int* info, std::size_t length) {
  Refinement(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr, berr,
             work, auxiliary, info, length);
}

extern "C" void __wrap_dpbrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const double* a, const lapack_int* lda,
                               const double* af, const lapack_int* ldaf,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* ferr,
                               double* berr, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t length) {
  Refinement(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr, berr,
             work, auxiliary, info, length);
}

extern "C" void __wrap_cpbrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* kd,
    const lapack_int* nrhs, const std::complex<float>* a, const lapack_int* lda,
    const std::complex<float>* af, const lapack_int* ldaf,
    const std::complex<float>* b, const lapack_int* ldb, std::complex<float>* x,
    const lapack_int* ldx, float* ferr, float* berr, std::complex<float>* work,
    float* auxiliary, lapack_int* info, std::size_t length) {
  Refinement(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr, berr,
             work, auxiliary, info, length);
}

extern "C" void __wrap_zpbrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* kd,
    const lapack_int* nrhs, const std::complex<double>* a,
    const lapack_int* lda, const std::complex<double>* af,
    const lapack_int* ldaf, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* ferr, double* berr, std::complex<double>* work, double* auxiliary,
    lapack_int* info, std::size_t length) {
  Refinement(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr, berr,
             work, auxiliary, info, length);
}
// NOLINTEND(bugprone-reserved-identifier)
namespace asc_band_refinement_test {
void ResetFault(std::int64_t info, double forward_error, double backward_error,
                bool require_zero_original_imaginary) {
  g_info = static_cast<lapack_int>(info);
  g_ferr = forward_error;
  g_berr = backward_error;
  g_calls = 0;
  g_valid = true;
  g_original_imaginary = require_zero_original_imaginary;
}
std::size_t FaultCalls() { return g_calls; }
bool FaultArgumentsValid() { return g_valid; }
}  // namespace asc_band_refinement_test
