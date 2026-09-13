#include "lu_expert_faults.h"

#include <cstddef>
#include <limits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_lapack_test::ExpertFault g_fault = asc_lapack_test::ExpertFault::kNone;
std::size_t g_query_calls = 0;
std::size_t g_execution_calls = 0;

void CorruptPivots(lapack_int n, lapack_int* pivots, lapack_int* info) {
  if (g_fault == asc_lapack_test::ExpertFault::kInvalidPivot && n > 0) {
    pivots[0] = 0;
  }
  if (g_fault == asc_lapack_test::ExpertFault::kExcessInfo) {
    *info = n + 1;
  }
}
}  // namespace

namespace asc_lapack_test {
void SetExpertFault(ExpertFault fault) { g_fault = fault; }
std::size_t InverseQueryCalls() { return g_query_calls; }
std::size_t InverseExecutionCalls() { return g_execution_calls; }
}  // namespace asc_lapack_test

// Test-only wrappers for the independently probed GNU ordinary/true64 symbol
// spelling. Synthetic results never count as successful provider numerics.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {

void __real_dgetrf2_(const lapack_int*, const lapack_int*, double*,
                     const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetrf2_(const lapack_int* m, const lapack_int* n, double* a,
                     const lapack_int* lda, lapack_int* pivots,
                     lapack_int* info) {
  if (g_fault == asc_lapack_test::ExpertFault::kNegativeInfo) {
    *info = -4;
    return;
  }
  __real_dgetrf2_(m, n, a, lda, pivots, info);
  CorruptPivots(*m, pivots, info);
}

void __real_dgetf2_(const lapack_int*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetf2_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  if (g_fault == asc_lapack_test::ExpertFault::kNegativeInfo) {
    *info = -4;
    return;
  }
  __real_dgetf2_(m, n, a, lda, pivots, info);
  CorruptPivots(*m, pivots, info);
}

void __real_dgesv_(const lapack_int*, const lapack_int*, double*,
                   const lapack_int*, lapack_int*, double*, const lapack_int*,
                   lapack_int*);
void __wrap_dgesv_(const lapack_int* n, const lapack_int* nrhs, double* a,
                   const lapack_int* lda, lapack_int* pivots, double* b,
                   const lapack_int* ldb, lapack_int* info) {
  if (g_fault == asc_lapack_test::ExpertFault::kNegativeInfo) {
    *info = -7;
    return;
  }
  __real_dgesv_(n, nrhs, a, lda, pivots, b, ldb, info);
  CorruptPivots(*n, pivots, info);
}
void __real_dgetri_(const lapack_int*, double*, const lapack_int*,
                    const lapack_int*, double*, const lapack_int*, lapack_int*);
void __wrap_dgetri_(const lapack_int* n, double* a, const lapack_int* lda,
                    const lapack_int* pivots, double* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (*lwork == -1) {
    ++g_query_calls;
  } else {
    ++g_execution_calls;
  }
  if (g_fault == asc_lapack_test::ExpertFault::kNegativeInfo) {
    *info = -6;
    return;
  }
  __real_dgetri_(n, a, lda, pivots, work, lwork, info);
  if (g_fault == asc_lapack_test::ExpertFault::kExcessInfo) {
    *info = *n + 1;
  }
  if (*lwork == -1) {
    if (g_fault == asc_lapack_test::ExpertFault::kQueryNan) {
      *work = std::numeric_limits<double>::quiet_NaN();
    }
    if (g_fault == asc_lapack_test::ExpertFault::kQueryInfinity) {
      *work = std::numeric_limits<double>::infinity();
    }
    if (g_fault == asc_lapack_test::ExpertFault::kQueryNegative) {
      *work = -1;
    }
    if (g_fault == asc_lapack_test::ExpertFault::kQueryShort) {
      *work = 1;
    }
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
