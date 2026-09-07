#include "band_expert_fault_support.h"

#include <complex>
#include <cstddef>
#include <cstdint>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "asc/dense/blas.h"

namespace {
lapack_int g_info = 0;
std::size_t g_calls = 0;
bool g_valid = true;
bool g_require_zero_imaginary = false;

template <typename T>
void Driver(const char* uplo, const lapack_int* n, const lapack_int* kd,
            const lapack_int* nrhs, T* ab, const lapack_int* ldab, T* b,
            const lapack_int* ldb, lapack_int* info, std::size_t length) {
  ++g_calls;
  if (uplo == nullptr || n == nullptr || kd == nullptr || nrhs == nullptr ||
      ab == nullptr || ldab == nullptr || b == nullptr || ldb == nullptr ||
      info == nullptr) {
    g_valid = false;
    return;
  }
  g_valid = g_valid && length == 1 && (*uplo == 'U' || *uplo == 'L') &&
            *n >= 0 && *kd >= 0 && *nrhs >= 0 && *ldab > *kd &&
            *ldb >= (*n > 0 ? *n : 1) && ab != nullptr && b != nullptr;
  if constexpr (asc::DenseBlasComplex<T>) {
    if (g_require_zero_imaginary) {
      for (lapack_int j = 0; j < *n; ++j) {
        g_valid =
            g_valid && ab[j * *ldab + (*uplo == 'U' ? *kd : 0)].imag() == 0;
      }
    }
  }
  if (*n > 0) {
    ab[*uplo == 'U' ? *kd : 0] = T{-131};
  }
  if (*n > 0 && *nrhs > 0 && g_info == 0) {
    b[0] = T{-137};
  }
  *info = g_info;
}
template <typename T, typename Real>
void Equilibration(const char* uplo, const lapack_int* n, const lapack_int* kd,
                   const T* ab, const lapack_int* ldab, Real* s, Real* scond,
                   Real* amax, lapack_int* info, std::size_t length) {
  ++g_calls;
  if (uplo == nullptr || n == nullptr || kd == nullptr || ab == nullptr ||
      ldab == nullptr || s == nullptr || scond == nullptr || amax == nullptr ||
      info == nullptr) {
    g_valid = false;
    return;
  }
  g_valid = g_valid && length == 1 && (*uplo == 'U' || *uplo == 'L') &&
            *n >= 0 && *kd >= 0 && *ldab > *kd && ab != nullptr && s != nullptr;
  if (*n > 0) {
    s[0] = Real{-139};
  }
  *scond = Real{-149};
  *amax = Real{-151};
  *info = g_info;
}
}  // namespace

// GNU ld --wrap endpoints are typed fault evidence, never numerical results.
// NOLINTBEGIN(bugprone-reserved-identifier)

extern "C" void __wrap_spbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              float* ab, const lapack_int* ldab, float* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Driver(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_spbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const float* ab,
                               const lapack_int* ldab, float* s, float* scond,
                               float* amax, lapack_int* info,
                               std::size_t length) {
  Equilibration(uplo, n, kd, ab, ldab, s, scond, amax, info, length);
}

extern "C" void __wrap_dpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              double* ab, const lapack_int* ldab, double* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Driver(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_dpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const double* ab,
                               const lapack_int* ldab, double* s, double* scond,
                               double* amax, lapack_int* info,
                               std::size_t length) {
  Equilibration(uplo, n, kd, ab, ldab, s, scond, amax, info, length);
}

extern "C" void __wrap_cpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              std::complex<float>* ab, const lapack_int* ldab,
                              std::complex<float>* b, const lapack_int* ldb,
                              lapack_int* info, std::size_t length) {
  Driver(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_cpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, float* s, float* scond,
                               float* amax, lapack_int* info,
                               std::size_t length) {
  Equilibration(uplo, n, kd, ab, ldab, s, scond, amax, info, length);
}

extern "C" void __wrap_zpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              std::complex<double>* ab, const lapack_int* ldab,
                              std::complex<double>* b, const lapack_int* ldb,
                              lapack_int* info, std::size_t length) {
  Driver(uplo, n, kd, nrhs, ab, ldab, b, ldb, info, length);
}
extern "C" void __wrap_zpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, double* s, double* scond,
                               double* amax, lapack_int* info,
                               std::size_t length) {
  Equilibration(uplo, n, kd, ab, ldab, s, scond, amax, info, length);
}
// NOLINTEND(bugprone-reserved-identifier)

namespace asc_band_expert_test {
void ResetFault(std::int64_t info, bool require_zero_imaginary) {
  g_info = static_cast<lapack_int>(info);
  g_calls = 0;
  g_valid = true;
  g_require_zero_imaginary = require_zero_imaginary;
}
std::size_t FaultCalls() { return g_calls; }
bool FaultArgumentsValid() { return g_valid; }
}  // namespace asc_band_expert_test
