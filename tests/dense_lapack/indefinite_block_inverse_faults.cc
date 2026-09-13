#include "indefinite_block_inverse_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_block_inverse_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
bool g_active = false;
template <typename Operation>
void Invoke(lapack_int n, const lapack_int* pivots, lapack_int* info,
            Operation operation) {
  if (g_active) {
    operation(info);
    return;
  }
  ++g_calls;
  g_full_width = *info == std::numeric_limits<lapack_int>::min();
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  g_active = true;
  operation(&native_info);
  g_active = false;
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
}  // namespace asc_block_inverse_fault_test
using asc_block_inverse_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytri2_base) __real_ssytri2_;
void __wrap_ssytri2_(const char* uplo, const lapack_int* n, float* a,
                     const lapack_int* lda, const lapack_int* pivots,
                     float* work, const lapack_int* size, lapack_int* info,
                     std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_ssytri2_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssytri2_base), decltype(__wrap_ssytri2_)>);
decltype(LAPACK_dsytri2_base) __real_dsytri2_;
void __wrap_dsytri2_(const char* uplo, const lapack_int* n, double* a,
                     const lapack_int* lda, const lapack_int* pivots,
                     double* work, const lapack_int* size, lapack_int* info,
                     std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_dsytri2_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsytri2_base), decltype(__wrap_dsytri2_)>);
decltype(LAPACK_csytri2_base) __real_csytri2_;
void __wrap_csytri2_(const char* uplo, const lapack_int* n,
                     lapack_complex_float* a, const lapack_int* lda,
                     const lapack_int* pivots, lapack_complex_float* work,
                     const lapack_int* size, lapack_int* info,
                     std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_csytri2_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csytri2_base), decltype(__wrap_csytri2_)>);
decltype(LAPACK_zsytri2_base) __real_zsytri2_;
void __wrap_zsytri2_(const char* uplo, const lapack_int* n,
                     lapack_complex_double* a, const lapack_int* lda,
                     const lapack_int* pivots, lapack_complex_double* work,
                     const lapack_int* size, lapack_int* info,
                     std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zsytri2_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsytri2_base), decltype(__wrap_zsytri2_)>);
decltype(LAPACK_chetri2_base) __real_chetri2_;
void __wrap_chetri2_(const char* uplo, const lapack_int* n,
                     lapack_complex_float* a, const lapack_int* lda,
                     const lapack_int* pivots, lapack_complex_float* work,
                     const lapack_int* size, lapack_int* info,
                     std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_chetri2_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chetri2_base), decltype(__wrap_chetri2_)>);
decltype(LAPACK_zhetri2_base) __real_zhetri2_;
void __wrap_zhetri2_(const char* uplo, const lapack_int* n,
                     lapack_complex_double* a, const lapack_int* lda,
                     const lapack_int* pivots, lapack_complex_double* work,
                     const lapack_int* size, lapack_int* info,
                     std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zhetri2_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhetri2_base), decltype(__wrap_zhetri2_)>);
decltype(LAPACK_ssytri2x_base) __real_ssytri2x_;
void __wrap_ssytri2x_(const char* uplo, const lapack_int* n, float* a,
                      const lapack_int* lda, const lapack_int* pivots,
                      float* work, const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_ssytri2x_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssytri2x_base), decltype(__wrap_ssytri2x_)>);
decltype(LAPACK_dsytri2x_base) __real_dsytri2x_;
void __wrap_dsytri2x_(const char* uplo, const lapack_int* n, double* a,
                      const lapack_int* lda, const lapack_int* pivots,
                      double* work, const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_dsytri2x_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsytri2x_base), decltype(__wrap_dsytri2x_)>);
decltype(LAPACK_csytri2x_base) __real_csytri2x_;
void __wrap_csytri2x_(const char* uplo, const lapack_int* n,
                      lapack_complex_float* a, const lapack_int* lda,
                      const lapack_int* pivots, lapack_complex_float* work,
                      const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_csytri2x_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csytri2x_base), decltype(__wrap_csytri2x_)>);
decltype(LAPACK_zsytri2x_base) __real_zsytri2x_;
void __wrap_zsytri2x_(const char* uplo, const lapack_int* n,
                      lapack_complex_double* a, const lapack_int* lda,
                      const lapack_int* pivots, lapack_complex_double* work,
                      const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zsytri2x_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsytri2x_base), decltype(__wrap_zsytri2x_)>);
decltype(LAPACK_chetri2x_base) __real_chetri2x_;
void __wrap_chetri2x_(const char* uplo, const lapack_int* n,
                      lapack_complex_float* a, const lapack_int* lda,
                      const lapack_int* pivots, lapack_complex_float* work,
                      const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_chetri2x_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chetri2x_base), decltype(__wrap_chetri2x_)>);
decltype(LAPACK_zhetri2x_base) __real_zhetri2x_;
void __wrap_zhetri2x_(const char* uplo, const lapack_int* n,
                      lapack_complex_double* a, const lapack_int* lda,
                      const lapack_int* pivots, lapack_complex_double* work,
                      const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zhetri2x_(uplo, n, a, lda, pivots, work, size, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhetri2x_base), decltype(__wrap_zhetri2x_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
