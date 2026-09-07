#include "sylvester_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

namespace {
using asc_sylvester_test::Fault;
Fault g_fault = Fault::kNone;
std::size_t g_calls = 0;
bool g_lengths_valid = true;

template <typename T, typename Real>
bool Inject(T* c, Real* scale, lapack_int* info, std::size_t length_a,
            std::size_t length_b) {
  ++g_calls;
  g_lengths_valid = g_lengths_valid && length_a == 1 && length_b == 1;
  if (g_fault == Fault::kNone) {
    return false;
  }
  c[0] = T{719};
  *scale = Real{0.5};
  *info = 0;
  switch (g_fault) {
    case Fault::kNegative:
      *info = -12;
      break;
    case Fault::kMinimumInteger:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case Fault::kImpossiblePositive:
      *info = std::numeric_limits<lapack_int>::max();
      break;
    case Fault::kNegativeScale:
      *scale = -1;
      break;
    case Fault::kLargeScale:
      *scale = 2;
      break;
    case Fault::kNanScale:
      *scale = std::numeric_limits<Real>::quiet_NaN();
      break;
    case Fault::kInfiniteScale:
      *scale = std::numeric_limits<Real>::infinity();
      break;
    case Fault::kZeroScale:
      *scale = 0;
      break;
    case Fault::kNonfiniteResult:
      c[0] = T{std::numeric_limits<Real>::infinity()};
      break;
    case Fault::kPerturbed:
      *info = 1;
      break;
    case Fault::kNone:
      break;
  }
  return true;
}
}  // namespace

namespace asc_sylvester_test {
void SetFault(Fault fault) { g_fault = fault; }
std::size_t ForeignCalls() { return g_calls; }
bool CharacterLengthsValid() { return g_lengths_valid; }
std::int64_t ProviderIntegerMinimum() {
  return std::numeric_limits<lapack_int>::min();
}
}  // namespace asc_sylvester_test

// Test-only GNU linker wrappers use the pinned LAPACK prototype types and the
// audited trailing character lengths; no wrapper is supported ASC API.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_strsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const float* a, const lapack_int* lda,
                    const float* b, const lapack_int* ldb, float* c,
                    const lapack_int* ldc, float* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b);
void __wrap_strsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const float* a, const lapack_int* lda,
                    const float* b, const lapack_int* ldb, float* c,
                    const lapack_int* ldc, float* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b) {
  if (!Inject(c, scale, info, length_a, length_b)) {
    __real_strsyl_(operation_a, operation_b, sign, m, n, a, lda, b, ldb, c, ldc,
                   scale, info, length_a, length_b);
  }
}
void __real_dtrsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const double* a, const lapack_int* lda,
                    const double* b, const lapack_int* ldb, double* c,
                    const lapack_int* ldc, double* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b);
void __wrap_dtrsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const double* a, const lapack_int* lda,
                    const double* b, const lapack_int* ldb, double* c,
                    const lapack_int* ldc, double* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b) {
  if (!Inject(c, scale, info, length_a, length_b)) {
    __real_dtrsyl_(operation_a, operation_b, sign, m, n, a, lda, b, ldb, c, ldc,
                   scale, info, length_a, length_b);
  }
}
void __real_ctrsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* b,
                    const lapack_int* ldb, std::complex<float>* c,
                    const lapack_int* ldc, float* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b);
void __wrap_ctrsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* b,
                    const lapack_int* ldb, std::complex<float>* c,
                    const lapack_int* ldc, float* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b) {
  if (!Inject(c, scale, info, length_a, length_b)) {
    __real_ctrsyl_(operation_a, operation_b, sign, m, n, a, lda, b, ldb, c, ldc,
                   scale, info, length_a, length_b);
  }
}
void __real_ztrsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* b,
                    const lapack_int* ldb, std::complex<double>* c,
                    const lapack_int* ldc, double* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b);
void __wrap_ztrsyl_(const char* operation_a, const char* operation_b,
                    const lapack_int* sign, const lapack_int* m,
                    const lapack_int* n, const std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* b,
                    const lapack_int* ldb, std::complex<double>* c,
                    const lapack_int* ldc, double* scale, lapack_int* info,
                    std::size_t length_a, std::size_t length_b) {
  if (!Inject(c, scale, info, length_a, length_b)) {
    __real_ztrsyl_(operation_a, operation_b, sign, m, n, a, lda, b, ldb, c, ldc,
                   scale, info, length_a, length_b);
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)

// Every wrapper declaration must match the actual pinned public C prototype,
// including constness, ABI-width INTEGER pointers and both character lengths.
// NOLINTBEGIN(bugprone-reserved-identifier)
static_assert(
    std::is_same_v<decltype(&__wrap_strsyl_), decltype(&LAPACK_strsyl_base)>);
static_assert(
    std::is_same_v<decltype(&__wrap_dtrsyl_), decltype(&LAPACK_dtrsyl_base)>);
static_assert(
    std::is_same_v<decltype(&__wrap_ctrsyl_), decltype(&LAPACK_ctrsyl_base)>);
static_assert(
    std::is_same_v<decltype(&__wrap_ztrsyl_), decltype(&LAPACK_ztrsyl_base)>);
// NOLINTEND(bugprone-reserved-identifier)
