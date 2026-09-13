#include "mixed_general_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"

namespace asc_mixed_fault {
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
bool Intercept(lapack_int n, lapack_int nrhs, T* a, lapack_int* pivots, T* x,
               lapack_int* iter, lapack_int* info) {
  ++g_calls;
  if (g_entry != nullptr) {
    g_entry();
  }
  if (g_mode == Mode::kPass) {
    return false;
  }
  for (lapack_int i = 0; i < n; ++i) {
    if (g_mode != Mode::kUnwrittenPivot && g_mode != Mode::kPartialPivot) {
      pivots[i] = i + 1;
    }
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
    case Mode::kInvalidPivot:
      pivots[0] = n + 1;
      break;
    case Mode::kPartialPivot:
      Partial(pivots);
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
decltype(LAPACK_dsgesv) __real_dsgesv_;
void __wrap_dsgesv_(const lapack_int* n, const lapack_int* nrhs, double* a,
                    const lapack_int* lda, lapack_int* pivots, const double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* work, float* lower, lapack_int* iter,
                    lapack_int* info) {
  if (!Intercept(*n, *nrhs, a, pivots, x, iter, info)) {
    __real_dsgesv_(n, nrhs, a, lda, pivots, b, ldb, x, ldx, work, lower, iter,
                   info);
  }
}
decltype(LAPACK_zcgesv) __real_zcgesv_;
void __wrap_zcgesv_(const lapack_int* n, const lapack_int* nrhs,
                    std::complex<double>* a, const lapack_int* lda,
                    lapack_int* pivots, const std::complex<double>* b,
                    const lapack_int* ldb, std::complex<double>* x,
                    const lapack_int* ldx, std::complex<double>* work,
                    std::complex<float>* lower, double* real, lapack_int* iter,
                    lapack_int* info) {
  if (!Intercept(*n, *nrhs, a, pivots, x, iter, info)) {
    __real_zcgesv_(n, nrhs, a, lda, pivots, b, ldb, x, ldx, work, lower, real,
                   iter, info);
  }
}
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsgesv), decltype(__wrap_dsgesv_)>);
static_assert(
    std::is_same_v<decltype(LAPACK_zcgesv), decltype(__wrap_zcgesv_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_mixed_fault
