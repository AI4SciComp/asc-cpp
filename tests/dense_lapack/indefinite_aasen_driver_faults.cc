#include "indefinite_aasen_driver_faults.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace {
using asc_aasen_driver_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_info = 0;
template <typename T, typename Operation>
void Invoke(lapack_int n, T* a, lapack_int* pivots, T* work, lapack_int* info,
            Operation operation) {
  assert(n == 1 || n == 3);
  ++g_calls;
  const T old_recommendation = work[0];
  const lapack_int old_pivot = pivots[0];
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  operation(&native_info);
  g_info = native_info;
  if (g_fault == Fault::kWrite32BitInfo) {
    const auto low = static_cast<std::int32_t>(native_info);
    std::memcpy(info, &low, sizeof(low));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -10;
  } else if (g_fault == Fault::kInfoBeyondOrder) {
    *info = n + 1;
  } else if (g_fault == Fault::kFalseSingular) {
    *info = 1;
  }
  if (g_fault == Fault::kZeroPivot) {
    pivots[0] = 0;
  } else if (g_fault == Fault::kBackwardPivot) {
    pivots[n - 1] = n - 1;
  } else if (g_fault == Fault::kWrongFirstPivot) {
    pivots[0] = 2;
  } else if (g_fault == Fault::kClobberUpperPivot) {
    if constexpr (sizeof(lapack_int) == 8) {
      pivots[0] |= static_cast<lapack_int>(std::uint64_t{1} << 32);
    }
  } else if (g_fault == Fault::kOmitPivot) {
    pivots[0] = old_pivot;
  } else if (g_fault == Fault::kWrite32BitPivot) {
    const auto low = static_cast<std::int32_t>(pivots[0]);
    pivots[0] = old_pivot;
    std::memcpy(pivots, &low, sizeof(low));
  }
  T& witness = n == 1 ? a[0] : work[2 * n - 2];
  if (g_fault == Fault::kConsistentSingular) {
    *info = n;
    witness = T{};
  } else if (g_fault == Fault::kSingularSentinelWitness) {
    witness = old_recommendation;
  } else if (g_fault == Fault::kSingularNanWitness) {
    witness = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  } else if (g_fault == Fault::kSingularNonzeroWitness) {
    witness = T{1};
  } else if (g_fault == Fault::kSingularRealOnlyWitness) {
    if constexpr (asc::DenseBlasComplex<T>) {
      witness.imag(-1);
    }
  } else if (g_fault == Fault::kSingularWrongInfo) {
    *info = 1;
  }
  if (g_fault == Fault::kOmitRecommendation) {
    work[0] = old_recommendation;
  } else if (g_fault == Fault::kNanRecommendation) {
    work[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  } else if (g_fault == Fault::kWrongRecommendation) {
    work[0] += T{1};
  } else if (g_fault == Fault::kNonzeroImagRecommendation ||
             g_fault == Fault::kRealOnlyRecommendation) {
    if constexpr (asc::DenseBlasComplex<T>) {
      work[0].imag(g_fault == Fault::kNonzeroImagRecommendation ? 1 : -1);
    }
  }
}
}  // namespace
namespace asc_aasen_driver_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_info; }
}  // namespace asc_aasen_driver_fault_test
// Only the named driver is interposed; its native dependency calls run
// unchanged. NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssysv_aa_base) __real_ssysv_aa_;
void __wrap_ssysv_aa_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, float* a, const lapack_int* lda,
                      lapack_int* pivots, float* b, const lapack_int* ldb,
                      float* work, const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, a, pivots, work, info, [&](lapack_int* native_info) {
    __real_ssysv_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssysv_aa_base), decltype(__wrap_ssysv_aa_)>);
decltype(LAPACK_dsysv_aa_base) __real_dsysv_aa_;
void __wrap_dsysv_aa_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, double* a, const lapack_int* lda,
                      lapack_int* pivots, double* b, const lapack_int* ldb,
                      double* work, const lapack_int* lwork, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, a, pivots, work, info, [&](lapack_int* native_info) {
    __real_dsysv_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsysv_aa_base), decltype(__wrap_dsysv_aa_)>);
decltype(LAPACK_csysv_aa_base) __real_csysv_aa_;
void __wrap_csysv_aa_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_float* a,
                      const lapack_int* lda, lapack_int* pivots,
                      lapack_complex_float* b, const lapack_int* ldb,
                      lapack_complex_float* work, const lapack_int* lwork,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, a, pivots, work, info, [&](lapack_int* native_info) {
    __real_csysv_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csysv_aa_base), decltype(__wrap_csysv_aa_)>);
decltype(LAPACK_zsysv_aa_base) __real_zsysv_aa_;
void __wrap_zsysv_aa_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_double* a,
                      const lapack_int* lda, lapack_int* pivots,
                      lapack_complex_double* b, const lapack_int* ldb,
                      lapack_complex_double* work, const lapack_int* lwork,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, a, pivots, work, info, [&](lapack_int* native_info) {
    __real_zsysv_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsysv_aa_base), decltype(__wrap_zsysv_aa_)>);
decltype(LAPACK_chesv_aa_base) __real_chesv_aa_;
void __wrap_chesv_aa_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_float* a,
                      const lapack_int* lda, lapack_int* pivots,
                      lapack_complex_float* b, const lapack_int* ldb,
                      lapack_complex_float* work, const lapack_int* lwork,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, a, pivots, work, info, [&](lapack_int* native_info) {
    __real_chesv_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chesv_aa_base), decltype(__wrap_chesv_aa_)>);
decltype(LAPACK_zhesv_aa_base) __real_zhesv_aa_;
void __wrap_zhesv_aa_(const char* uplo, const lapack_int* n,
                      const lapack_int* nrhs, lapack_complex_double* a,
                      const lapack_int* lda, lapack_int* pivots,
                      lapack_complex_double* b, const lapack_int* ldb,
                      lapack_complex_double* work, const lapack_int* lwork,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, a, pivots, work, info, [&](lapack_int* native_info) {
    __real_zhesv_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     native_info, length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhesv_aa_base), decltype(__wrap_zhesv_aa_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
