#include "indefinite_rk_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rk_prototypes.h"
#include "asc/dense/blas.h"
namespace asc_rk_fault_test {
namespace {
Routine g_routine = Routine::kTf2;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
template <typename T, typename Operation>
void Invoke(Routine routine, char uplo, lapack_int n, T* a, lapack_int lda,
            T* e, lapack_int* pivots, T* work, lapack_int* info,
            Operation operation) {
  if (routine != g_routine) {
    operation(info);
    return;
  }
  ++g_calls;
  g_full_width = *info == std::numeric_limits<lapack_int>::min();
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  operation(&native_info);
  g_native_info = native_info;
  if (g_fault == Fault::kWrite16BitInfo) {
    const auto partial = static_cast<std::int16_t>(native_info);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault == Fault::kWrite32BitInfo) {
    const auto partial = static_cast<std::int32_t>(native_info);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -2;
  }
  if (g_fault == Fault::kLargeInfo) {
    *info = n + 1;
  }
  if (g_fault == Fault::kZeroPivot) {
    pivots[0] = 0;
  }
  if (g_fault == Fault::kBrokenPair) {
    // A lone negative at the traversal boundary cannot form a complete pair.
    pivots[uplo == 'U' ? n - 1 : 0] = -1;
    if (n > 1) {
      pivots[uplo == 'U' ? n - 2 : 1] = uplo == 'U' ? n - 1 : 2;
    }
  }
  if (g_fault == Fault::kWrongDirection) {
    // Scalar order has no distinct in-range direction; use the zero target.
    if (n == 1) {
      pivots[0] = 0;
    } else if (uplo == 'U') {
      pivots[0] = n;
    } else {
      pivots[n - 1] = 1;
    }
  }
  if (g_fault == Fault::kChangedE) {
    e[uplo == 'U' ? 0 : n - 1] = T{7};
  }
  if (g_fault == Fault::kChangedFactorSlot) {
    for (lapack_int i = 0; i + 1 < n; ++i) {
      if (pivots[i] < 0) {
        a[uplo == 'U' ? (i + 1) * lda + i : i * lda + i + 1] = T{9};
        break;
      }
    }
  }
  if (g_fault == Fault::kChangedWork && routine == Routine::kTrf) {
    work[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  }
  g_published_info = *info;
}
}  // namespace
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
  g_calls = 0;
  g_full_width = false;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
std::int64_t LastPublishedInfo() { return g_published_info; }
bool SeedWasFullWidth() { return g_full_width; }
}  // namespace asc_rk_fault_test
using asc_rk_fault_test::Invoke;
using asc_rk_fault_test::Routine;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(ssytf2_rk_) __real_ssytf2_rk_;
void __wrap_ssytf2_rk_(char* uplo, lapack_int* n, float* a, lapack_int* lda,
                       float* e, lapack_int* pivots, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTf2, *uplo, *n, a, *lda, e, pivots,
         static_cast<float*>(nullptr), info, [&](lapack_int* native_info) {
           __real_ssytf2_rk_(uplo, n, a, lda, e, pivots, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(ssytf2_rk_), decltype(__wrap_ssytf2_rk_)>);
decltype(LAPACK_ssytrf_rk_base) __real_ssytrf_rk_;
void __wrap_ssytrf_rk_(const char* uplo, const lapack_int* n, float* a,
                       const lapack_int* lda, float* e, lapack_int* pivots,
                       float* work, const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTrf, *uplo, *n, a, *lda, e, pivots, work, info,
         [&](lapack_int* native_info) {
           __real_ssytrf_rk_(uplo, n, a, lda, e, pivots, work, lwork,
                             native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_ssytrf_rk_base),
                             decltype(__wrap_ssytrf_rk_)>);
decltype(dsytf2_rk_) __real_dsytf2_rk_;
void __wrap_dsytf2_rk_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                       double* e, lapack_int* pivots, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTf2, *uplo, *n, a, *lda, e, pivots,
         static_cast<double*>(nullptr), info, [&](lapack_int* native_info) {
           __real_dsytf2_rk_(uplo, n, a, lda, e, pivots, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(dsytf2_rk_), decltype(__wrap_dsytf2_rk_)>);
decltype(LAPACK_dsytrf_rk_base) __real_dsytrf_rk_;
void __wrap_dsytrf_rk_(const char* uplo, const lapack_int* n, double* a,
                       const lapack_int* lda, double* e, lapack_int* pivots,
                       double* work, const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTrf, *uplo, *n, a, *lda, e, pivots, work, info,
         [&](lapack_int* native_info) {
           __real_dsytrf_rk_(uplo, n, a, lda, e, pivots, work, lwork,
                             native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_dsytrf_rk_base),
                             decltype(__wrap_dsytrf_rk_)>);
decltype(csytf2_rk_) __real_csytf2_rk_;
void __wrap_csytf2_rk_(char* uplo, lapack_int* n, lapack_complex_float* a,
                       lapack_int* lda, lapack_complex_float* e,
                       lapack_int* pivots, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTf2, *uplo, *n, a, *lda, e, pivots,
         static_cast<lapack_complex_float*>(nullptr), info,
         [&](lapack_int* native_info) {
           __real_csytf2_rk_(uplo, n, a, lda, e, pivots, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(csytf2_rk_), decltype(__wrap_csytf2_rk_)>);
decltype(LAPACK_csytrf_rk_base) __real_csytrf_rk_;
void __wrap_csytrf_rk_(const char* uplo, const lapack_int* n,
                       lapack_complex_float* a, const lapack_int* lda,
                       lapack_complex_float* e, lapack_int* pivots,
                       lapack_complex_float* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrf, *uplo, *n, a, *lda, e, pivots, work, info,
         [&](lapack_int* native_info) {
           __real_csytrf_rk_(uplo, n, a, lda, e, pivots, work, lwork,
                             native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_csytrf_rk_base),
                             decltype(__wrap_csytrf_rk_)>);
decltype(zsytf2_rk_) __real_zsytf2_rk_;
void __wrap_zsytf2_rk_(char* uplo, lapack_int* n, lapack_complex_double* a,
                       lapack_int* lda, lapack_complex_double* e,
                       lapack_int* pivots, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTf2, *uplo, *n, a, *lda, e, pivots,
         static_cast<lapack_complex_double*>(nullptr), info,
         [&](lapack_int* native_info) {
           __real_zsytf2_rk_(uplo, n, a, lda, e, pivots, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(zsytf2_rk_), decltype(__wrap_zsytf2_rk_)>);
decltype(LAPACK_zsytrf_rk_base) __real_zsytrf_rk_;
void __wrap_zsytrf_rk_(const char* uplo, const lapack_int* n,
                       lapack_complex_double* a, const lapack_int* lda,
                       lapack_complex_double* e, lapack_int* pivots,
                       lapack_complex_double* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrf, *uplo, *n, a, *lda, e, pivots, work, info,
         [&](lapack_int* native_info) {
           __real_zsytrf_rk_(uplo, n, a, lda, e, pivots, work, lwork,
                             native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_zsytrf_rk_base),
                             decltype(__wrap_zsytrf_rk_)>);
decltype(chetf2_rk_) __real_chetf2_rk_;
void __wrap_chetf2_rk_(char* uplo, lapack_int* n, lapack_complex_float* a,
                       lapack_int* lda, lapack_complex_float* e,
                       lapack_int* pivots, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTf2, *uplo, *n, a, *lda, e, pivots,
         static_cast<lapack_complex_float*>(nullptr), info,
         [&](lapack_int* native_info) {
           __real_chetf2_rk_(uplo, n, a, lda, e, pivots, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(chetf2_rk_), decltype(__wrap_chetf2_rk_)>);
decltype(LAPACK_chetrf_rk_base) __real_chetrf_rk_;
void __wrap_chetrf_rk_(const char* uplo, const lapack_int* n,
                       lapack_complex_float* a, const lapack_int* lda,
                       lapack_complex_float* e, lapack_int* pivots,
                       lapack_complex_float* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrf, *uplo, *n, a, *lda, e, pivots, work, info,
         [&](lapack_int* native_info) {
           __real_chetrf_rk_(uplo, n, a, lda, e, pivots, work, lwork,
                             native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_chetrf_rk_base),
                             decltype(__wrap_chetrf_rk_)>);
decltype(zhetf2_rk_) __real_zhetf2_rk_;
void __wrap_zhetf2_rk_(char* uplo, lapack_int* n, lapack_complex_double* a,
                       lapack_int* lda, lapack_complex_double* e,
                       lapack_int* pivots, lapack_int* info,
                       std::size_t length) {
  Invoke(Routine::kTf2, *uplo, *n, a, *lda, e, pivots,
         static_cast<lapack_complex_double*>(nullptr), info,
         [&](lapack_int* native_info) {
           __real_zhetf2_rk_(uplo, n, a, lda, e, pivots, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(zhetf2_rk_), decltype(__wrap_zhetf2_rk_)>);
decltype(LAPACK_zhetrf_rk_base) __real_zhetrf_rk_;
void __wrap_zhetrf_rk_(const char* uplo, const lapack_int* n,
                       lapack_complex_double* a, const lapack_int* lda,
                       lapack_complex_double* e, lapack_int* pivots,
                       lapack_complex_double* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrf, *uplo, *n, a, *lda, e, pivots, work, info,
         [&](lapack_int* native_info) {
           __real_zhetrf_rk_(uplo, n, a, lda, e, pivots, work, lwork,
                             native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_zhetrf_rk_base),
                             decltype(__wrap_zhetrf_rk_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
