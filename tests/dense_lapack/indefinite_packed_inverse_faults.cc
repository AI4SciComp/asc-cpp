#include "indefinite_packed_inverse_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_packed_inverse_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
template <typename Operation>
void Invoke(lapack_int n, const lapack_int* pivots, lapack_int* info,
            Operation operation) {
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
  if (g_fault == Fault::kMaximumInfo) {
    *info = std::numeric_limits<lapack_int>::max();
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -4;
  }
  if (g_fault == Fault::kOutOfRangeInfo) {
    *info = n + 1;
  }
  if (g_fault == Fault::kContradictoryInfo) {
    *info = native_info == 0 ? 1 : 0;
  }
  if (g_fault == Fault::kWrongIndex) {
    *info = native_info == 1 ? n + 1 : 1;
  }
  if (g_fault == Fault::kChangedPivot) {
    // Only the backend's mutable private converted copy reaches this fault
    // observer. Simulate a provider violating its input-only IPIV contract.
    const_cast<lapack_int*>(pivots)[0] = 0;
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
bool SeedWasFullWidth() { return g_full_width; }
}  // namespace asc_packed_inverse_fault_test
using asc_packed_inverse_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssptri_base) __real_ssptri_;
void __wrap_ssptri_(const char* uplo, const lapack_int* n, float* a,
                    const lapack_int* pivots, float* work, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_ssptri_(uplo, n, a, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssptri_base), decltype(__wrap_ssptri_)>);
decltype(LAPACK_dsptri_base) __real_dsptri_;
void __wrap_dsptri_(const char* uplo, const lapack_int* n, double* a,
                    const lapack_int* pivots, double* work, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_dsptri_(uplo, n, a, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsptri_base), decltype(__wrap_dsptri_)>);
decltype(LAPACK_csptri_base) __real_csptri_;
void __wrap_csptri_(const char* uplo, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* pivots,
                    lapack_complex_float* work, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_csptri_(uplo, n, a, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csptri_base), decltype(__wrap_csptri_)>);
decltype(LAPACK_zsptri_base) __real_zsptri_;
void __wrap_zsptri_(const char* uplo, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* pivots,
                    lapack_complex_double* work, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zsptri_(uplo, n, a, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsptri_base), decltype(__wrap_zsptri_)>);
decltype(LAPACK_chptri_base) __real_chptri_;
void __wrap_chptri_(const char* uplo, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* pivots,
                    lapack_complex_float* work, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_chptri_(uplo, n, a, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chptri_base), decltype(__wrap_chptri_)>);
decltype(LAPACK_zhptri_base) __real_zhptri_;
void __wrap_zhptri_(const char* uplo, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* pivots,
                    lapack_complex_double* work, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zhptri_(uplo, n, a, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhptri_base), decltype(__wrap_zhptri_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
