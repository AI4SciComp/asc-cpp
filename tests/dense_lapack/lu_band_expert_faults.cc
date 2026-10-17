#include "lu_band_expert_faults.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace {
using asc_lu_band_expert_faults::Fault;
using asc_lu_band_expert_faults::Routine;
Routine g_routine = Routine::kEquilibrate;
Fault g_fault = Fault::kPass;
int g_calls = 0;
bool g_info_sentinel = false;
bool g_pivot_sentinels = false;
bool Intercept(Routine routine, const lapack_int* info) {
  if (routine != g_routine) {
    return false;
  }
  ++g_calls;
  g_info_sentinel = *info == std::numeric_limits<lapack_int>::min();
  return g_fault != Fault::kPass;
}
bool NumericalFault() {
  return g_fault == Fault::kNegativeError ||
         g_fault == Fault::kNonfiniteError || g_fault == Fault::kMixedError ||
         g_fault == Fault::kNegativeStats ||
         g_fault == Fault::kNonfiniteStats || g_fault == Fault::kAccuracy;
}
void Info(lapack_int maximum, lapack_int* info) {
  if (g_fault == Fault::kNoInfo) {
    return;
  }
  if (g_fault == Fault::kPartialInfo) {
    const std::uint16_t partial = 0;
    std::memcpy(info, &partial, sizeof(partial));
    return;
  }
  *info = 0;
  if (g_fault == Fault::kNegative) {
    *info = -4;
  }
  if (g_fault == Fault::kMinimum) {
    *info = std::numeric_limits<lapack_int>::min();
  }
  if (g_fault == Fault::kLargePositive) {
    *info = maximum + 1;
  }
}
void Pivots(lapack_int n, lapack_int* pivots) {
  g_pivot_sentinels = true;
  for (lapack_int i = 0; i < n; ++i) {
    g_pivot_sentinels &= pivots[i] == std::numeric_limits<lapack_int>::min();
  }
  if (g_fault == Fault::kNoPivots) {
    return;
  }
  for (lapack_int i = 0; i < n; ++i) {
    if (g_fault == Fault::kPartialPivot && i == n - 1) {
      const auto partial = static_cast<std::uint16_t>(i + 1);
      std::memcpy(pivots + i, &partial, sizeof(partial));
    } else {
      pivots[i] = i + 1;
    }
  }
  if (g_fault == Fault::kLatePivot && n > 0) {
    pivots[n - 1] = n + 1;
  }
}
template <typename Real>
void Errors(lapack_int nrhs, Real* ferr, Real* berr) {
  if (nrhs == 0) {
    return;
  }
  if (g_fault == Fault::kNegativeError) {
    ferr[0] = -1;
  }
  if (g_fault == Fault::kNonfiniteError || g_fault == Fault::kMixedError) {
    ferr[0] = std::numeric_limits<Real>::quiet_NaN();
  }
  if (g_fault == Fault::kMixedError && nrhs > 1) {
    berr[1] = -1;
  }
}
template <typename Real>
void Stats(Real* rcond) {
  if (g_fault == Fault::kNegativeStats) {
    *rcond = -1;
  }
  if (g_fault == Fault::kNonfiniteStats) {
    *rcond = std::numeric_limits<Real>::quiet_NaN();
  }
}
}  // namespace
namespace asc_lu_band_expert_faults {
void Select(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
  g_calls = 0;
  g_info_sentinel = false;
  g_pivot_sentinels = false;
}
int Calls() { return g_calls; }
bool SawInfoSentinel() { return g_info_sentinel; }
bool SawPivotSentinels() { return g_pivot_sentinels; }
}  // namespace asc_lu_band_expert_faults
// GNU ld reserves these exact wrapper names for diagnostic-only interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku, float const* ab,
                    lapack_int const* ldab, float* r, float* c, float* rowcnd,
                    float* colcnd, float* amax, lapack_int* info);
void __wrap_sgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku, float const* ab,
                    lapack_int const* ldab, float* r, float* c, float* rowcnd,
                    float* colcnd, float* amax, lapack_int* info) {
  if (!Intercept(Routine::kEquilibrate, info)) {
    __real_sgbequ_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
  Info(*m + *n, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbequ_), decltype(LAPACK_sgbequ)>);
void __real_sgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, float const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    float const* anorm, float* rcond, float* work,
                    lapack_int* iwork, lapack_int* info, FORTRAN_STRLEN length);
void __wrap_sgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, float const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    float const* anorm, float* rcond, float* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kCondition, info)) {
    __real_sgbcon_(norm, n, kl, ku, ab, ldab, ipiv, anorm, rcond, work, iwork,
                   info, length);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbcon_), decltype(LAPACK_sgbcon_base)>);
void __real_sgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, float const* ab,
                    lapack_int const* ldab, float const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    float const* b, lapack_int const* ldb, float* x,
                    lapack_int const* ldx, float* ferr, float* berr,
                    float* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN length);
void __wrap_sgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, float const* ab,
                    lapack_int const* ldab, float const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    float const* b, lapack_int const* ldb, float* x,
                    lapack_int const* ldx, float* ferr, float* berr,
                    float* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kRefine, info)) {
    __real_sgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, iwork, info, length);
    return;
  }
  if (NumericalFault()) {
    __real_sgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, iwork, info, length);
    Errors(*nrhs, ferr, berr);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbrfs_), decltype(LAPACK_sgbrfs_base)>);
void __real_sgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs, float* ab,
                   lapack_int const* ldab, lapack_int* ipiv, float* b,
                   lapack_int const* ldb, lapack_int* info);
void __wrap_sgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs, float* ab,
                   lapack_int const* ldab, lapack_int* ipiv, float* b,
                   lapack_int const* ldb, lapack_int* info) {
  if (!Intercept(Routine::kSolve, info)) {
    __real_sgbsv_(n, kl, ku, nrhs, ab, ldab, ipiv, b, ldb, info);
    return;
  }
  Pivots(*n, ipiv);
  Info(*n, info);
}
static_assert(std::is_same_v<decltype(__wrap_sgbsv_), decltype(LAPACK_sgbsv)>);
void __real_sgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, float* ab, lapack_int const* ldab,
                    float* afb, lapack_int const* ldafb, lapack_int* ipiv,
                    char* equed, float* r, float* c, float* b,
                    lapack_int const* ldb, float* x, lapack_int const* ldx,
                    float* rcond, float* ferr, float* berr, float* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
                    FORTRAN_STRLEN equed_length);
void __wrap_sgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, float* ab, lapack_int const* ldab,
                    float* afb, lapack_int const* ldafb, lapack_int* ipiv,
                    char* equed, float* r, float* c, float* b,
                    lapack_int const* ldb, float* x, lapack_int const* ldx,
                    float* rcond, float* ferr, float* berr, float* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
                    FORTRAN_STRLEN equed_length) {
  if (!Intercept(Routine::kDriver, info)) {
    __real_sgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, iwork,
                   info, fact_length, trans_length, equed_length);
    return;
  }
  if (NumericalFault()) {
    __real_sgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, iwork,
                   info, fact_length, trans_length, equed_length);
    Errors(*nrhs, ferr, berr);
    Stats(rcond);
    if (g_fault == Fault::kAccuracy) {
      *info = *n + 1;
    }
    return;
  }
  if (*fact != 'F') {
    Pivots(*n, ipiv);
    *equed = 'N';
  }
  if (g_fault == Fault::kInvalidEqued) {
    *equed = '?';
  }
  Info(*n + 1, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbsvx_), decltype(LAPACK_sgbsvx_base)>);
void __real_dgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    double const* ab, lapack_int const* ldab, double* r,
                    double* c, double* rowcnd, double* colcnd, double* amax,
                    lapack_int* info);
void __wrap_dgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    double const* ab, lapack_int const* ldab, double* r,
                    double* c, double* rowcnd, double* colcnd, double* amax,
                    lapack_int* info) {
  if (!Intercept(Routine::kEquilibrate, info)) {
    __real_dgbequ_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
  Info(*m + *n, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbequ_), decltype(LAPACK_dgbequ)>);
void __real_dgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, double const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    double const* anorm, double* rcond, double* work,
                    lapack_int* iwork, lapack_int* info, FORTRAN_STRLEN length);
void __wrap_dgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, double const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    double const* anorm, double* rcond, double* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kCondition, info)) {
    __real_dgbcon_(norm, n, kl, ku, ab, ldab, ipiv, anorm, rcond, work, iwork,
                   info, length);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbcon_), decltype(LAPACK_dgbcon_base)>);
void __real_dgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, double const* ab,
                    lapack_int const* ldab, double const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    double const* b, lapack_int const* ldb, double* x,
                    lapack_int const* ldx, double* ferr, double* berr,
                    double* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN length);
void __wrap_dgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, double const* ab,
                    lapack_int const* ldab, double const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    double const* b, lapack_int const* ldb, double* x,
                    lapack_int const* ldx, double* ferr, double* berr,
                    double* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kRefine, info)) {
    __real_dgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, iwork, info, length);
    return;
  }
  if (NumericalFault()) {
    __real_dgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, iwork, info, length);
    Errors(*nrhs, ferr, berr);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbrfs_), decltype(LAPACK_dgbrfs_base)>);
void __real_dgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs, double* ab,
                   lapack_int const* ldab, lapack_int* ipiv, double* b,
                   lapack_int const* ldb, lapack_int* info);
void __wrap_dgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs, double* ab,
                   lapack_int const* ldab, lapack_int* ipiv, double* b,
                   lapack_int const* ldb, lapack_int* info) {
  if (!Intercept(Routine::kSolve, info)) {
    __real_dgbsv_(n, kl, ku, nrhs, ab, ldab, ipiv, b, ldb, info);
    return;
  }
  Pivots(*n, ipiv);
  Info(*n, info);
}
static_assert(std::is_same_v<decltype(__wrap_dgbsv_), decltype(LAPACK_dgbsv)>);
void __real_dgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, double* ab, lapack_int const* ldab,
                    double* afb, lapack_int const* ldafb, lapack_int* ipiv,
                    char* equed, double* r, double* c, double* b,
                    lapack_int const* ldb, double* x, lapack_int const* ldx,
                    double* rcond, double* ferr, double* berr, double* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
                    FORTRAN_STRLEN equed_length);
void __wrap_dgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, double* ab, lapack_int const* ldab,
                    double* afb, lapack_int const* ldafb, lapack_int* ipiv,
                    char* equed, double* r, double* c, double* b,
                    lapack_int const* ldb, double* x, lapack_int const* ldx,
                    double* rcond, double* ferr, double* berr, double* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
                    FORTRAN_STRLEN equed_length) {
  if (!Intercept(Routine::kDriver, info)) {
    __real_dgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, iwork,
                   info, fact_length, trans_length, equed_length);
    return;
  }
  if (NumericalFault()) {
    __real_dgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, iwork,
                   info, fact_length, trans_length, equed_length);
    Errors(*nrhs, ferr, berr);
    Stats(rcond);
    if (g_fault == Fault::kAccuracy) {
      *info = *n + 1;
    }
    return;
  }
  if (*fact != 'F') {
    Pivots(*n, ipiv);
    *equed = 'N';
  }
  if (g_fault == Fault::kInvalidEqued) {
    *equed = '?';
  }
  Info(*n + 1, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbsvx_), decltype(LAPACK_dgbsvx_base)>);
void __real_cgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_complex_float const* ab, lapack_int const* ldab,
                    float* r, float* c, float* rowcnd, float* colcnd,
                    float* amax, lapack_int* info);
void __wrap_cgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_complex_float const* ab, lapack_int const* ldab,
                    float* r, float* c, float* rowcnd, float* colcnd,
                    float* amax, lapack_int* info) {
  if (!Intercept(Routine::kEquilibrate, info)) {
    __real_cgbequ_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
  Info(*m + *n, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbequ_), decltype(LAPACK_cgbequ)>);
void __real_cgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, lapack_complex_float const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    float const* anorm, float* rcond,
                    lapack_complex_float* work, float* rwork, lapack_int* info,
                    FORTRAN_STRLEN length);
void __wrap_cgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, lapack_complex_float const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    float const* anorm, float* rcond,
                    lapack_complex_float* work, float* rwork, lapack_int* info,
                    FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kCondition, info)) {
    __real_cgbcon_(norm, n, kl, ku, ab, ldab, ipiv, anorm, rcond, work, rwork,
                   info, length);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbcon_), decltype(LAPACK_cgbcon_base)>);
void __real_cgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_float const* ab,
                    lapack_int const* ldab, lapack_complex_float const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    lapack_complex_float const* b, lapack_int const* ldb,
                    lapack_complex_float* x, lapack_int const* ldx, float* ferr,
                    float* berr, lapack_complex_float* work, float* rwork,
                    lapack_int* info, FORTRAN_STRLEN length);
void __wrap_cgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_float const* ab,
                    lapack_int const* ldab, lapack_complex_float const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    lapack_complex_float const* b, lapack_int const* ldb,
                    lapack_complex_float* x, lapack_int const* ldx, float* ferr,
                    float* berr, lapack_complex_float* work, float* rwork,
                    lapack_int* info, FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kRefine, info)) {
    __real_cgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, rwork, info, length);
    return;
  }
  if (NumericalFault()) {
    __real_cgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, rwork, info, length);
    Errors(*nrhs, ferr, berr);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbrfs_), decltype(LAPACK_cgbrfs_base)>);
void __real_cgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs,
                   lapack_complex_float* ab, lapack_int const* ldab,
                   lapack_int* ipiv, lapack_complex_float* b,
                   lapack_int const* ldb, lapack_int* info);
void __wrap_cgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs,
                   lapack_complex_float* ab, lapack_int const* ldab,
                   lapack_int* ipiv, lapack_complex_float* b,
                   lapack_int const* ldb, lapack_int* info) {
  if (!Intercept(Routine::kSolve, info)) {
    __real_cgbsv_(n, kl, ku, nrhs, ab, ldab, ipiv, b, ldb, info);
    return;
  }
  Pivots(*n, ipiv);
  Info(*n, info);
}
static_assert(std::is_same_v<decltype(__wrap_cgbsv_), decltype(LAPACK_cgbsv)>);
void __real_cgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_float* ab,
                    lapack_int const* ldab, lapack_complex_float* afb,
                    lapack_int const* ldafb, lapack_int* ipiv, char* equed,
                    float* r, float* c, lapack_complex_float* b,
                    lapack_int const* ldb, lapack_complex_float* x,
                    lapack_int const* ldx, float* rcond, float* ferr,
                    float* berr, lapack_complex_float* work, float* rwork,
                    lapack_int* info, FORTRAN_STRLEN fact_length,
                    FORTRAN_STRLEN trans_length, FORTRAN_STRLEN equed_length);
void __wrap_cgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_float* ab,
                    lapack_int const* ldab, lapack_complex_float* afb,
                    lapack_int const* ldafb, lapack_int* ipiv, char* equed,
                    float* r, float* c, lapack_complex_float* b,
                    lapack_int const* ldb, lapack_complex_float* x,
                    lapack_int const* ldx, float* rcond, float* ferr,
                    float* berr, lapack_complex_float* work, float* rwork,
                    lapack_int* info, FORTRAN_STRLEN fact_length,
                    FORTRAN_STRLEN trans_length, FORTRAN_STRLEN equed_length) {
  if (!Intercept(Routine::kDriver, info)) {
    __real_cgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, rwork,
                   info, fact_length, trans_length, equed_length);
    return;
  }
  if (NumericalFault()) {
    __real_cgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, rwork,
                   info, fact_length, trans_length, equed_length);
    Errors(*nrhs, ferr, berr);
    Stats(rcond);
    if (g_fault == Fault::kAccuracy) {
      *info = *n + 1;
    }
    return;
  }
  if (*fact != 'F') {
    Pivots(*n, ipiv);
    *equed = 'N';
  }
  if (g_fault == Fault::kInvalidEqued) {
    *equed = '?';
  }
  Info(*n + 1, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbsvx_), decltype(LAPACK_cgbsvx_base)>);
void __real_zgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_complex_double const* ab, lapack_int const* ldab,
                    double* r, double* c, double* rowcnd, double* colcnd,
                    double* amax, lapack_int* info);
void __wrap_zgbequ_(lapack_int const* m, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_complex_double const* ab, lapack_int const* ldab,
                    double* r, double* c, double* rowcnd, double* colcnd,
                    double* amax, lapack_int* info) {
  if (!Intercept(Routine::kEquilibrate, info)) {
    __real_zgbequ_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
  Info(*m + *n, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbequ_), decltype(LAPACK_zgbequ)>);
void __real_zgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, lapack_complex_double const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    double const* anorm, double* rcond,
                    lapack_complex_double* work, double* rwork,
                    lapack_int* info, FORTRAN_STRLEN length);
void __wrap_zgbcon_(char const* norm, lapack_int const* n, lapack_int const* kl,
                    lapack_int const* ku, lapack_complex_double const* ab,
                    lapack_int const* ldab, lapack_int const* ipiv,
                    double const* anorm, double* rcond,
                    lapack_complex_double* work, double* rwork,
                    lapack_int* info, FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kCondition, info)) {
    __real_zgbcon_(norm, n, kl, ku, ab, ldab, ipiv, anorm, rcond, work, rwork,
                   info, length);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbcon_), decltype(LAPACK_zgbcon_base)>);
void __real_zgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_double const* ab,
                    lapack_int const* ldab, lapack_complex_double const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    lapack_complex_double const* b, lapack_int const* ldb,
                    lapack_complex_double* x, lapack_int const* ldx,
                    double* ferr, double* berr, lapack_complex_double* work,
                    double* rwork, lapack_int* info, FORTRAN_STRLEN length);
void __wrap_zgbrfs_(char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_double const* ab,
                    lapack_int const* ldab, lapack_complex_double const* afb,
                    lapack_int const* ldafb, lapack_int const* ipiv,
                    lapack_complex_double const* b, lapack_int const* ldb,
                    lapack_complex_double* x, lapack_int const* ldx,
                    double* ferr, double* berr, lapack_complex_double* work,
                    double* rwork, lapack_int* info, FORTRAN_STRLEN length) {
  if (!Intercept(Routine::kRefine, info)) {
    __real_zgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, rwork, info, length);
    return;
  }
  if (NumericalFault()) {
    __real_zgbrfs_(trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv, b, ldb,
                   x, ldx, ferr, berr, work, rwork, info, length);
    Errors(*nrhs, ferr, berr);
    return;
  }
  Info(0, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbrfs_), decltype(LAPACK_zgbrfs_base)>);
void __real_zgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs,
                   lapack_complex_double* ab, lapack_int const* ldab,
                   lapack_int* ipiv, lapack_complex_double* b,
                   lapack_int const* ldb, lapack_int* info);
void __wrap_zgbsv_(lapack_int const* n, lapack_int const* kl,
                   lapack_int const* ku, lapack_int const* nrhs,
                   lapack_complex_double* ab, lapack_int const* ldab,
                   lapack_int* ipiv, lapack_complex_double* b,
                   lapack_int const* ldb, lapack_int* info) {
  if (!Intercept(Routine::kSolve, info)) {
    __real_zgbsv_(n, kl, ku, nrhs, ab, ldab, ipiv, b, ldb, info);
    return;
  }
  Pivots(*n, ipiv);
  Info(*n, info);
}
static_assert(std::is_same_v<decltype(__wrap_zgbsv_), decltype(LAPACK_zgbsv)>);
void __real_zgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_double* ab,
                    lapack_int const* ldab, lapack_complex_double* afb,
                    lapack_int const* ldafb, lapack_int* ipiv, char* equed,
                    double* r, double* c, lapack_complex_double* b,
                    lapack_int const* ldb, lapack_complex_double* x,
                    lapack_int const* ldx, double* rcond, double* ferr,
                    double* berr, lapack_complex_double* work, double* rwork,
                    lapack_int* info, FORTRAN_STRLEN fact_length,
                    FORTRAN_STRLEN trans_length, FORTRAN_STRLEN equed_length);
void __wrap_zgbsvx_(char const* fact, char const* trans, lapack_int const* n,
                    lapack_int const* kl, lapack_int const* ku,
                    lapack_int const* nrhs, lapack_complex_double* ab,
                    lapack_int const* ldab, lapack_complex_double* afb,
                    lapack_int const* ldafb, lapack_int* ipiv, char* equed,
                    double* r, double* c, lapack_complex_double* b,
                    lapack_int const* ldb, lapack_complex_double* x,
                    lapack_int const* ldx, double* rcond, double* ferr,
                    double* berr, lapack_complex_double* work, double* rwork,
                    lapack_int* info, FORTRAN_STRLEN fact_length,
                    FORTRAN_STRLEN trans_length, FORTRAN_STRLEN equed_length) {
  if (!Intercept(Routine::kDriver, info)) {
    __real_zgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, rwork,
                   info, fact_length, trans_length, equed_length);
    return;
  }
  if (NumericalFault()) {
    __real_zgbsvx_(fact, trans, n, kl, ku, nrhs, ab, ldab, afb, ldafb, ipiv,
                   equed, r, c, b, ldb, x, ldx, rcond, ferr, berr, work, rwork,
                   info, fact_length, trans_length, equed_length);
    Errors(*nrhs, ferr, berr);
    Stats(rcond);
    if (g_fault == Fault::kAccuracy) {
      *info = *n + 1;
    }
    return;
  }
  if (*fact != 'F') {
    Pivots(*n, ipiv);
    *equed = 'N';
  }
  if (g_fault == Fault::kInvalidEqued) {
    *equed = '?';
  }
  Info(*n + 1, info);
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbsvx_), decltype(LAPACK_zgbsvx_base)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
