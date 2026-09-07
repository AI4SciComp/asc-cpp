#include "band_cholesky_fault_support.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../../src/dense/lapack/internal_band_abi.h"

namespace {
lapack_int g_info = 0;
std::size_t g_calls = 0;
bool g_valid = true;
bool g_require_zero_imaginary = false;

template <typename T>
void Factor(const char* uplo, const lapack_int* n, const lapack_int* kd, T* ab,
            const lapack_int* ldab, lapack_int* info, std::size_t length) {
  ++g_calls;
  g_valid = g_valid && length == 1 && (*uplo == 'U' || *uplo == 'L') &&
            *n >= 0 && *kd >= 0 && *ldab > *kd && ab != nullptr;
  if constexpr (!std::is_arithmetic_v<T>) {
    // Inspect the actual foreign input before mutation: ignored original
    // diagonal components must not be copied into factorization workspace.
    if (g_require_zero_imaginary) {
      for (lapack_int i = 0; i < *n; ++i) {
        g_valid =
            g_valid && ab[i * *ldab + (*uplo == 'U' ? *kd : 0)].imag() == 0;
      }
    }
  }
  if (*n > 0 && ab != nullptr) {
    ab[*uplo == 'U' ? *kd : 0] = T{-131};
  }
  *info = g_info;
}
template <typename T>
void Solve(const char* uplo, const lapack_int* n, const lapack_int* kd,
           const lapack_int* nrhs, const T* ab, const lapack_int* ldab, T* b,
           const lapack_int* ldb, lapack_int* info, std::size_t length) {
  ++g_calls;
  g_valid = g_valid && length == 1 && (*uplo == 'U' || *uplo == 'L') &&
            *n >= 0 && *kd >= 0 && *nrhs >= 0 && *ldab > *kd &&
            *ldb >= (*n > 0 ? *n : 1) && ab != nullptr && b != nullptr;
  if (*n > 0 && *nrhs > 0 && b != nullptr) {
    b[0] = T{-131};
  }
  *info = g_info;
}
}  // namespace

// GNU ld's mandated --wrap names. This executable does not claim numerical
// evidence: these typed fault endpoints never call a foreign routine.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __wrap_spbtrf_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, float* ab,
                               const lapack_int* ldab, lapack_int* info,
                               std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_spbtf2_(char* uplo, lapack_int* n, lapack_int* kd,
                               float* ab, lapack_int* ldab, lapack_int* info,
                               std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_spbtrs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const float* ab, const lapack_int* ldab,
                               float* b, const lapack_int* ldb,
                               lapack_int* info, std::size_t length) {
  Solve(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_dpbtrf_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, double* ab,
                               const lapack_int* ldab, lapack_int* info,
                               std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_dpbtf2_(char* uplo, lapack_int* n, lapack_int* kd,
                               double* ab, lapack_int* ldab, lapack_int* info,
                               std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_dpbtrs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const double* ab, const lapack_int* ldab,
                               double* b, const lapack_int* ldb,
                               lapack_int* info, std::size_t length) {
  Solve(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_cpbtrf_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, std::complex<float>* ab,
                               const lapack_int* ldab, lapack_int* info,
                               std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_cpbtf2_(char* uplo, lapack_int* n, lapack_int* kd,
                               std::complex<float>* ab, lapack_int* ldab,
                               lapack_int* info, std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_cpbtrs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, std::complex<float>* b,
                               const lapack_int* ldb, lapack_int* info,
                               std::size_t length) {
  Solve(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_zpbtrf_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, std::complex<double>* ab,
                               const lapack_int* ldab, lapack_int* info,
                               std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_zpbtf2_(char* uplo, lapack_int* n, lapack_int* kd,
                               std::complex<double>* ab, lapack_int* ldab,
                               lapack_int* info, std::size_t length) {
  Factor(uplo, n, kd, ab, ldab, info, length);
}
extern "C" void __wrap_zpbtrs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, std::complex<double>* b,
                               const lapack_int* ldb, lapack_int* info,
                               std::size_t length) {
  Solve(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
// NOLINTEND(bugprone-reserved-identifier)

namespace asc_band_test {
void ResetFault(std::int64_t info, bool require_zero_imaginary) {
  g_info = static_cast<lapack_int>(info);
  g_calls = 0;
  g_valid = true;
  g_require_zero_imaginary = require_zero_imaginary;
}
std::size_t FaultCalls() { return g_calls; }
bool FaultArgumentsValid() { return g_valid; }
}  // namespace asc_band_test
