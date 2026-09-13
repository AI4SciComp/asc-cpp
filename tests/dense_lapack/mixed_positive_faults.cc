#include "mixed_positive_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"

namespace asc_mixed_positive_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local void (*g_entry)() = nullptr;
void Partial(lapack_int* value) {
  // Deliberately leave the upper bytes of the initialized sentinel intact.
  // LP64 uses an unwritten-output control, since no wider INTEGER exists.
  if constexpr (sizeof(lapack_int) > sizeof(std::int32_t)) {
    const std::int32_t one = 1;
    std::memcpy(value, &one, sizeof(one));
  }
}
template <typename T>
bool Intercept(lapack_int n, lapack_int nrhs, T* a, T* x, lapack_int* iter,
               lapack_int* info) {
  ++g_calls;
  if (g_entry != nullptr) {
    g_entry();
  }
  if (g_mode == Mode::kPass) {
    return false;
  }
  if (g_mode != Mode::kUnwrittenIter && g_mode != Mode::kPartialIter) {
    *iter = 0;
  }
  if (g_mode != Mode::kUnwrittenInfo && g_mode != Mode::kPartialInfo) {
    *info = 0;
  }
  if (g_mode != Mode::kUnwrittenX) {
    for (lapack_int i = 0; i < n * nrhs; ++i) {
      x[i] = T{1};
    }
  }
  switch (g_mode) {
    case Mode::kNegativeInfo:
    case Mode::kWriteThenNegative:
      *info = -1;
      if (g_mode == Mode::kWriteThenNegative && n > 0) {
        a[0] = T{-31};
      }
      break;
    case Mode::kExcessInfo:
      *info = n + 1;
      break;
    case Mode::kPartialInfo:
      Partial(info);
      break;
    case Mode::kInvalidIter:
      *iter = -4;
      break;
    case Mode::kPartialIter:
      Partial(iter);
      break;
    case Mode::kImplementationFallback:
      *iter = -1;
      break;
    default:
      break;
  }
  return true;
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_entry = nullptr;
}
std::size_t Calls() { return g_calls; }
void OnEntry(void (*callback)()) { g_entry = callback; }
// GNU linker spellings are localized to this test-only boundary interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_dsposv_base) __real_dsposv_;
void __wrap_dsposv_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    const double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* work, float* lower,
                    lapack_int* iter, lapack_int* info, std::size_t length) {
  if (!Intercept(*n, *nrhs, a, x, iter, info)) {
    __real_dsposv_(uplo, n, nrhs, a, lda, b, ldb, x, ldx, work, lower, iter,
                   info, length);
  }
}
decltype(LAPACK_zcposv_base) __real_zcposv_;
void __wrap_zcposv_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* b,
                    const lapack_int* ldb, std::complex<double>* x,
                    const lapack_int* ldx, std::complex<double>* work,
                    std::complex<float>* lower, double* real, lapack_int* iter,
                    lapack_int* info, std::size_t length) {
  if (!Intercept(*n, *nrhs, a, x, iter, info)) {
    __real_zcposv_(uplo, n, nrhs, a, lda, b, ldb, x, ldx, work, lower, real,
                   iter, info, length);
  }
}
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsposv_base), decltype(__wrap_dsposv_)>);
static_assert(
    std::is_same_v<decltype(LAPACK_zcposv_base), decltype(__wrap_zcposv_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_mixed_positive_fault
