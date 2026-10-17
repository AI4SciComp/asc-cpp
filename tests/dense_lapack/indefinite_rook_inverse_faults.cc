#include "indefinite_rook_inverse_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_inverse_prototypes.h"
namespace asc_rook_inverse_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
template <typename Operation>
void Invoke(lapack_int n, lapack_int* pivots, lapack_int* info,
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
    pivots[0] = 0;
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
}  // namespace asc_rook_inverse_fault_test
using asc_rook_inverse_fault_test::Invoke;
// GNU ld --wrap requires these exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(ssytri_rook_) __real_ssytri_rook_;
void __wrap_ssytri_rook_(char* uplo, lapack_int* n, float* a, lapack_int* lda,
                         lapack_int* pivots, float* work, lapack_int* info,
                         std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_ssytri_rook_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(ssytri_rook_), decltype(__wrap_ssytri_rook_)>);
decltype(dsytri_rook_) __real_dsytri_rook_;
void __wrap_dsytri_rook_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                         lapack_int* pivots, double* work, lapack_int* info,
                         std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_dsytri_rook_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(dsytri_rook_), decltype(__wrap_dsytri_rook_)>);
decltype(csytri_rook_) __real_csytri_rook_;
void __wrap_csytri_rook_(char* uplo, lapack_int* n, lapack_complex_float* a,
                         lapack_int* lda, lapack_int* pivots,
                         lapack_complex_float* work, lapack_int* info,
                         std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_csytri_rook_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(csytri_rook_), decltype(__wrap_csytri_rook_)>);
decltype(zsytri_rook_) __real_zsytri_rook_;
void __wrap_zsytri_rook_(char* uplo, lapack_int* n, lapack_complex_double* a,
                         lapack_int* lda, lapack_int* pivots,
                         lapack_complex_double* work, lapack_int* info,
                         std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zsytri_rook_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(zsytri_rook_), decltype(__wrap_zsytri_rook_)>);
decltype(chetri_rook_) __real_chetri_rook_;
void __wrap_chetri_rook_(char* uplo, lapack_int* n, lapack_complex_float* a,
                         lapack_int* lda, lapack_int* pivots,
                         lapack_complex_float* work, lapack_int* info,
                         std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_chetri_rook_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(chetri_rook_), decltype(__wrap_chetri_rook_)>);
decltype(zhetri_rook_) __real_zhetri_rook_;
void __wrap_zhetri_rook_(char* uplo, lapack_int* n, lapack_complex_double* a,
                         lapack_int* lda, lapack_int* pivots,
                         lapack_complex_double* work, lapack_int* info,
                         std::size_t length) {
  Invoke(*n, pivots, info, [&](lapack_int* native_info) {
    __real_zhetri_rook_(uplo, n, a, lda, pivots, work, native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(zhetri_rook_), decltype(__wrap_zhetri_rook_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
