#include "indefinite_rook_faults.h"

#include <cstddef>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_prototypes.h"

namespace {
using asc_indefinite_rook_test::Fault;
using asc_indefinite_rook_test::ForeignRoutine;
ForeignRoutine g_routine = ForeignRoutine::kTrf;
Fault g_fault = Fault::kNone;

void Corrupt(lapack_int n, lapack_int* pivots, lapack_int* info) {
  if (g_fault == Fault::kExcessInfo) {
    *info = n + 1;
  }
  if (g_fault == Fault::kPivotMinimum && n > 0) {
    pivots[0] = std::numeric_limits<lapack_int>::min();
  }
  if (g_fault == Fault::kBrokenPair && n > 1) {
    pivots[1] = 1;
  }
}
}  // namespace

namespace asc_indefinite_rook_test {
void SetFault(ForeignRoutine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
}
}  // namespace asc_indefinite_rook_test

// Test-only ELF wrappers, signatures checked against the actual pinned header
// and compiler-emitted TF2 declarations. Synthetic returns are failure-path
// evidence only, never mathematical provider evidence.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(dsytrf_rook_) __real_dsytrf_rook_;
decltype(dsytf2_rook_) __real_dsytf2_rook_;
decltype(dsytrs_rook_) __real_dsytrs_rook_;

void __wrap_dsytrf_rook_(const char* uplo, const lapack_int* n, double* a,
                         const lapack_int* lda, lapack_int* pivots,
                         double* work, const lapack_int* lwork,
                         lapack_int* info, std::size_t length) {
  if (g_routine == ForeignRoutine::kTrf && g_fault == Fault::kNegativeInfo) {
    *info = -4;
    return;
  }
  __real_dsytrf_rook_(uplo, n, a, lda, pivots, work, lwork, info, length);
  if (g_routine == ForeignRoutine::kTrf) {
    Corrupt(*n, pivots, info);
    if (g_fault == Fault::kWorkNan) {
      *work = std::numeric_limits<double>::quiet_NaN();
    }
  }
}
void __wrap_dsytf2_rook_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                         lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  if (g_routine == ForeignRoutine::kTf2 && g_fault == Fault::kNegativeInfo) {
    *info = -4;
    return;
  }
  __real_dsytf2_rook_(uplo, n, a, lda, pivots, info, length);
  if (g_routine == ForeignRoutine::kTf2) {
    Corrupt(*n, pivots, info);
  }
}
void __wrap_dsytrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const double* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         double* b, const lapack_int* ldb, lapack_int* info,
                         std::size_t length) {
  if (g_routine == ForeignRoutine::kTrs && g_fault == Fault::kNegativeInfo) {
    *info = -8;
    return;
  }
  __real_dsytrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, info, length);
  if (g_routine == ForeignRoutine::kTrs && g_fault == Fault::kExcessInfo) {
    *info = 1;
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
