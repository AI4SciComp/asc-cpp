#include "band_cholesky_fault_support.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_band_abi.h"

namespace {
lapack_int g_info = 0;
std::size_t g_calls = 0;
bool g_valid = true;
bool g_require_zero_imaginary = false;
asc_band_test::InfoWrite g_info_write = asc_band_test::InfoWrite::kComplete;

void WriteInfo(lapack_int* info) {
  if (g_info_write == asc_band_test::InfoWrite::kComplete) {
    *info = g_info;
  } else if (g_info_write == asc_band_test::InfoWrite::kLowWordOnly) {
    // Simulate a provider writing a 32-bit zero through a 64-bit INFO slot.
    // memcpy accesses the actual object's bytes without an aliased int lvalue.
    const std::int32_t zero = 0;
    std::memcpy(info, &zero, sizeof(zero));
  }
}

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
  WriteInfo(info);
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
  WriteInfo(info);
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

static_assert(
    std::is_same_v<decltype(&__wrap_spbtrf_), decltype(&LAPACK_spbtrf_base)>);
static_assert(std::is_same_v<decltype(&__wrap_spbtf2_),
                             decltype(&LAPACK_GLOBAL_SUFFIX(spbtf2, SPBTF2))>);
static_assert(
    std::is_same_v<decltype(&__wrap_spbtrs_), decltype(&LAPACK_spbtrs_base)>);
static_assert(
    std::is_same_v<decltype(&__wrap_dpbtrf_), decltype(&LAPACK_dpbtrf_base)>);
static_assert(std::is_same_v<decltype(&__wrap_dpbtf2_),
                             decltype(&LAPACK_GLOBAL_SUFFIX(dpbtf2, DPBTF2))>);
static_assert(
    std::is_same_v<decltype(&__wrap_dpbtrs_), decltype(&LAPACK_dpbtrs_base)>);
static_assert(
    std::is_same_v<decltype(&__wrap_cpbtrf_), decltype(&LAPACK_cpbtrf_base)>);
static_assert(std::is_same_v<decltype(&__wrap_cpbtf2_),
                             decltype(&LAPACK_GLOBAL_SUFFIX(cpbtf2, CPBTF2))>);
static_assert(
    std::is_same_v<decltype(&__wrap_cpbtrs_), decltype(&LAPACK_cpbtrs_base)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zpbtrf_), decltype(&LAPACK_zpbtrf_base)>);
static_assert(std::is_same_v<decltype(&__wrap_zpbtf2_),
                             decltype(&LAPACK_GLOBAL_SUFFIX(zpbtf2, ZPBTF2))>);
static_assert(
    std::is_same_v<decltype(&__wrap_zpbtrs_), decltype(&LAPACK_zpbtrs_base)>);

namespace asc_band_test {
void ResetFault(std::int64_t info, bool require_zero_imaginary,
                InfoWrite info_write) {
  g_info = static_cast<lapack_int>(info);
  g_calls = 0;
  g_valid = true;
  g_require_zero_imaginary = require_zero_imaginary;
  g_info_write = info_write;
}
std::size_t FaultCalls() { return g_calls; }
bool FaultArgumentsValid() { return g_valid; }
}  // namespace asc_band_test
