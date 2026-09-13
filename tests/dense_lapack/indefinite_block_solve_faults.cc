#include "indefinite_block_solve_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace asc_block_solve_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
template <typename T, typename Operation>
void Invoke(const T* factors, const lapack_int* pivots, T* work,
            lapack_int* info, Operation operation) {
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
  // Only mutable private copies reach these observers. Simulate
  // input/restoration defects after the actual provider call; caller
  // factor/pivot storage is const.
  if (g_fault == Fault::kChangedPivot) {
    const_cast<lapack_int*>(pivots)[0] = 0;
  }
  if (g_fault == Fault::kChangedFactor) {
    const_cast<T*>(factors)[0] += T{1};
  }
  // WORK is scratch, not a native result channel. Its final value is
  // unrestricted.
  if (g_fault == Fault::kChangedWork) {
    work[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
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
}  // namespace asc_block_solve_fault_test
using asc_block_solve_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytrs2_base) __real_ssytrs2_;
void __wrap_ssytrs2_(const char* uplo, const lapack_int* n,
                     const lapack_int* nrhs, const float* a,
                     const lapack_int* lda, const lapack_int* pivots, float* b,
                     const lapack_int* ldb, float* work, lapack_int* info,
                     std::size_t length) {
  Invoke(a, pivots, work, info, [&](lapack_int* native_info) {
    __real_ssytrs2_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, native_info,
                    length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssytrs2_base), decltype(__wrap_ssytrs2_)>);
decltype(LAPACK_dsytrs2_base) __real_dsytrs2_;
void __wrap_dsytrs2_(const char* uplo, const lapack_int* n,
                     const lapack_int* nrhs, const double* a,
                     const lapack_int* lda, const lapack_int* pivots, double* b,
                     const lapack_int* ldb, double* work, lapack_int* info,
                     std::size_t length) {
  Invoke(a, pivots, work, info, [&](lapack_int* native_info) {
    __real_dsytrs2_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, native_info,
                    length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsytrs2_base), decltype(__wrap_dsytrs2_)>);
decltype(LAPACK_csytrs2_base) __real_csytrs2_;
void __wrap_csytrs2_(const char* uplo, const lapack_int* n,
                     const lapack_int* nrhs, const lapack_complex_float* a,
                     const lapack_int* lda, const lapack_int* pivots,
                     lapack_complex_float* b, const lapack_int* ldb,
                     lapack_complex_float* work, lapack_int* info,
                     std::size_t length) {
  Invoke(a, pivots, work, info, [&](lapack_int* native_info) {
    __real_csytrs2_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, native_info,
                    length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csytrs2_base), decltype(__wrap_csytrs2_)>);
decltype(LAPACK_zsytrs2_base) __real_zsytrs2_;
void __wrap_zsytrs2_(const char* uplo, const lapack_int* n,
                     const lapack_int* nrhs, const lapack_complex_double* a,
                     const lapack_int* lda, const lapack_int* pivots,
                     lapack_complex_double* b, const lapack_int* ldb,
                     lapack_complex_double* work, lapack_int* info,
                     std::size_t length) {
  Invoke(a, pivots, work, info, [&](lapack_int* native_info) {
    __real_zsytrs2_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, native_info,
                    length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsytrs2_base), decltype(__wrap_zsytrs2_)>);
decltype(LAPACK_chetrs2_base) __real_chetrs2_;
void __wrap_chetrs2_(const char* uplo, const lapack_int* n,
                     const lapack_int* nrhs, const lapack_complex_float* a,
                     const lapack_int* lda, const lapack_int* pivots,
                     lapack_complex_float* b, const lapack_int* ldb,
                     lapack_complex_float* work, lapack_int* info,
                     std::size_t length) {
  Invoke(a, pivots, work, info, [&](lapack_int* native_info) {
    __real_chetrs2_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, native_info,
                    length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chetrs2_base), decltype(__wrap_chetrs2_)>);
decltype(LAPACK_zhetrs2_base) __real_zhetrs2_;
void __wrap_zhetrs2_(const char* uplo, const lapack_int* n,
                     const lapack_int* nrhs, const lapack_complex_double* a,
                     const lapack_int* lda, const lapack_int* pivots,
                     lapack_complex_double* b, const lapack_int* ldb,
                     lapack_complex_double* work, lapack_int* info,
                     std::size_t length) {
  Invoke(a, pivots, work, info, [&](lapack_int* native_info) {
    __real_zhetrs2_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, native_info,
                    length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhetrs2_base), decltype(__wrap_zhetrs2_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
