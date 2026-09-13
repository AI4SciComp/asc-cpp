#include "band_condition_fault_support.h"

#include <complex>
#include <cstddef>
#include <cstdint>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "asc/dense/blas.h"

namespace {
lapack_int g_info = 0;
double g_result = 0;
std::size_t g_calls = 0;
bool g_valid = true;

template <typename T, typename Real, typename Auxiliary>
void Condition(const char* uplo, const lapack_int* n, const lapack_int* kd,
               const T* ab, const lapack_int* ldab, const Real* anorm,
               Real* rcond, T* work, Auxiliary* auxiliary, lapack_int* info,
               std::size_t length) {
  ++g_calls;
  if (uplo == nullptr || n == nullptr || kd == nullptr || ab == nullptr ||
      ldab == nullptr || anorm == nullptr || rcond == nullptr ||
      work == nullptr || auxiliary == nullptr || info == nullptr) {
    g_valid = false;
    return;
  }
  g_valid = g_valid && length == 1 && (*uplo == 'U' || *uplo == 'L') &&
            *n >= 0 && *kd >= 0 && *ldab > *kd && *anorm >= 0;
  if (*n > 0 && *anorm > 0) {
    const lapack_int count = (asc::DenseBlasComplex<T> ? 2 : 3) * *n;
    for (lapack_int i = 0; i < count; ++i) {
      work[i] = T{-239};
    }
    for (lapack_int i = 0; i < *n; ++i) {
      auxiliary[i] = Auxiliary{17};
    }
  }
  *rcond = static_cast<Real>(g_result);
  *info = g_info;
}
}  // namespace

// Typed GNU ld fault endpoints are ABI/report evidence, not numerical results.
// NOLINTBEGIN(bugprone-reserved-identifier)

extern "C" void __wrap_spbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const float* ab,
                               const lapack_int* ldab, const float* anorm,
                               float* rcond, float* work, lapack_int* auxiliary,
                               lapack_int* info, std::size_t length) {
  Condition(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary, info, length);
}

extern "C" void __wrap_dpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const double* ab,
                               const lapack_int* ldab, const double* anorm,
                               double* rcond, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t length) {
  Condition(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary, info, length);
}

extern "C" void __wrap_cpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, const float* anorm,
                               float* rcond, std::complex<float>* work,
                               float* auxiliary, lapack_int* info,
                               std::size_t length) {
  Condition(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary, info, length);
}

extern "C" void __wrap_zpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, const double* anorm,
                               double* rcond, std::complex<double>* work,
                               double* auxiliary, lapack_int* info,
                               std::size_t length) {
  Condition(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary, info, length);
}
// NOLINTEND(bugprone-reserved-identifier)

namespace asc_band_condition_test {
void ResetFault(std::int64_t info, double result) {
  g_info = static_cast<lapack_int>(info);
  g_result = result;
  g_calls = 0;
  g_valid = true;
}
std::size_t FaultCalls() { return g_calls; }
bool FaultArgumentsValid() { return g_valid; }
}  // namespace asc_band_condition_test
