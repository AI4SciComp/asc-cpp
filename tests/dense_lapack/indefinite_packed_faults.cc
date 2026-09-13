#include "indefinite_packed_faults.h"

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
using asc_packed_indefinite_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_injected_info = 0;
void PivotFaults(lapack_int n, bool upper, lapack_int* pivots) {
  if (g_fault == Fault::kOmitLastPivot) {
    pivots[n - 1] = std::numeric_limits<lapack_int>::min();
  }
  if (g_fault == Fault::kZeroPivot) {
    pivots[0] = 0;
  }
  if (g_fault == Fault::kLargePositivePivot) {
    pivots[0] = n + 1;
  }
  if (g_fault == Fault::kLargeNegativePivot) {
    pivots[0] = -n - 1;
  }
  if (g_fault == Fault::kWrongDirection) {
    if (n == 1) {
      pivots[0] = 0;
    } else if (upper) {
      pivots[0] = n;
    } else {
      pivots[n - 1] = 1;
    }
  }
  if (g_fault == Fault::kUnpairedEdge) {
    for (lapack_int i = 0; i < n; ++i) {
      pivots[i] = i + 1;
    }
    pivots[n - 1] = -1;
  }
  if (g_fault == Fault::kUnequalPair) {
    pivots[0] = -1;
    if (n > 1) {
      pivots[1] = -2;
    }
  }
  if (g_fault == Fault::kWrongPairDirection) {
    if (n == 1) {
      pivots[0] = -1;
    } else if (upper) {
      pivots[0] = -n;
      pivots[1] = -n;
    } else {
      pivots[n - 2] = -1;
      pivots[n - 1] = -1;
    }
  }
}
template <typename T, typename Operation>
void Invoke(lapack_int n, bool upper, T* a, lapack_int* pivots,
            lapack_int* info, Operation operation) {
  assert(n > 0 && n <= 8);
  ++g_calls;
  std::array<lapack_int, 8> p{};
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  operation(p.data(), &native_info);
  g_native_info = native_info;
  if (g_fault == Fault::kWrite32BitInfo) {
    const auto partial = static_cast<std::int32_t>(native_info);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -3;
  }
  if (g_fault == Fault::kOutOfRangeInfo) {
    *info = n + 1;
  }
  if (g_fault == Fault::kNonzeroPartial || g_fault == Fault::kNanPartial) {
    *info = n;
    a[n * (n + 1) / 2 - 1] =
        g_fault == Fault::kNanPartial
            ? T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()}
            : T{1};
  }
  g_injected_info = *info;
  for (lapack_int i = 0; i < n; ++i) {
    if (g_fault == Fault::kWrite32BitPivots) {
      const auto partial = static_cast<std::int32_t>(p[i]);
      std::memcpy(pivots + i, &partial, sizeof(partial));
    } else if (g_fault != Fault::kOmitPivots) {
      pivots[i] = p[i];
    }
  }
  PivotFaults(n, upper, pivots);
  // These addresses are live guard objects in the checked caller workspace.
  // Independently corrupt each endpoint while retaining valid INFO and pivots.
  if (g_fault == Fault::kMatrixPrefix) {
    a[-1] = T{};
  }
  if (g_fault == Fault::kMatrixSuffix) {
    a[n * (n + 1) / 2] = T{};
  }
  if (g_fault == Fault::kPivotPrefix) {
    pivots[-1] = 0;
  }
  if (g_fault == Fault::kPivotSuffix) {
    pivots[n] = 0;
  }
}
}  // namespace
namespace asc_packed_indefinite_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_native_info = 0;
  g_injected_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
std::int64_t LastInjectedInfo() { return g_injected_info; }
}  // namespace asc_packed_indefinite_fault_test
// GNU ld --wrap requires these exact reserved external symbol names, matching
// the existing test-only interoperability convention.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssptrf_base) __real_ssptrf_;
void __wrap_ssptrf_(const char* tri, const lapack_int* n, float* a,
                    lapack_int* pivots, lapack_int* info, std::size_t length) {
  Invoke(*n, *tri == 'U', a, pivots, info,
         [&](lapack_int* p, lapack_int* native_info) {
           __real_ssptrf_(tri, n, a, p, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssptrf_base), decltype(__wrap_ssptrf_)>);
decltype(LAPACK_dsptrf_base) __real_dsptrf_;
void __wrap_dsptrf_(const char* tri, const lapack_int* n, double* a,
                    lapack_int* pivots, lapack_int* info, std::size_t length) {
  Invoke(*n, *tri == 'U', a, pivots, info,
         [&](lapack_int* p, lapack_int* native_info) {
           __real_dsptrf_(tri, n, a, p, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsptrf_base), decltype(__wrap_dsptrf_)>);
decltype(LAPACK_csptrf_base) __real_csptrf_;
void __wrap_csptrf_(const char* tri, const lapack_int* n,
                    lapack_complex_float* a, lapack_int* pivots,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, *tri == 'U', a, pivots, info,
         [&](lapack_int* p, lapack_int* native_info) {
           __real_csptrf_(tri, n, a, p, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csptrf_base), decltype(__wrap_csptrf_)>);
decltype(LAPACK_zsptrf_base) __real_zsptrf_;
void __wrap_zsptrf_(const char* tri, const lapack_int* n,
                    lapack_complex_double* a, lapack_int* pivots,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, *tri == 'U', a, pivots, info,
         [&](lapack_int* p, lapack_int* native_info) {
           __real_zsptrf_(tri, n, a, p, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsptrf_base), decltype(__wrap_zsptrf_)>);
decltype(LAPACK_chptrf_base) __real_chptrf_;
void __wrap_chptrf_(const char* tri, const lapack_int* n,
                    lapack_complex_float* a, lapack_int* pivots,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, *tri == 'U', a, pivots, info,
         [&](lapack_int* p, lapack_int* native_info) {
           __real_chptrf_(tri, n, a, p, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chptrf_base), decltype(__wrap_chptrf_)>);
decltype(LAPACK_zhptrf_base) __real_zhptrf_;
void __wrap_zhptrf_(const char* tri, const lapack_int* n,
                    lapack_complex_double* a, lapack_int* pivots,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, *tri == 'U', a, pivots, info,
         [&](lapack_int* p, lapack_int* native_info) {
           __real_zhptrf_(tri, n, a, p, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhptrf_base), decltype(__wrap_zhptrf_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
