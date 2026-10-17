#include "indefinite_rook_condition_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_condition_prototypes.h"
namespace {
using asc_rook_condition_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = std::numeric_limits<std::int64_t>::min();
long double g_native_condition = -1;
template <typename Real, typename Operation>
void Invoke(lapack_int* info, Real* condition, Operation operation) {
  ++g_calls;
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  Real native_condition = -1;
  operation(&native_info, &native_condition);
  g_native_info = native_info;
  g_native_condition = native_condition;
  if (g_fault == Fault::kWrite32BitZero) {
    const std::uint32_t zero = 0;
    std::memcpy(info, &zero, sizeof(zero));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault != Fault::kOmitCondition) {
    *condition = native_condition;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -6;
  }
  if (g_fault == Fault::kPositiveInfo) {
    *info = 7;
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
}
}  // namespace
namespace asc_rook_condition_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_native_info = std::numeric_limits<std::int64_t>::min();
  g_native_condition = -1;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
long double LastNativeCondition() { return g_native_condition; }
}  // namespace asc_rook_condition_fault_test
// Every fault follows the real routine. Numerical kernels remain unchanged.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(ssycon_rook_) __real_ssycon_rook_;
void __wrap_ssycon_rook_(char* uplo, lapack_int* n, float* a, lapack_int* lda,
                         lapack_int* pivots, float* norm, float* condition,
                         float* work, lapack_int* iwork, lapack_int* info,
                         std::size_t length) {
  Invoke(info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_ssycon_rook_(uplo, n, a, lda, pivots, norm, native_condition,
                               work, iwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(ssycon_rook_), decltype(__wrap_ssycon_rook_)>);
decltype(dsycon_rook_) __real_dsycon_rook_;
void __wrap_dsycon_rook_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                         lapack_int* pivots, double* norm, double* condition,
                         double* work, lapack_int* iwork, lapack_int* info,
                         std::size_t length) {
  Invoke(info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_dsycon_rook_(uplo, n, a, lda, pivots, norm, native_condition,
                               work, iwork, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(dsycon_rook_), decltype(__wrap_dsycon_rook_)>);
decltype(csycon_rook_) __real_csycon_rook_;
void __wrap_csycon_rook_(char* uplo, lapack_int* n, lapack_complex_float* a,
                         lapack_int* lda, lapack_int* pivots, float* norm,
                         float* condition, lapack_complex_float* work,
                         lapack_int* info, std::size_t length) {
  Invoke(info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_csycon_rook_(uplo, n, a, lda, pivots, norm, native_condition,
                               work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(csycon_rook_), decltype(__wrap_csycon_rook_)>);
decltype(zsycon_rook_) __real_zsycon_rook_;
void __wrap_zsycon_rook_(char* uplo, lapack_int* n, lapack_complex_double* a,
                         lapack_int* lda, lapack_int* pivots, double* norm,
                         double* condition, lapack_complex_double* work,
                         lapack_int* info, std::size_t length) {
  Invoke(info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_zsycon_rook_(uplo, n, a, lda, pivots, norm, native_condition,
                               work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(zsycon_rook_), decltype(__wrap_zsycon_rook_)>);
decltype(checon_rook_) __real_checon_rook_;
void __wrap_checon_rook_(char* uplo, lapack_int* n, lapack_complex_float* a,
                         lapack_int* lda, lapack_int* pivots, float* norm,
                         float* condition, lapack_complex_float* work,
                         lapack_int* info, std::size_t length) {
  Invoke(info, condition,
         [&](lapack_int* native_info, float* native_condition) {
           __real_checon_rook_(uplo, n, a, lda, pivots, norm, native_condition,
                               work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(checon_rook_), decltype(__wrap_checon_rook_)>);
decltype(zhecon_rook_) __real_zhecon_rook_;
void __wrap_zhecon_rook_(char* uplo, lapack_int* n, lapack_complex_double* a,
                         lapack_int* lda, lapack_int* pivots, double* norm,
                         double* condition, lapack_complex_double* work,
                         lapack_int* info, std::size_t length) {
  Invoke(info, condition,
         [&](lapack_int* native_info, double* native_condition) {
           __real_zhecon_rook_(uplo, n, a, lda, pivots, norm, native_condition,
                               work, native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(zhecon_rook_), decltype(__wrap_zhecon_rook_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
