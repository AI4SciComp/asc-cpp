#include "indefinite_packed_condition_faults.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_packed_condition_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
long double g_native_condition = 0;
long double g_published_condition = 0;
bool g_full_width = false;
bool g_nan_seed = false;

template <typename Real>
void AlterCondition(Real native, Real* condition) {
  if (g_fault != Fault::kOmitCondition) {
    *condition = native;
  }
  switch (g_fault) {
    case Fault::kNegativeCondition:
      *condition = Real{-2};
      break;
    case Fault::kNanCondition:
      *condition = std::numeric_limits<Real>::quiet_NaN();
      break;
    case Fault::kInfCondition:
      *condition = std::numeric_limits<Real>::infinity();
      break;
    case Fault::kNegativeInfCondition:
      *condition = -std::numeric_limits<Real>::infinity();
      break;
    case Fault::kZeroCondition:
      *condition = Real{0};
      break;
    case Fault::kTwoCondition:
      *condition = Real{2};
      break;
    case Fault::kSubnormalCondition:
      *condition = std::numeric_limits<Real>::denorm_min();
      break;
    case Fault::kNegativeZeroCondition:
      *condition = -Real{0};
      break;
    default:
      break;
  }
}

template <typename Real, typename Operation>
void Invoke(const lapack_int* pivots, lapack_int* info, Real* condition,
            Operation operation) {
  ++g_calls;
  g_full_width = *info == std::numeric_limits<lapack_int>::min();
  g_nan_seed = std::isnan(*condition);
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  Real native_condition = std::numeric_limits<Real>::quiet_NaN();
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
    *info = -5;
  }
  if (g_fault == Fault::kPositiveInfo) {
    *info = 7;
  }
  if (g_fault == Fault::kMaximumInfo) {
    *info = std::numeric_limits<lapack_int>::max();
  }
  if (g_fault == Fault::kChangedPivot) {
    // Only the mutable converted native copy reaches this observer.
    const_cast<lapack_int*>(pivots)[0] = 0;
  }
  AlterCondition(native_condition, condition);
  g_published_info = *info;
  g_published_condition = *condition;
}
}  // namespace
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_full_width = false;
  g_nan_seed = false;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
std::int64_t LastPublishedInfo() { return g_published_info; }
long double LastNativeCondition() { return g_native_condition; }
long double LastPublishedCondition() { return g_published_condition; }
bool SeedWasFullWidth() { return g_full_width; }
bool ConditionSeedWasNan() { return g_nan_seed; }
}  // namespace asc_packed_condition_fault_test
using asc_packed_condition_fault_test::Invoke;
// GNU ld --wrap requires these exact external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_sspcon_base) __real_sspcon_;
void __wrap_sspcon_(const char* uplo, const lapack_int* n, const float* a,
                    const lapack_int* pivots, const float* norm,
                    float* condition, float* work, lapack_int* iwork,
                    lapack_int* info, std::size_t length) {
  Invoke(pivots, info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_sspcon_(uplo, n, a, pivots, norm, native_condition, work,
                          iwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_sspcon_base), decltype(__wrap_sspcon_)>);
decltype(LAPACK_dspcon_base) __real_dspcon_;
void __wrap_dspcon_(const char* uplo, const lapack_int* n, const double* a,
                    const lapack_int* pivots, const double* norm,
                    double* condition, double* work, lapack_int* iwork,
                    lapack_int* info, std::size_t length) {
  Invoke(pivots, info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_dspcon_(uplo, n, a, pivots, norm, native_condition, work,
                          iwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dspcon_base), decltype(__wrap_dspcon_)>);
decltype(LAPACK_cspcon_base) __real_cspcon_;
void __wrap_cspcon_(const char* uplo, const lapack_int* n,
                    const lapack_complex_float* a, const lapack_int* pivots,
                    const float* norm, float* condition,
                    lapack_complex_float* work, lapack_int* info,
                    std::size_t length) {
  Invoke(pivots, info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_cspcon_(uplo, n, a, pivots, norm, native_condition, work,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_cspcon_base), decltype(__wrap_cspcon_)>);
decltype(LAPACK_zspcon_base) __real_zspcon_;
void __wrap_zspcon_(const char* uplo, const lapack_int* n,
                    const lapack_complex_double* a, const lapack_int* pivots,
                    const double* norm, double* condition,
                    lapack_complex_double* work, lapack_int* info,
                    std::size_t length) {
  Invoke(pivots, info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_zspcon_(uplo, n, a, pivots, norm, native_condition, work,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zspcon_base), decltype(__wrap_zspcon_)>);
decltype(LAPACK_chpcon_base) __real_chpcon_;
void __wrap_chpcon_(const char* uplo, const lapack_int* n,
                    const lapack_complex_float* a, const lapack_int* pivots,
                    const float* norm, float* condition,
                    lapack_complex_float* work, lapack_int* info,
                    std::size_t length) {
  Invoke(pivots, info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_chpcon_(uplo, n, a, pivots, norm, native_condition, work,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chpcon_base), decltype(__wrap_chpcon_)>);
decltype(LAPACK_zhpcon_base) __real_zhpcon_;
void __wrap_zhpcon_(const char* uplo, const lapack_int* n,
                    const lapack_complex_double* a, const lapack_int* pivots,
                    const double* norm, double* condition,
                    lapack_complex_double* work, lapack_int* info,
                    std::size_t length) {
  Invoke(pivots, info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_zhpcon_(uplo, n, a, pivots, norm, native_condition, work,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhpcon_base), decltype(__wrap_zhpcon_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
