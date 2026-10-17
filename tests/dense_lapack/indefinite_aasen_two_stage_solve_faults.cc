#include "indefinite_aasen_two_stage_solve_faults.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace {
using asc_aasen_two_stage_solve_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_info = 0;
void CorruptPivots(lapack_int n, const lapack_int* input_p,
                   const lapack_int* input_q) {
  // Only caller-owned private mutable INTEGER objects are fault-injected.
  auto* p = const_cast<lapack_int*>(input_p);
  auto* q = const_cast<lapack_int*>(input_q);
  if (g_fault == Fault::kZeroOuterPivot) {
    p[0] = 0;
  } else if (g_fault == Fault::kZeroBandPivot) {
    q[0] = 0;
  } else if (g_fault == Fault::kBackwardOuterPivot) {
    p[n - 1] = n - 1;
  } else if (g_fault == Fault::kBackwardBandPivot) {
    q[n - 1] = n - 1;
  } else if (g_fault == Fault::kUpperOuterPivot) {
    if constexpr (sizeof(lapack_int) == 8) {
      p[0] |= static_cast<lapack_int>(std::uint64_t{1} << 32);
    }
  } else if (g_fault == Fault::kUpperBandPivot) {
    if constexpr (sizeof(lapack_int) == 8) {
      q[0] |= static_cast<lapack_int>(std::uint64_t{1} << 32);
    }
  } else if (g_fault == Fault::kChangedOuterPivot && n == 3) {
    p[1] = p[1] == 2 ? 3 : 2;
  } else if (g_fault == Fault::kChangedBandPivot && n == 3) {
    q[0] = q[0] == 1 ? 2 : 1;
  }
}
template <typename Operation>
void Invoke(lapack_int n, const lapack_int* p, const lapack_int* q,
            lapack_int* info, Operation operation) {
  assert(n == 1 || n == 3);
  ++g_calls;
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
  } else if (g_fault == Fault::kPositiveInfo) {
    *info = 1;
  } else if (g_fault == Fault::kInfoAtOrder) {
    *info = n;
  } else if (g_fault == Fault::kInfoBeyondOrder) {
    *info = n + 1;
  }
  CorruptPivots(n, p, q);
}
}  // namespace
namespace asc_aasen_two_stage_solve_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_info; }
}  // namespace asc_aasen_two_stage_solve_fault_test
// Only the exact consumer is interposed; the producer is unchanged.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytrs_aa_2stage_base) __real_ssytrs_aa_2stage_;
void __wrap_ssytrs_aa_2stage_(const char* tri, const lapack_int* n,
                              const lapack_int* nrhs, const float* a,
                              const lapack_int* lda, float* tb,
                              const lapack_int* ltb, const lapack_int* p,
                              const lapack_int* q, float* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Invoke(*n, p, q, info, [&](lapack_int* native_info) {
    __real_ssytrs_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                             native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_ssytrs_aa_2stage_base),
                             decltype(__wrap_ssytrs_aa_2stage_)>);
decltype(LAPACK_dsytrs_aa_2stage_base) __real_dsytrs_aa_2stage_;
void __wrap_dsytrs_aa_2stage_(const char* tri, const lapack_int* n,
                              const lapack_int* nrhs, const double* a,
                              const lapack_int* lda, double* tb,
                              const lapack_int* ltb, const lapack_int* p,
                              const lapack_int* q, double* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Invoke(*n, p, q, info, [&](lapack_int* native_info) {
    __real_dsytrs_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                             native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_dsytrs_aa_2stage_base),
                             decltype(__wrap_dsytrs_aa_2stage_)>);
decltype(LAPACK_csytrs_aa_2stage_base) __real_csytrs_aa_2stage_;
void __wrap_csytrs_aa_2stage_(const char* tri, const lapack_int* n,
                              const lapack_int* nrhs,
                              const lapack_complex_float* a,
                              const lapack_int* lda, lapack_complex_float* tb,
                              const lapack_int* ltb, const lapack_int* p,
                              const lapack_int* q, lapack_complex_float* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Invoke(*n, p, q, info, [&](lapack_int* native_info) {
    __real_csytrs_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                             native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_csytrs_aa_2stage_base),
                             decltype(__wrap_csytrs_aa_2stage_)>);
decltype(LAPACK_zsytrs_aa_2stage_base) __real_zsytrs_aa_2stage_;
void __wrap_zsytrs_aa_2stage_(const char* tri, const lapack_int* n,
                              const lapack_int* nrhs,
                              const lapack_complex_double* a,
                              const lapack_int* lda, lapack_complex_double* tb,
                              const lapack_int* ltb, const lapack_int* p,
                              const lapack_int* q, lapack_complex_double* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Invoke(*n, p, q, info, [&](lapack_int* native_info) {
    __real_zsytrs_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                             native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_zsytrs_aa_2stage_base),
                             decltype(__wrap_zsytrs_aa_2stage_)>);
decltype(LAPACK_chetrs_aa_2stage_base) __real_chetrs_aa_2stage_;
void __wrap_chetrs_aa_2stage_(const char* tri, const lapack_int* n,
                              const lapack_int* nrhs,
                              const lapack_complex_float* a,
                              const lapack_int* lda, lapack_complex_float* tb,
                              const lapack_int* ltb, const lapack_int* p,
                              const lapack_int* q, lapack_complex_float* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Invoke(*n, p, q, info, [&](lapack_int* native_info) {
    __real_chetrs_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                             native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_chetrs_aa_2stage_base),
                             decltype(__wrap_chetrs_aa_2stage_)>);
decltype(LAPACK_zhetrs_aa_2stage_base) __real_zhetrs_aa_2stage_;
void __wrap_zhetrs_aa_2stage_(const char* tri, const lapack_int* n,
                              const lapack_int* nrhs,
                              const lapack_complex_double* a,
                              const lapack_int* lda, lapack_complex_double* tb,
                              const lapack_int* ltb, const lapack_int* p,
                              const lapack_int* q, lapack_complex_double* b,
                              const lapack_int* ldb, lapack_int* info,
                              std::size_t length) {
  Invoke(*n, p, q, info, [&](lapack_int* native_info) {
    __real_zhetrs_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                             native_info, length);
  });
}
static_assert(std::is_same_v<decltype(LAPACK_zhetrs_aa_2stage_base),
                             decltype(__wrap_zhetrs_aa_2stage_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
