#include "qr_faults.h"

#include <complex>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_qr_test::Routine g_routine = asc_qr_test::Routine::kGeqrf;
asc_qr_test::Fault g_fault = asc_qr_test::Fault::kNone;
std::size_t g_calls = 0;
std::size_t g_queries = 0;

template <typename T>
bool Inject(asc_qr_test::Routine routine, bool query, T* a, T* work,
            lapack_int* info) {
  ++g_calls;
  g_queries += query ? 1 : 0;
  if (routine != g_routine || g_fault == asc_qr_test::Fault::kNone) {
    return false;
  }
  *info = 0;
  switch (g_fault) {
    case asc_qr_test::Fault::kNegative:
      *info = -4;
      break;
    case asc_qr_test::Fault::kMinimumInteger:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case asc_qr_test::Fault::kPositive:
      *info = 1;
      break;
    case asc_qr_test::Fault::kQueryZero:
      *work = T{};
      break;
    case asc_qr_test::Fault::kQueryNan:
      *work = T{std::numeric_limits<float>::quiet_NaN()};
      break;
    case asc_qr_test::Fault::kQueryImaginary:
      if constexpr (!std::is_floating_point_v<T>) {
        *work = T{32, 1};
      }
      break;
    default:
      break;
  }
  if (!query) {
    a[0] = T{-881};
  }
  return true;
}
}  // namespace

namespace asc_qr_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
}
std::size_t ForeignCalls() { return g_calls; }
std::size_t QueryCalls() { return g_queries; }
}  // namespace asc_qr_test

// Test-only GNU wrapping uses the already executed GNU CHARACTER-length ABI
// probe. Production declarations remain authoritative LAPACK_* macros.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {

void __real_sgeqrf_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, float* tau, float* work,
                    const lapack_int* lwork, lapack_int* info);
void __wrap_sgeqrf_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, float* tau, float* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqrf, *lwork == -1, a, work, info)) {
    return;
  }
  __real_sgeqrf_(m, n, a, lda, tau, work, lwork, info);
}

void __real_sgeqr2_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, float* tau, float* work,
                    lapack_int* info);
void __wrap_sgeqr2_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, float* tau, float* work,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqr2, false, a, work, info)) {
    return;
  }
  __real_sgeqr2_(m, n, a, lda, tau, work, info);
}

void __real_sorgqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, float* a, const lapack_int* lda,
                    const float* tau, float* work, const lapack_int* lwork,
                    lapack_int* info);
void __wrap_sorgqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, float* a, const lapack_int* lda,
                    const float* tau, float* work, const lapack_int* lwork,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGenerate, *lwork == -1, a, work, info)) {
    return;
  }
  __real_sorgqr_(m, n, k, a, lda, tau, work, lwork, info);
}

void __real_sormqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k, const float* a,
                    const lapack_int* lda, const float* tau, float* matrix,
                    const lapack_int* ldc, float* work, const lapack_int* lwork,
                    lapack_int* info, std::size_t side_length,
                    std::size_t trans_length);
void __wrap_sormqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k, const float* a,
                    const lapack_int* lda, const float* tau, float* matrix,
                    const lapack_int* ldc, float* work, const lapack_int* lwork,
                    lapack_int* info, std::size_t side_length,
                    std::size_t trans_length) {
  if (Inject(asc_qr_test::Routine::kApply, *lwork == -1, matrix, work, info)) {
    return;
  }
  __real_sormqr_(side, trans, m, n, k, a, lda, tau, matrix, ldc, work, lwork,
                 info, side_length, trans_length);
}

void __real_dgeqrf_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, double* tau, double* work,
                    const lapack_int* lwork, lapack_int* info);
void __wrap_dgeqrf_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, double* tau, double* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqrf, *lwork == -1, a, work, info)) {
    return;
  }
  __real_dgeqrf_(m, n, a, lda, tau, work, lwork, info);
}

void __real_dgeqr2_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, double* tau, double* work,
                    lapack_int* info);
void __wrap_dgeqr2_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, double* tau, double* work,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqr2, false, a, work, info)) {
    return;
  }
  __real_dgeqr2_(m, n, a, lda, tau, work, info);
}

void __real_dorgqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, double* a, const lapack_int* lda,
                    const double* tau, double* work, const lapack_int* lwork,
                    lapack_int* info);
void __wrap_dorgqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, double* a, const lapack_int* lda,
                    const double* tau, double* work, const lapack_int* lwork,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGenerate, *lwork == -1, a, work, info)) {
    return;
  }
  __real_dorgqr_(m, n, k, a, lda, tau, work, lwork, info);
}

void __real_dormqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k, const double* a,
                    const lapack_int* lda, const double* tau, double* matrix,
                    const lapack_int* ldc, double* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t side_length, std::size_t trans_length);
void __wrap_dormqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k, const double* a,
                    const lapack_int* lda, const double* tau, double* matrix,
                    const lapack_int* ldc, double* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t side_length, std::size_t trans_length) {
  if (Inject(asc_qr_test::Routine::kApply, *lwork == -1, matrix, work, info)) {
    return;
  }
  __real_dormqr_(side, trans, m, n, k, a, lda, tau, matrix, ldc, work, lwork,
                 info, side_length, trans_length);
}

void __real_cgeqrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    std::complex<float>* tau, std::complex<float>* work,
                    const lapack_int* lwork, lapack_int* info);
void __wrap_cgeqrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    std::complex<float>* tau, std::complex<float>* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqrf, *lwork == -1, a, work, info)) {
    return;
  }
  __real_cgeqrf_(m, n, a, lda, tau, work, lwork, info);
}

void __real_cgeqr2_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    std::complex<float>* tau, std::complex<float>* work,
                    lapack_int* info);
void __wrap_cgeqr2_(const lapack_int* m, const lapack_int* n,
                    std::complex<float>* a, const lapack_int* lda,
                    std::complex<float>* tau, std::complex<float>* work,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqr2, false, a, work, info)) {
    return;
  }
  __real_cgeqr2_(m, n, a, lda, tau, work, info);
}

void __real_cungqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* tau,
                    std::complex<float>* work, const lapack_int* lwork,
                    lapack_int* info);
void __wrap_cungqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* tau,
                    std::complex<float>* work, const lapack_int* lwork,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGenerate, *lwork == -1, a, work, info)) {
    return;
  }
  __real_cungqr_(m, n, k, a, lda, tau, work, lwork, info);
}

void __real_cunmqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k,
                    const std::complex<float>* a, const lapack_int* lda,
                    const std::complex<float>* tau, std::complex<float>* matrix,
                    const lapack_int* ldc, std::complex<float>* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t side_length, std::size_t trans_length);
void __wrap_cunmqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k,
                    const std::complex<float>* a, const lapack_int* lda,
                    const std::complex<float>* tau, std::complex<float>* matrix,
                    const lapack_int* ldc, std::complex<float>* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t side_length, std::size_t trans_length) {
  if (Inject(asc_qr_test::Routine::kApply, *lwork == -1, matrix, work, info)) {
    return;
  }
  __real_cunmqr_(side, trans, m, n, k, a, lda, tau, matrix, ldc, work, lwork,
                 info, side_length, trans_length);
}

void __real_zgeqrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    std::complex<double>* tau, std::complex<double>* work,
                    const lapack_int* lwork, lapack_int* info);
void __wrap_zgeqrf_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    std::complex<double>* tau, std::complex<double>* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqrf, *lwork == -1, a, work, info)) {
    return;
  }
  __real_zgeqrf_(m, n, a, lda, tau, work, lwork, info);
}

void __real_zgeqr2_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    std::complex<double>* tau, std::complex<double>* work,
                    lapack_int* info);
void __wrap_zgeqr2_(const lapack_int* m, const lapack_int* n,
                    std::complex<double>* a, const lapack_int* lda,
                    std::complex<double>* tau, std::complex<double>* work,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGeqr2, false, a, work, info)) {
    return;
  }
  __real_zgeqr2_(m, n, a, lda, tau, work, info);
}

void __real_zungqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* tau,
                    std::complex<double>* work, const lapack_int* lwork,
                    lapack_int* info);
void __wrap_zungqr_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* k, std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* tau,
                    std::complex<double>* work, const lapack_int* lwork,
                    lapack_int* info) {
  if (Inject(asc_qr_test::Routine::kGenerate, *lwork == -1, a, work, info)) {
    return;
  }
  __real_zungqr_(m, n, k, a, lda, tau, work, lwork, info);
}

void __real_zunmqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k,
                    const std::complex<double>* a, const lapack_int* lda,
                    const std::complex<double>* tau,
                    std::complex<double>* matrix, const lapack_int* ldc,
                    std::complex<double>* work, const lapack_int* lwork,
                    lapack_int* info, std::size_t side_length,
                    std::size_t trans_length);
void __wrap_zunmqr_(const char* side, const char* trans, const lapack_int* m,
                    const lapack_int* n, const lapack_int* k,
                    const std::complex<double>* a, const lapack_int* lda,
                    const std::complex<double>* tau,
                    std::complex<double>* matrix, const lapack_int* ldc,
                    std::complex<double>* work, const lapack_int* lwork,
                    lapack_int* info, std::size_t side_length,
                    std::size_t trans_length) {
  if (Inject(asc_qr_test::Routine::kApply, *lwork == -1, matrix, work, info)) {
    return;
  }
  __real_zunmqr_(side, trans, m, n, k, a, lda, tau, matrix, ldc, work, lwork,
                 info, side_length, trans_length);
}
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
