#include "indefinite_aasen_faults.h"

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
using asc_aasen_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_info = 0;
template <typename T, typename Operation>
void Invoke(lapack_int n, lapack_int* pivots, T* work, lapack_int* info,
            Operation operation) {
  assert(n == 1 || n == 3);
  ++g_calls;
  std::array<lapack_int, 3> output{};
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  const T old_work = work[0];
  operation(output.data(), &native_info);
  g_info = native_info;
  if (g_fault == Fault::kWrite32BitInfo) {
    const std::uint32_t zero = 0;
    std::memcpy(info, &zero, sizeof(zero));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -7;
  } else if (g_fault == Fault::kPositiveInfo) {
    *info = 1;
  }
  for (lapack_int i = 0; i < n; ++i) {
    if (g_fault == Fault::kWrite32BitPivots) {
      const auto partial = static_cast<std::int32_t>(output[i]);
      std::memcpy(pivots + i, &partial, sizeof(partial));
    } else if (g_fault != Fault::kOmitPivots) {
      pivots[i] = output[i];
    }
  }
  if (g_fault == Fault::kZeroPivot) {
    pivots[0] = 0;
  } else if (g_fault == Fault::kOutOfBoundsPivot) {
    pivots[n - 1] = n + 1;
  } else if (g_fault == Fault::kWrongFirstPivot) {
    pivots[0] = 2;
  } else if (g_fault == Fault::kBackwardPivot) {
    pivots[n - 1] = n - 1;
  } else if (g_fault == Fault::kNegativePivot) {
    pivots[0] = -1;
  }
  if (g_fault == Fault::kOmitWork) {
    work[0] = old_work;
  } else if (g_fault == Fault::kNanWork) {
    work[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  } else if (g_fault == Fault::kIncorrectWork) {
    work[0] += T{1};
  } else if (g_fault == Fault::kRealOnlyWork) {
    if constexpr (asc::DenseBlasComplex<T>) {
      work[0].imag(old_work.imag());
    }
  }
}
}  // namespace
namespace asc_aasen_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_info; }
}  // namespace asc_aasen_fault_test
// Inject only output defects after the named native routine executes.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytrf_aa_base) __real_ssytrf_aa_;
void __wrap_ssytrf_aa_(const char* uplo, const lapack_int* n, float* a,
                       const lapack_int* lda, lapack_int* pivots, float* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info,
         [&](lapack_int* output, lapack_int* native_info) {
           __real_ssytrf_aa_(uplo, n, a, lda, output, work, lwork, native_info,
                             length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_ssytrf_aa_base),
                             decltype(__wrap_ssytrf_aa_)>);
decltype(LAPACK_dsytrf_aa_base) __real_dsytrf_aa_;
void __wrap_dsytrf_aa_(const char* uplo, const lapack_int* n, double* a,
                       const lapack_int* lda, lapack_int* pivots, double* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info,
         [&](lapack_int* output, lapack_int* native_info) {
           __real_dsytrf_aa_(uplo, n, a, lda, output, work, lwork, native_info,
                             length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_dsytrf_aa_base),
                             decltype(__wrap_dsytrf_aa_)>);
decltype(LAPACK_csytrf_aa_base) __real_csytrf_aa_;
void __wrap_csytrf_aa_(const char* uplo, const lapack_int* n,
                       lapack_complex_float* a, const lapack_int* lda,
                       lapack_int* pivots, lapack_complex_float* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info,
         [&](lapack_int* output, lapack_int* native_info) {
           __real_csytrf_aa_(uplo, n, a, lda, output, work, lwork, native_info,
                             length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_csytrf_aa_base),
                             decltype(__wrap_csytrf_aa_)>);
decltype(LAPACK_zsytrf_aa_base) __real_zsytrf_aa_;
void __wrap_zsytrf_aa_(const char* uplo, const lapack_int* n,
                       lapack_complex_double* a, const lapack_int* lda,
                       lapack_int* pivots, lapack_complex_double* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info,
         [&](lapack_int* output, lapack_int* native_info) {
           __real_zsytrf_aa_(uplo, n, a, lda, output, work, lwork, native_info,
                             length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_zsytrf_aa_base),
                             decltype(__wrap_zsytrf_aa_)>);
decltype(LAPACK_chetrf_aa_base) __real_chetrf_aa_;
void __wrap_chetrf_aa_(const char* uplo, const lapack_int* n,
                       lapack_complex_float* a, const lapack_int* lda,
                       lapack_int* pivots, lapack_complex_float* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info,
         [&](lapack_int* output, lapack_int* native_info) {
           __real_chetrf_aa_(uplo, n, a, lda, output, work, lwork, native_info,
                             length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_chetrf_aa_base),
                             decltype(__wrap_chetrf_aa_)>);
decltype(LAPACK_zhetrf_aa_base) __real_zhetrf_aa_;
void __wrap_zhetrf_aa_(const char* uplo, const lapack_int* n,
                       lapack_complex_double* a, const lapack_int* lda,
                       lapack_int* pivots, lapack_complex_double* work,
                       const lapack_int* lwork, lapack_int* info,
                       std::size_t length) {
  Invoke(*n, pivots, work, info,
         [&](lapack_int* output, lapack_int* native_info) {
           __real_zhetrf_aa_(uplo, n, a, lda, output, work, lwork, native_info,
                             length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_zhetrf_aa_base),
                             decltype(__wrap_zhetrf_aa_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
