#include "indefinite_aasen_solve_faults.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace {
using asc_aasen_solve_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_info = 0;
template <typename T, typename Operation>
void Invoke(lapack_int n, const lapack_int* input_pivots, T* work,
            lapack_int* info, Operation operation) {
  assert(n == 1 || n == 3);
  ++g_calls;
  const T old_last_diagonal = work[2 * n - 2];
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
  // The wrapper's private input objects are mutable; the native declaration
  // promises only that it reads them. Deliberate corruption tests that promise.
  auto* pivots = const_cast<lapack_int*>(input_pivots);
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
  }
  if (g_fault == Fault::kConsistentSingular) {
    *info = n;
    work[2 * n - 2] = T{};
  } else if (g_fault == Fault::kSingularOmitWork) {
    work[2 * n - 2] = old_last_diagonal;
  } else if (g_fault == Fault::kSingularNanWork) {
    work[2 * n - 2] =
        T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  } else if (g_fault == Fault::kSingularNonzeroWork) {
    work[2 * n - 2] = T{1};
  } else if (g_fault == Fault::kSingularRealOnlyWork) {
    if constexpr (asc::DenseBlasComplex<T>) {
      work[2 * n - 2].imag(old_last_diagonal.imag());
    }
  } else if (g_fault == Fault::kSingularWrongInfo) {
    *info = 1;
  }
}
}  // namespace
namespace asc_aasen_solve_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_info; }
}  // namespace asc_aasen_solve_fault_test
// Only the named solve is interposed; the preceding producer runs unchanged.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytrs_aa_base) __real_ssytrs_aa_;
void __wrap_ssytrs_aa_(const char* uplo, const lapack_int* n,
                       const lapack_int* nrhs, const float* a,
                       const lapack_int* lda, const lapack_int* pivots,
                       float* b, const lapack_int* ldb, float* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info, [&](lapack_int* native_info) {
    __real_ssytrs_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_ssytrs_aa_base),
                             decltype(__wrap_ssytrs_aa_)>);
decltype(LAPACK_dsytrs_aa_base) __real_dsytrs_aa_;
void __wrap_dsytrs_aa_(const char* uplo, const lapack_int* n,
                       const lapack_int* nrhs, const double* a,
                       const lapack_int* lda, const lapack_int* pivots,
                       double* b, const lapack_int* ldb, double* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info, [&](lapack_int* native_info) {
    __real_dsytrs_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_dsytrs_aa_base),
                             decltype(__wrap_dsytrs_aa_)>);
decltype(LAPACK_csytrs_aa_base) __real_csytrs_aa_;
void __wrap_csytrs_aa_(const char* uplo, const lapack_int* n,
                       const lapack_int* nrhs, const lapack_complex_float* a,
                       const lapack_int* lda, const lapack_int* pivots,
                       lapack_complex_float* b, const lapack_int* ldb,
                       lapack_complex_float* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, info, [&](lapack_int* native_info) {
    __real_csytrs_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_csytrs_aa_base),
                             decltype(__wrap_csytrs_aa_)>);
decltype(LAPACK_zsytrs_aa_base) __real_zsytrs_aa_;
void __wrap_zsytrs_aa_(const char* uplo, const lapack_int* n,
                       const lapack_int* nrhs, const lapack_complex_double* a,
                       const lapack_int* lda, const lapack_int* pivots,
                       lapack_complex_double* b, const lapack_int* ldb,
                       lapack_complex_double* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, info, [&](lapack_int* native_info) {
    __real_zsytrs_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_zsytrs_aa_base),
                             decltype(__wrap_zsytrs_aa_)>);
decltype(LAPACK_chetrs_aa_base) __real_chetrs_aa_;
void __wrap_chetrs_aa_(const char* uplo, const lapack_int* n,
                       const lapack_int* nrhs, const lapack_complex_float* a,
                       const lapack_int* lda, const lapack_int* pivots,
                       lapack_complex_float* b, const lapack_int* ldb,
                       lapack_complex_float* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, info, [&](lapack_int* native_info) {
    __real_chetrs_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_chetrs_aa_base),
                             decltype(__wrap_chetrs_aa_)>);
decltype(LAPACK_zhetrs_aa_base) __real_zhetrs_aa_;
void __wrap_zhetrs_aa_(const char* uplo, const lapack_int* n,
                       const lapack_int* nrhs, const lapack_complex_double* a,
                       const lapack_int* lda, const lapack_int* pivots,
                       lapack_complex_double* b, const lapack_int* ldb,
                       lapack_complex_double* work, const lapack_int* lwork,
                       lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, info, [&](lapack_int* native_info) {
    __real_zhetrs_aa_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_zhetrs_aa_base),
                             decltype(__wrap_zhetrs_aa_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
