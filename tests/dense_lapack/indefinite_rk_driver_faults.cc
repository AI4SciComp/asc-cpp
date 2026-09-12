#include "indefinite_rk_driver_faults.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace {
using asc_rk_driver_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_info = 0;
template <typename T, typename Operation>
void Invoke(lapack_int n, T* e, lapack_int* pivots, T* work, lapack_int* info,
            Operation operation) {
  assert(n > 0 && n <= 2);
  ++g_calls;
  std::array<lapack_int, 2> native_pivots{};
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  const T old_work = work[0];
  std::array<T, 2> old_e{};
  for (lapack_int i = 0; i < n; ++i) {
    old_e[i] = e[i];
  }
  operation(native_pivots.data(), &native_info);
  g_info = native_info;
  if (g_fault == Fault::kOmitE) {
    for (lapack_int i = 0; i < n; ++i) {
      e[i] = old_e[i];
    }
  } else if (g_fault == Fault::kBadE) {
    e[0] = T{1};
  }

  if (g_fault == Fault::kWrite32BitInfo) {
    const std::uint32_t zero = 0;
    std::memcpy(info, &zero, sizeof(zero));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -10;
  } else if (g_fault == Fault::kPositiveInfo) {
    *info = n + 1;
  } else if (g_fault == Fault::kInconsistentPositiveInfo) {
    *info = 1;
  }
  for (lapack_int i = 0; i < n; ++i) {
    if (g_fault == Fault::kWrite32BitPivots) {
      const auto partial = static_cast<std::int32_t>(native_pivots[i]);
      std::memcpy(pivots + i, &partial, sizeof(partial));
    } else if (g_fault != Fault::kOmitPivots) {
      pivots[i] = native_pivots[i];
    }
  }
  if (g_fault == Fault::kZeroPivot) {
    pivots[0] = 0;
  } else if (g_fault == Fault::kOutOfBoundsPivot) {
    pivots[0] = n + 1;
  } else if (g_fault == Fault::kUnpairedPivot) {
    pivots[0] = -1;
  }
  if (g_fault == Fault::kIncorrectWork) {
    work[0] += T{1};
  } else if (g_fault == Fault::kOmitWork) {
    work[0] = old_work;
  } else if (g_fault == Fault::kNanWork) {
    work[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  }
}
}  // namespace
namespace asc_rk_driver_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_info; }
}  // namespace asc_rk_driver_fault_test
// Each injected native-output defect follows execution of the actual routine.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssysv_rk_base) __real_ssysv_rk_;
void __wrap_ssysv_rk_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, float* a, const lapack_int* lda,
                      float* e, lapack_int* pivots, float* b,
                      const lapack_int* ldb, float* work,
                      const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, e, pivots, work, info,
         [&](lapack_int* native_pivots, lapack_int* native_info) {
           __real_ssysv_rk_(uplo, n, nrhs, a, lda, e, native_pivots, b, ldb,
                            work, lwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssysv_rk_base), decltype(__wrap_ssysv_rk_)>);
decltype(LAPACK_dsysv_rk_base) __real_dsysv_rk_;
void __wrap_dsysv_rk_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, double* a, const lapack_int* lda,
                      double* e, lapack_int* pivots, double* b,
                      const lapack_int* ldb, double* work,
                      const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, e, pivots, work, info,
         [&](lapack_int* native_pivots, lapack_int* native_info) {
           __real_dsysv_rk_(uplo, n, nrhs, a, lda, e, native_pivots, b, ldb,
                            work, lwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsysv_rk_base), decltype(__wrap_dsysv_rk_)>);
decltype(LAPACK_csysv_rk_base) __real_csysv_rk_;
void __wrap_csysv_rk_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_float* a,
                      const lapack_int* lda, lapack_complex_float* e,
                      lapack_int* pivots, lapack_complex_float* b,
                      const lapack_int* ldb, lapack_complex_float* work,
                      const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, e, pivots, work, info,
         [&](lapack_int* native_pivots, lapack_int* native_info) {
           __real_csysv_rk_(uplo, n, nrhs, a, lda, e, native_pivots, b, ldb,
                            work, lwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csysv_rk_base), decltype(__wrap_csysv_rk_)>);
decltype(LAPACK_zsysv_rk_base) __real_zsysv_rk_;
void __wrap_zsysv_rk_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_double* a,
                      const lapack_int* lda, lapack_complex_double* e,
                      lapack_int* pivots, lapack_complex_double* b,
                      const lapack_int* ldb, lapack_complex_double* work,
                      const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, e, pivots, work, info,
         [&](lapack_int* native_pivots, lapack_int* native_info) {
           __real_zsysv_rk_(uplo, n, nrhs, a, lda, e, native_pivots, b, ldb,
                            work, lwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsysv_rk_base), decltype(__wrap_zsysv_rk_)>);
decltype(LAPACK_chesv_rk_base) __real_chesv_rk_;
void __wrap_chesv_rk_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_float* a,
                      const lapack_int* lda, lapack_complex_float* e,
                      lapack_int* pivots, lapack_complex_float* b,
                      const lapack_int* ldb, lapack_complex_float* work,
                      const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, e, pivots, work, info,
         [&](lapack_int* native_pivots, lapack_int* native_info) {
           __real_chesv_rk_(uplo, n, nrhs, a, lda, e, native_pivots, b, ldb,
                            work, lwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chesv_rk_base), decltype(__wrap_chesv_rk_)>);
decltype(LAPACK_zhesv_rk_base) __real_zhesv_rk_;
void __wrap_zhesv_rk_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_double* a,
                      const lapack_int* lda, lapack_complex_double* e,
                      lapack_int* pivots, lapack_complex_double* b,
                      const lapack_int* ldb, lapack_complex_double* work,
                      const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, e, pivots, work, info,
         [&](lapack_int* native_pivots, lapack_int* native_info) {
           __real_zhesv_rk_(uplo, n, nrhs, a, lda, e, native_pivots, b, ldb,
                            work, lwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhesv_rk_base), decltype(__wrap_zhesv_rk_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
