#include "indefinite_rk_solve_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_rk_solve_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
template <typename Operation>
void Invoke(const lapack_int* n, const lapack_int* pivots, lapack_int* info,
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
  if (g_fault == Fault::kNegativeInfo) {
    *info = -2;
  }
  if (g_fault == Fault::kPositiveInfo) {
    *info = 1;
  }
  if (g_fault == Fault::kOutOfRangeInfo) {
    *info = *n + 1;
  }
  // Only the mutable private INTEGER copy reaches this observer. Public
  // factors and E remain const native inputs; no factor conversion exists.
  if (g_fault == Fault::kChangedPivot) {
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
}  // namespace asc_rk_solve_fault_test
using asc_rk_solve_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytrs_3_base) __real_ssytrs_3_;
void __wrap_ssytrs_3_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, const float* a,
                      const lapack_int* lda, const float* extra,
                      const lapack_int* pivots, float* b, const lapack_int* ldb,
                      lapack_int* info, std::size_t length) {
  Invoke(n, pivots, info, [&](lapack_int* native_info) {
    __real_ssytrs_3_(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssytrs_3_base), decltype(__wrap_ssytrs_3_)>);
decltype(LAPACK_dsytrs_3_base) __real_dsytrs_3_;
void __wrap_dsytrs_3_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, const double* a,
                      const lapack_int* lda, const double* extra,
                      const lapack_int* pivots, double* b,
                      const lapack_int* ldb, lapack_int* info,
                      std::size_t length) {
  Invoke(n, pivots, info, [&](lapack_int* native_info) {
    __real_dsytrs_3_(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsytrs_3_base), decltype(__wrap_dsytrs_3_)>);
decltype(LAPACK_csytrs_3_base) __real_csytrs_3_;
void __wrap_csytrs_3_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, const lapack_complex_float* a,
                      const lapack_int* lda, const lapack_complex_float* extra,
                      const lapack_int* pivots, lapack_complex_float* b,
                      const lapack_int* ldb, lapack_int* info,
                      std::size_t length) {
  Invoke(n, pivots, info, [&](lapack_int* native_info) {
    __real_csytrs_3_(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csytrs_3_base), decltype(__wrap_csytrs_3_)>);
decltype(LAPACK_zsytrs_3_base) __real_zsytrs_3_;
void __wrap_zsytrs_3_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, const lapack_complex_double* a,
                      const lapack_int* lda, const lapack_complex_double* extra,
                      const lapack_int* pivots, lapack_complex_double* b,
                      const lapack_int* ldb, lapack_int* info,
                      std::size_t length) {
  Invoke(n, pivots, info, [&](lapack_int* native_info) {
    __real_zsytrs_3_(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsytrs_3_base), decltype(__wrap_zsytrs_3_)>);
decltype(LAPACK_chetrs_3_base) __real_chetrs_3_;
void __wrap_chetrs_3_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, const lapack_complex_float* a,
                      const lapack_int* lda, const lapack_complex_float* extra,
                      const lapack_int* pivots, lapack_complex_float* b,
                      const lapack_int* ldb, lapack_int* info,
                      std::size_t length) {
  Invoke(n, pivots, info, [&](lapack_int* native_info) {
    __real_chetrs_3_(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chetrs_3_base), decltype(__wrap_chetrs_3_)>);
decltype(LAPACK_zhetrs_3_base) __real_zhetrs_3_;
void __wrap_zhetrs_3_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, const lapack_complex_double* a,
                      const lapack_int* lda, const lapack_complex_double* extra,
                      const lapack_int* pivots, lapack_complex_double* b,
                      const lapack_int* ldb, lapack_int* info,
                      std::size_t length) {
  Invoke(n, pivots, info, [&](lapack_int* native_info) {
    __real_zhetrs_3_(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhetrs_3_base), decltype(__wrap_zhetrs_3_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
