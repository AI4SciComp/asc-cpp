#include "indefinite_rk_condition_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_rk_condition_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
long double g_native_condition = -1;
bool g_full_width = false;
template <typename Real, typename Operation>
void Invoke(const lapack_int* n, const lapack_int* pivots, lapack_int* info,
            Real* condition, Operation operation) {
  ++g_calls;
  g_full_width =
      *info == std::numeric_limits<lapack_int>::min() && *condition == Real{-1};
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  Real native_condition = -1;
  operation(&native_info, &native_condition);
  g_native_info = native_info;
  g_native_condition = native_condition;
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
  if (g_fault == Fault::kPositiveInfo) {
    *info = 1;
  }
  if (g_fault == Fault::kOutOfRangeInfo) {
    *info = *n + 1;
  }
  // Only the mutable private INTEGER copy is changed by this observer.
  if (g_fault == Fault::kChangedPivot) {
    const_cast<lapack_int*>(pivots)[0] = 0;
  }
  if (g_fault == Fault::kWrite16BitCondition) {
    std::memcpy(condition, &native_condition, sizeof(std::uint16_t));
  } else if (g_fault == Fault::kWrite32BitCondition) {
    std::memcpy(condition, &native_condition, sizeof(std::uint32_t));
  } else if (g_fault != Fault::kOmitCondition) {
    *condition = native_condition;
  }
  if (g_fault == Fault::kNegativeCondition) {
    *condition = -2;
  }
  if (g_fault == Fault::kNanCondition) {
    *condition = std::numeric_limits<Real>::quiet_NaN();
  }
  if (g_fault == Fault::kInfCondition) {
    *condition = std::numeric_limits<Real>::infinity();
  }
  if (g_fault == Fault::kZeroCondition) {
    *condition = 0;
  }
  if (g_fault == Fault::kAboveOneCondition) {
    *condition = 2;
  }
  g_published_info = *info;
}
}  // namespace
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_full_width = false;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
std::int64_t LastPublishedInfo() { return g_published_info; }
long double LastNativeCondition() { return g_native_condition; }
bool SeedWasFullWidth() { return g_full_width; }
}  // namespace asc_rk_condition_fault_test
using asc_rk_condition_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssycon_3_base) __real_ssycon_3_;
void __wrap_ssycon_3_(const char* uplo, const lapack_int* n, const float* a,
                      const lapack_int* lda, const float* extra,
                      const lapack_int* pivots, const float* norm,
                      float* condition, float* work, lapack_int* iwork,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_ssycon_3_(uplo, n, a, lda, extra, pivots, norm,
                            native_condition, work, iwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssycon_3_base), decltype(__wrap_ssycon_3_)>);
decltype(LAPACK_dsycon_3_base) __real_dsycon_3_;
void __wrap_dsycon_3_(const char* uplo, const lapack_int* n, const double* a,
                      const lapack_int* lda, const double* extra,
                      const lapack_int* pivots, const double* norm,
                      double* condition, double* work, lapack_int* iwork,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_dsycon_3_(uplo, n, a, lda, extra, pivots, norm,
                            native_condition, work, iwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsycon_3_base), decltype(__wrap_dsycon_3_)>);
decltype(LAPACK_csycon_3_base) __real_csycon_3_;
void __wrap_csycon_3_(const char* uplo, const lapack_int* n,
                      const lapack_complex_float* a, const lapack_int* lda,
                      const lapack_complex_float* extra,
                      const lapack_int* pivots, const float* norm,
                      float* condition, lapack_complex_float* work,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_csycon_3_(uplo, n, a, lda, extra, pivots, norm,
                            native_condition, work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csycon_3_base), decltype(__wrap_csycon_3_)>);
decltype(LAPACK_zsycon_3_base) __real_zsycon_3_;
void __wrap_zsycon_3_(const char* uplo, const lapack_int* n,
                      const lapack_complex_double* a, const lapack_int* lda,
                      const lapack_complex_double* extra,
                      const lapack_int* pivots, const double* norm,
                      double* condition, lapack_complex_double* work,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_zsycon_3_(uplo, n, a, lda, extra, pivots, norm,
                            native_condition, work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsycon_3_base), decltype(__wrap_zsycon_3_)>);
decltype(LAPACK_checon_3_base) __real_checon_3_;
void __wrap_checon_3_(const char* uplo, const lapack_int* n,
                      const lapack_complex_float* a, const lapack_int* lda,
                      const lapack_complex_float* extra,
                      const lapack_int* pivots, const float* norm,
                      float* condition, lapack_complex_float* work,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_checon_3_(uplo, n, a, lda, extra, pivots, norm,
                            native_condition, work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_checon_3_base), decltype(__wrap_checon_3_)>);
decltype(LAPACK_zhecon_3_base) __real_zhecon_3_;
void __wrap_zhecon_3_(const char* uplo, const lapack_int* n,
                      const lapack_complex_double* a, const lapack_int* lda,
                      const lapack_complex_double* extra,
                      const lapack_int* pivots, const double* norm,
                      double* condition, lapack_complex_double* work,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_zhecon_3_(uplo, n, a, lda, extra, pivots, norm,
                            native_condition, work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhecon_3_base), decltype(__wrap_zhecon_3_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
