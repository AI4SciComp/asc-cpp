#include "band_unwritten_info_support.h"

#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_band_abi.h"

namespace {
std::string_view g_target;
bool g_omit = false;
std::size_t g_calls = 0;
bool g_success = true;

void Publish(std::string_view routine, lapack_int native_info,
             lapack_int* destination) {
  if (routine == g_target) {
    ++g_calls;
    g_success = g_success && native_info == 0;
    if (g_omit) {
      return;
    }
  }
  *destination = native_info;
}
}  // namespace

// Exact pinned ABI endpoints invoke the real provider with a private INFO.
// Only the selected entry's INFO publication is withheld; nested native calls
// retain their real INFO. This is injected output-omission evidence.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_spbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              float* ab, const lapack_int* ldab, float* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length);
extern "C" void __wrap_spbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              float* ab, const lapack_int* ldab, float* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_spbsv_(uplo, n, kd, nrhs, ab, ldab, b, ldb, &native_info, length);
  Publish("spbsv", native_info, info);
}

extern "C" void __real_spbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const float* ab,
                               const lapack_int* ldab, float* s, float* scond,
                               float* amax, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_spbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const float* ab,
                               const lapack_int* ldab, float* s, float* scond,
                               float* amax, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_spbequ_(uplo, n, kd, ab, ldab, s, scond, amax, &native_info, length);
  Publish("spbequ", native_info, info);
}

extern "C" void __real_dpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              double* ab, const lapack_int* ldab, double* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length);
extern "C" void __wrap_dpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              double* ab, const lapack_int* ldab, double* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_dpbsv_(uplo, n, kd, nrhs, ab, ldab, b, ldb, &native_info, length);
  Publish("dpbsv", native_info, info);
}

extern "C" void __real_dpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const double* ab,
                               const lapack_int* ldab, double* s, double* scond,
                               double* amax, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_dpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const double* ab,
                               const lapack_int* ldab, double* s, double* scond,
                               double* amax, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_dpbequ_(uplo, n, kd, ab, ldab, s, scond, amax, &native_info, length);
  Publish("dpbequ", native_info, info);
}

extern "C" void __real_cpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              std::complex<float>* ab, const lapack_int* ldab,
                              std::complex<float>* b, const lapack_int* ldb,
                              lapack_int* info, std::size_t length);
extern "C" void __wrap_cpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              std::complex<float>* ab, const lapack_int* ldab,
                              std::complex<float>* b, const lapack_int* ldb,
                              lapack_int* info, std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_cpbsv_(uplo, n, kd, nrhs, ab, ldab, b, ldb, &native_info, length);
  Publish("cpbsv", native_info, info);
}

extern "C" void __real_cpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, float* s, float* scond,
                               float* amax, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_cpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, float* s, float* scond,
                               float* amax, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_cpbequ_(uplo, n, kd, ab, ldab, s, scond, amax, &native_info, length);
  Publish("cpbequ", native_info, info);
}

extern "C" void __real_zpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              std::complex<double>* ab, const lapack_int* ldab,
                              std::complex<double>* b, const lapack_int* ldb,
                              lapack_int* info, std::size_t length);
extern "C" void __wrap_zpbsv_(const char* uplo, const lapack_int* n,
                              const lapack_int* kd, const lapack_int* nrhs,
                              std::complex<double>* ab, const lapack_int* ldab,
                              std::complex<double>* b, const lapack_int* ldb,
                              lapack_int* info, std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_zpbsv_(uplo, n, kd, nrhs, ab, ldab, b, ldb, &native_info, length);
  Publish("zpbsv", native_info, info);
}

extern "C" void __real_zpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, double* s, double* scond,
                               double* amax, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_zpbequ_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, double* s, double* scond,
                               double* amax, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_zpbequ_(uplo, n, kd, ab, ldab, s, scond, amax, &native_info, length);
  Publish("zpbequ", native_info, info);
}

extern "C" void __real_spbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const float* ab,
                               const lapack_int* ldab, const float* anorm,
                               float* rcond, float* work, lapack_int* auxiliary,
                               lapack_int* info, std::size_t length);
extern "C" void __wrap_spbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const float* ab,
                               const lapack_int* ldab, const float* anorm,
                               float* rcond, float* work, lapack_int* auxiliary,
                               lapack_int* info, std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_spbcon_(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary,
                 &native_info, length);
  Publish("spbcon", native_info, info);
}

extern "C" void __real_dpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const double* ab,
                               const lapack_int* ldab, const double* anorm,
                               double* rcond, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_dpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const double* ab,
                               const lapack_int* ldab, const double* anorm,
                               double* rcond, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_dpbcon_(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary,
                 &native_info, length);
  Publish("dpbcon", native_info, info);
}

extern "C" void __real_cpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, const float* anorm,
                               float* rcond, std::complex<float>* work,
                               float* auxiliary, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_cpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<float>* ab,
                               const lapack_int* ldab, const float* anorm,
                               float* rcond, std::complex<float>* work,
                               float* auxiliary, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_cpbcon_(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary,
                 &native_info, length);
  Publish("cpbcon", native_info, info);
}

extern "C" void __real_zpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, const double* anorm,
                               double* rcond, std::complex<double>* work,
                               double* auxiliary, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_zpbcon_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd,
                               const std::complex<double>* ab,
                               const lapack_int* ldab, const double* anorm,
                               double* rcond, std::complex<double>* work,
                               double* auxiliary, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_zpbcon_(uplo, n, kd, ab, ldab, anorm, rcond, work, auxiliary,
                 &native_info, length);
  Publish("zpbcon", native_info, info);
}

extern "C" void __real_spbrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const float* a, const lapack_int* lda,
                               const float* af, const lapack_int* ldaf,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               float* work, lapack_int* auxiliary,
                               lapack_int* info, std::size_t length);
extern "C" void __wrap_spbrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const float* a, const lapack_int* lda,
                               const float* af, const lapack_int* ldaf,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               float* work, lapack_int* auxiliary,
                               lapack_int* info, std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_spbrfs_(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr,
                 berr, work, auxiliary, &native_info, length);
  Publish("spbrfs", native_info, info);
}

extern "C" void __real_dpbrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const double* a, const lapack_int* lda,
                               const double* af, const lapack_int* ldaf,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* ferr,
                               double* berr, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t length);
extern "C" void __wrap_dpbrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* kd, const lapack_int* nrhs,
                               const double* a, const lapack_int* lda,
                               const double* af, const lapack_int* ldaf,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* ferr,
                               double* berr, double* work,
                               lapack_int* auxiliary, lapack_int* info,
                               std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_dpbrfs_(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr,
                 berr, work, auxiliary, &native_info, length);
  Publish("dpbrfs", native_info, info);
}

extern "C" void __real_cpbrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* kd,
    const lapack_int* nrhs, const std::complex<float>* a, const lapack_int* lda,
    const std::complex<float>* af, const lapack_int* ldaf,
    const std::complex<float>* b, const lapack_int* ldb, std::complex<float>* x,
    const lapack_int* ldx, float* ferr, float* berr, std::complex<float>* work,
    float* auxiliary, lapack_int* info, std::size_t length);
extern "C" void __wrap_cpbrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* kd,
    const lapack_int* nrhs, const std::complex<float>* a, const lapack_int* lda,
    const std::complex<float>* af, const lapack_int* ldaf,
    const std::complex<float>* b, const lapack_int* ldb, std::complex<float>* x,
    const lapack_int* ldx, float* ferr, float* berr, std::complex<float>* work,
    float* auxiliary, lapack_int* info, std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_cpbrfs_(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr,
                 berr, work, auxiliary, &native_info, length);
  Publish("cpbrfs", native_info, info);
}

extern "C" void __real_zpbrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* kd,
    const lapack_int* nrhs, const std::complex<double>* a,
    const lapack_int* lda, const std::complex<double>* af,
    const lapack_int* ldaf, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* ferr, double* berr, std::complex<double>* work, double* auxiliary,
    lapack_int* info, std::size_t length);
extern "C" void __wrap_zpbrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* kd,
    const lapack_int* nrhs, const std::complex<double>* a,
    const lapack_int* lda, const std::complex<double>* af,
    const lapack_int* ldaf, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* ferr, double* berr, std::complex<double>* work, double* auxiliary,
    lapack_int* info, std::size_t length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_zpbrfs_(uplo, n, kd, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr,
                 berr, work, auxiliary, &native_info, length);
  Publish("zpbrfs", native_info, info);
}

extern "C" void __real_spbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, float* ab,
    const lapack_int* ldab, float* afb, const lapack_int* ldafb, char* equed,
    float* scales, float* b, const lapack_int* ldb, float* x,
    const lapack_int* ldx, float* rcond, float* ferr, float* berr, float* work,
    lapack_int* auxiliary, lapack_int* info, std::size_t fact_length,
    std::size_t uplo_length, std::size_t equed_length);
extern "C" void __wrap_spbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, float* ab,
    const lapack_int* ldab, float* afb, const lapack_int* ldafb, char* equed,
    float* scales, float* b, const lapack_int* ldb, float* x,
    const lapack_int* ldx, float* rcond, float* ferr, float* berr, float* work,
    lapack_int* auxiliary, lapack_int* info, std::size_t fact_length,
    std::size_t uplo_length, std::size_t equed_length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_spbsvx_(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales,
                 b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                 &native_info, fact_length, uplo_length, equed_length);
  Publish("spbsvx", native_info, info);
}

extern "C" void __real_dpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, double* ab,
    const lapack_int* ldab, double* afb, const lapack_int* ldafb, char* equed,
    double* scales, double* b, const lapack_int* ldb, double* x,
    const lapack_int* ldx, double* rcond, double* ferr, double* berr,
    double* work, lapack_int* auxiliary, lapack_int* info,
    std::size_t fact_length, std::size_t uplo_length, std::size_t equed_length);
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
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_dpbsvx_(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales,
                 b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                 &native_info, fact_length, uplo_length, equed_length);
  Publish("dpbsvx", native_info, info);
}

extern "C" void __real_cpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, std::complex<float>* ab,
    const lapack_int* ldab, std::complex<float>* afb, const lapack_int* ldafb,
    char* equed, float* scales, std::complex<float>* b, const lapack_int* ldb,
    std::complex<float>* x, const lapack_int* ldx, float* rcond, float* ferr,
    float* berr, std::complex<float>* work, float* auxiliary, lapack_int* info,
    std::size_t fact_length, std::size_t uplo_length, std::size_t equed_length);
extern "C" void __wrap_cpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, std::complex<float>* ab,
    const lapack_int* ldab, std::complex<float>* afb, const lapack_int* ldafb,
    char* equed, float* scales, std::complex<float>* b, const lapack_int* ldb,
    std::complex<float>* x, const lapack_int* ldx, float* rcond, float* ferr,
    float* berr, std::complex<float>* work, float* auxiliary, lapack_int* info,
    std::size_t fact_length, std::size_t uplo_length,
    std::size_t equed_length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_cpbsvx_(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales,
                 b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                 &native_info, fact_length, uplo_length, equed_length);
  Publish("cpbsvx", native_info, info);
}

extern "C" void __real_zpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, std::complex<double>* ab,
    const lapack_int* ldab, std::complex<double>* afb, const lapack_int* ldafb,
    char* equed, double* scales, std::complex<double>* b, const lapack_int* ldb,
    std::complex<double>* x, const lapack_int* ldx, double* rcond, double* ferr,
    double* berr, std::complex<double>* work, double* auxiliary,
    lapack_int* info, std::size_t fact_length, std::size_t uplo_length,
    std::size_t equed_length);
extern "C" void __wrap_zpbsvx_(
    const char* fact, const char* uplo, const lapack_int* n,
    const lapack_int* kd, const lapack_int* nrhs, std::complex<double>* ab,
    const lapack_int* ldab, std::complex<double>* afb, const lapack_int* ldafb,
    char* equed, double* scales, std::complex<double>* b, const lapack_int* ldb,
    std::complex<double>* x, const lapack_int* ldx, double* rcond, double* ferr,
    double* berr, std::complex<double>* work, double* auxiliary,
    lapack_int* info, std::size_t fact_length, std::size_t uplo_length,
    std::size_t equed_length) {
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  __real_zpbsvx_(fact, uplo, n, kd, nrhs, ab, ldab, afb, ldafb, equed, scales,
                 b, ldb, x, ldx, rcond, ferr, berr, work, auxiliary,
                 &native_info, fact_length, uplo_length, equed_length);
  Publish("zpbsvx", native_info, info);
}
// NOLINTEND(bugprone-reserved-identifier)
namespace asc_band_unwritten_test {
void Reset(std::string_view routine, bool omit) {
  g_target = routine;
  g_omit = omit;
  g_calls = 0;
  g_success = true;
}
std::size_t Calls() { return g_calls; }
bool NativeSucceeded() { return g_success; }
}  // namespace asc_band_unwritten_test
