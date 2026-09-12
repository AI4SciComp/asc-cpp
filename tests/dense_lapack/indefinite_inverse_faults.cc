#include "indefinite_inverse_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_inverse_fault_test {
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
  if (g_fault == Fault::kWrite32BitInfo) {
    const auto partial = static_cast<std::int32_t>(native_info);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
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
}  // namespace asc_inverse_fault_test
using asc_inverse_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytri_base) __real_ssytri_;
void __wrap_ssytri_(const char* uplo, const lapack_int* n, float* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    float* work, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_ssytri_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssytri_base), decltype(__wrap_ssytri_)>);
decltype(LAPACK_dsytri_base) __real_dsytri_;
void __wrap_dsytri_(const char* uplo, const lapack_int* n, double* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    double* work, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_dsytri_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsytri_base), decltype(__wrap_dsytri_)>);
decltype(LAPACK_csytri_base) __real_csytri_;
void __wrap_csytri_(const char* uplo, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* lda,
                    const lapack_int* pivots, lapack_complex_float* work,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_csytri_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csytri_base), decltype(__wrap_csytri_)>);
decltype(LAPACK_zsytri_base) __real_zsytri_;
void __wrap_zsytri_(const char* uplo, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* lda,
                    const lapack_int* pivots, lapack_complex_double* work,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zsytri_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsytri_base), decltype(__wrap_zsytri_)>);
decltype(LAPACK_chetri_base) __real_chetri_;
void __wrap_chetri_(const char* uplo, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* lda,
                    const lapack_int* pivots, lapack_complex_float* work,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_chetri_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chetri_base), decltype(__wrap_chetri_)>);
decltype(LAPACK_zhetri_base) __real_zhetri_;
void __wrap_zhetri_(const char* uplo, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* lda,
                    const lapack_int* pivots, lapack_complex_double* work,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zhetri_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhetri_base), decltype(__wrap_zhetri_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
