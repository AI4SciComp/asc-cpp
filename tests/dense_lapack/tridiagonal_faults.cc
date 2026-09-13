#include "tridiagonal_faults.h"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"

namespace asc_tridiagonal_fault {
namespace {
Operation g_operation = Operation::kFactor;
Mode g_mode = Mode::kNone;
std::size_t g_calls = 0;
std::size_t g_focus_calls = 0;
bool g_integer_segments_valid = true;
bool g_character_lengths_valid = true;
bool g_zero_rhs_observed = false;

Mode Enter(Operation operation) {
  ++g_calls;
  if (operation == g_operation) {
    ++g_focus_calls;
    return g_mode;
  }
  return Mode::kNone;
}
bool TransposeLength(const char* value, FORTRAN_STRLEN length) {
  if (g_operation == Operation::kSolve) {
    return length ==
           1;  // Direct ASC character actual, not an internal literal.
  }
  // Exact pinned GTCON source passes these full literal actual arguments to
  // its CHARACTER*1 GTTRS dummy. Their emitted lengths are not one.
  return length == 1 ||
         (length == 12 && std::string_view(value, length) == "No transpose") ||
         (length == 9 && std::string_view(value, length) == "Transpose") ||
         (length == 19 &&
          std::string_view(value, length) == "Conjugate transpose");
}
bool Preflight(Mode mode, lapack_int n, lapack_int* info) {
  if (mode == Mode::kNegativeInfo) {
    *info = -1;
    return true;
  }
  if (mode == Mode::kExcessInfo) {
    *info = n + 2;
    return true;
  }
  return false;
}
// Only the first pivot is the output under test; scalar fixtures ensure no
// other unwritten pivot can hide a partially written native integer.
bool PivotOutput(Mode mode, lapack_int n, lapack_int* pivots,
                 lapack_int* info) {
  if (n != 1 ||
      (mode != Mode::kUnwrittenPivot && mode != Mode::kPartialPivotWidth)) {
    return false;
  }
  if (mode == Mode::kPartialPivotWidth) {
    const std::int32_t valid_pivot = 1;
    std::memcpy(pivots, &valid_pivot, sizeof(valid_pivot));
  }
  *info = 0;
  return true;
}
template <typename Real>
void Diagnostic(Mode mode, Real* value) {
  if (mode == Mode::kNegativeDiagnostic) {
    *value = -1;
  } else if (mode == Mode::kNonfiniteDiagnostic) {
    *value = std::numeric_limits<Real>::quiet_NaN();
  }
}
}  // namespace

void Reset(Operation operation, Mode mode) {
  g_operation = operation;
  g_mode = mode;
  g_calls = 0;
  g_focus_calls = 0;
  g_integer_segments_valid = true;
  g_character_lengths_valid = true;
  g_zero_rhs_observed = false;
}
std::size_t Calls() { return g_calls; }
std::size_t FocusCalls() { return g_focus_calls; }
bool IntegerSegmentsValid() { return g_integer_segments_valid; }
bool CharacterLengthsValid() { return g_character_lengths_valid; }
bool ZeroRhsObserved() { return g_zero_rhs_observed; }

// GNU ld requires these exact __wrap_/__real_ spellings. This private
// test-only exception matches the repository allocation and ABI probes.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_sgttrf) __real_sgttrf_;
void __wrap_sgttrf_(const lapack_int* n, float* dl, float* d, float* du,
                    float* du2, lapack_int* ipiv, lapack_int* info) {
  const Mode mode = Enter(Operation::kFactor);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }

  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_sgttrf_(n, dl, d, du, du2, ipiv, info);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
}

decltype(LAPACK_sgttrs_base) __real_sgttrs_;
void __wrap_sgttrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const float* dl, const float* d,
                    const float* du, const float* du2, const lapack_int* ipiv,
                    float* b, const lapack_int* ldb, lapack_int* info,
                    FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kSolve);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid =
      g_character_lengths_valid && TransposeLength(trans, trans_length);
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_sgttrs_(trans, n, nrhs, dl, d, du, du2, ipiv, b, ldb, info,
                 trans_length);
}

decltype(LAPACK_sgtsv) __real_sgtsv_;
void __wrap_sgtsv_(const lapack_int* n, const lapack_int* nrhs, float* dl,
                   float* d, float* du, float* b, const lapack_int* ldb,
                   lapack_int* info) {
  const Mode mode = Enter(Operation::kDriver);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }

  if (*n > 0 && *nrhs == 0) {
    g_zero_rhs_observed =
        std::all_of(b, b + *n, [](const float value) { return value == 0; });
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_sgtsv_(n, nrhs, dl, d, du, b, ldb, info);
}

decltype(LAPACK_sgtcon_base) __real_sgtcon_;
void __wrap_sgtcon_(const char* norm, const lapack_int* n, const float* dl,
                    const float* d, const float* du, const float* du2,
                    const lapack_int* ipiv, const float* anorm, float* rcond,
                    float* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN norm_length) {
  const Mode mode = Enter(Operation::kCondition);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && norm_length == 1;
  if (*n > 0) {
    g_integer_segments_valid =
        g_integer_segments_valid &&
        static_cast<const void*>(iwork) == static_cast<const void*>(ipiv + *n);
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_sgtcon_(norm, n, dl, d, du, du2, ipiv, anorm, rcond, work, iwork, info,
                 norm_length);
  Diagnostic(mode, rcond);
}

decltype(LAPACK_sgtrfs_base) __real_sgtrfs_;
void __wrap_sgtrfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const float* dl, const float* d,
                    const float* du, const float* dlf, const float* df,
                    const float* duf, const float* du2, const lapack_int* ipiv,
                    const float* b, const lapack_int* ldb, float* x,
                    const lapack_int* ldx, float* ferr, float* berr,
                    float* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kRefinement);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (*n > 0) {
    g_integer_segments_valid =
        g_integer_segments_valid &&
        static_cast<const void*>(iwork) == static_cast<const void*>(ipiv + *n);
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_sgtrfs_(trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b, ldb, x,
                 ldx, ferr, berr, work, iwork, info, trans_length);
  if (*nrhs > 0) {
    Diagnostic(mode, ferr);
  }
}

decltype(LAPACK_sgtsvx_base) __real_sgtsvx_;
void __wrap_sgtsvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const float* dl, const float* d,
                    const float* du, float* dlf, float* df, float* duf,
                    float* du2, lapack_int* ipiv, const float* b,
                    const lapack_int* ldb, float* x, const lapack_int* ldx,
                    float* rcond, float* ferr, float* berr, float* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kExpert);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }
  g_character_lengths_valid = g_character_lengths_valid && fact_length == 1;
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (*n > 0) {
    g_integer_segments_valid =
        g_integer_segments_valid &&
        static_cast<const void*>(iwork) == static_cast<const void*>(ipiv + *n);
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_sgtsvx_(fact, trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b,
                 ldb, x, ldx, rcond, ferr, berr, work, iwork, info, fact_length,
                 trans_length);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
  Diagnostic(mode, rcond);
  if (mode == Mode::kWarningNegativeError && *nrhs > 0) {
    *info = *n + 1;
    ferr[0] = -1;
  }
}

decltype(LAPACK_dgttrf) __real_dgttrf_;
void __wrap_dgttrf_(const lapack_int* n, double* dl, double* d, double* du,
                    double* du2, lapack_int* ipiv, lapack_int* info) {
  const Mode mode = Enter(Operation::kFactor);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }

  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_dgttrf_(n, dl, d, du, du2, ipiv, info);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
}

decltype(LAPACK_dgttrs_base) __real_dgttrs_;
void __wrap_dgttrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const double* dl, const double* d,
                    const double* du, const double* du2, const lapack_int* ipiv,
                    double* b, const lapack_int* ldb, lapack_int* info,
                    FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kSolve);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid =
      g_character_lengths_valid && TransposeLength(trans, trans_length);
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_dgttrs_(trans, n, nrhs, dl, d, du, du2, ipiv, b, ldb, info,
                 trans_length);
}

decltype(LAPACK_dgtsv) __real_dgtsv_;
void __wrap_dgtsv_(const lapack_int* n, const lapack_int* nrhs, double* dl,
                   double* d, double* du, double* b, const lapack_int* ldb,
                   lapack_int* info) {
  const Mode mode = Enter(Operation::kDriver);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }

  if (*n > 0 && *nrhs == 0) {
    g_zero_rhs_observed =
        std::all_of(b, b + *n, [](const double value) { return value == 0; });
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_dgtsv_(n, nrhs, dl, d, du, b, ldb, info);
}

decltype(LAPACK_dgtcon_base) __real_dgtcon_;
void __wrap_dgtcon_(const char* norm, const lapack_int* n, const double* dl,
                    const double* d, const double* du, const double* du2,
                    const lapack_int* ipiv, const double* anorm, double* rcond,
                    double* work, lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN norm_length) {
  const Mode mode = Enter(Operation::kCondition);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && norm_length == 1;
  if (*n > 0) {
    g_integer_segments_valid =
        g_integer_segments_valid &&
        static_cast<const void*>(iwork) == static_cast<const void*>(ipiv + *n);
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_dgtcon_(norm, n, dl, d, du, du2, ipiv, anorm, rcond, work, iwork, info,
                 norm_length);
  Diagnostic(mode, rcond);
}

decltype(LAPACK_dgtrfs_base) __real_dgtrfs_;
void __wrap_dgtrfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const double* dl, const double* d,
                    const double* du, const double* dlf, const double* df,
                    const double* duf, const double* du2,
                    const lapack_int* ipiv, const double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* ferr, double* berr, double* work, lapack_int* iwork,
                    lapack_int* info, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kRefinement);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (*n > 0) {
    g_integer_segments_valid =
        g_integer_segments_valid &&
        static_cast<const void*>(iwork) == static_cast<const void*>(ipiv + *n);
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_dgtrfs_(trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b, ldb, x,
                 ldx, ferr, berr, work, iwork, info, trans_length);
  if (*nrhs > 0) {
    Diagnostic(mode, ferr);
  }
}

decltype(LAPACK_dgtsvx_base) __real_dgtsvx_;
void __wrap_dgtsvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const double* dl, const double* d,
                    const double* du, double* dlf, double* df, double* duf,
                    double* du2, lapack_int* ipiv, const double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* rcond, double* ferr, double* berr, double* work,
                    lapack_int* iwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kExpert);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }
  g_character_lengths_valid = g_character_lengths_valid && fact_length == 1;
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (*n > 0) {
    g_integer_segments_valid =
        g_integer_segments_valid &&
        static_cast<const void*>(iwork) == static_cast<const void*>(ipiv + *n);
  }
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_dgtsvx_(fact, trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b,
                 ldb, x, ldx, rcond, ferr, berr, work, iwork, info, fact_length,
                 trans_length);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
  Diagnostic(mode, rcond);
  if (mode == Mode::kWarningNegativeError && *nrhs > 0) {
    *info = *n + 1;
    ferr[0] = -1;
  }
}

decltype(LAPACK_cgttrf) __real_cgttrf_;
void __wrap_cgttrf_(const lapack_int* n, std::complex<float>* dl,
                    std::complex<float>* d, std::complex<float>* du,
                    std::complex<float>* du2, lapack_int* ipiv,
                    lapack_int* info) {
  const Mode mode = Enter(Operation::kFactor);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }

  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_cgttrf_(n, dl, d, du, du2, ipiv, info);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
}

decltype(LAPACK_cgttrs_base) __real_cgttrs_;
void __wrap_cgttrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* dl,
                    const std::complex<float>* d, const std::complex<float>* du,
                    const std::complex<float>* du2, const lapack_int* ipiv,
                    std::complex<float>* b, const lapack_int* ldb,
                    lapack_int* info, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kSolve);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid =
      g_character_lengths_valid && TransposeLength(trans, trans_length);
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_cgttrs_(trans, n, nrhs, dl, d, du, du2, ipiv, b, ldb, info,
                 trans_length);
}

decltype(LAPACK_cgtsv) __real_cgtsv_;
void __wrap_cgtsv_(const lapack_int* n, const lapack_int* nrhs,
                   std::complex<float>* dl, std::complex<float>* d,
                   std::complex<float>* du, std::complex<float>* b,
                   const lapack_int* ldb, lapack_int* info) {
  const Mode mode = Enter(Operation::kDriver);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }

  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_cgtsv_(n, nrhs, dl, d, du, b, ldb, info);
}

decltype(LAPACK_cgtcon_base) __real_cgtcon_;
void __wrap_cgtcon_(const char* norm, const lapack_int* n,
                    const std::complex<float>* dl, const std::complex<float>* d,
                    const std::complex<float>* du,
                    const std::complex<float>* du2, const lapack_int* ipiv,
                    const float* anorm, float* rcond, std::complex<float>* work,
                    lapack_int* info, FORTRAN_STRLEN norm_length) {
  const Mode mode = Enter(Operation::kCondition);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && norm_length == 1;
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_cgtcon_(norm, n, dl, d, du, du2, ipiv, anorm, rcond, work, info,
                 norm_length);
  Diagnostic(mode, rcond);
}

decltype(LAPACK_cgtrfs_base) __real_cgtrfs_;
void __wrap_cgtrfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* dl,
                    const std::complex<float>* d, const std::complex<float>* du,
                    const std::complex<float>* dlf,
                    const std::complex<float>* df,
                    const std::complex<float>* duf,
                    const std::complex<float>* du2, const lapack_int* ipiv,
                    const std::complex<float>* b, const lapack_int* ldb,
                    std::complex<float>* x, const lapack_int* ldx, float* ferr,
                    float* berr, std::complex<float>* work, float* rwork,
                    lapack_int* info, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kRefinement);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_cgtrfs_(trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b, ldb, x,
                 ldx, ferr, berr, work, rwork, info, trans_length);
  if (*nrhs > 0) {
    Diagnostic(mode, ferr);
  }
}

decltype(LAPACK_cgtsvx_base) __real_cgtsvx_;
void __wrap_cgtsvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* dl,
                    const std::complex<float>* d, const std::complex<float>* du,
                    std::complex<float>* dlf, std::complex<float>* df,
                    std::complex<float>* duf, std::complex<float>* du2,
                    lapack_int* ipiv, const std::complex<float>* b,
                    const lapack_int* ldb, std::complex<float>* x,
                    const lapack_int* ldx, float* rcond, float* ferr,
                    float* berr, std::complex<float>* work, float* rwork,
                    lapack_int* info, FORTRAN_STRLEN fact_length,
                    FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kExpert);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }
  g_character_lengths_valid = g_character_lengths_valid && fact_length == 1;
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_cgtsvx_(fact, trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b,
                 ldb, x, ldx, rcond, ferr, berr, work, rwork, info, fact_length,
                 trans_length);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
  Diagnostic(mode, rcond);
  if (mode == Mode::kWarningNegativeError && *nrhs > 0) {
    *info = *n + 1;
    ferr[0] = -1;
  }
}

decltype(LAPACK_zgttrf) __real_zgttrf_;
void __wrap_zgttrf_(const lapack_int* n, std::complex<double>* dl,
                    std::complex<double>* d, std::complex<double>* du,
                    std::complex<double>* du2, lapack_int* ipiv,
                    lapack_int* info) {
  const Mode mode = Enter(Operation::kFactor);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }

  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_zgttrf_(n, dl, d, du, du2, ipiv, info);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
}

decltype(LAPACK_zgttrs_base) __real_zgttrs_;
void __wrap_zgttrs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* dl,
                    const std::complex<double>* d,
                    const std::complex<double>* du,
                    const std::complex<double>* du2, const lapack_int* ipiv,
                    std::complex<double>* b, const lapack_int* ldb,
                    lapack_int* info, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kSolve);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid =
      g_character_lengths_valid && TransposeLength(trans, trans_length);
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_zgttrs_(trans, n, nrhs, dl, d, du, du2, ipiv, b, ldb, info,
                 trans_length);
}

decltype(LAPACK_zgtsv) __real_zgtsv_;
void __wrap_zgtsv_(const lapack_int* n, const lapack_int* nrhs,
                   std::complex<double>* dl, std::complex<double>* d,
                   std::complex<double>* du, std::complex<double>* b,
                   const lapack_int* ldb, lapack_int* info) {
  const Mode mode = Enter(Operation::kDriver);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }

  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_zgtsv_(n, nrhs, dl, d, du, b, ldb, info);
}

decltype(LAPACK_zgtcon_base) __real_zgtcon_;
void __wrap_zgtcon_(const char* norm, const lapack_int* n,
                    const std::complex<double>* dl,
                    const std::complex<double>* d,
                    const std::complex<double>* du,
                    const std::complex<double>* du2, const lapack_int* ipiv,
                    const double* anorm, double* rcond,
                    std::complex<double>* work, lapack_int* info,
                    FORTRAN_STRLEN norm_length) {
  const Mode mode = Enter(Operation::kCondition);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && norm_length == 1;
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_zgtcon_(norm, n, dl, d, du, du2, ipiv, anorm, rcond, work, info,
                 norm_length);
  Diagnostic(mode, rcond);
}

decltype(LAPACK_zgtrfs_base) __real_zgtrfs_;
void __wrap_zgtrfs_(
    const char* trans, const lapack_int* n, const lapack_int* nrhs,
    const std::complex<double>* dl, const std::complex<double>* d,
    const std::complex<double>* du, const std::complex<double>* dlf,
    const std::complex<double>* df, const std::complex<double>* duf,
    const std::complex<double>* du2, const lapack_int* ipiv,
    const std::complex<double>* b, const lapack_int* ldb,
    std::complex<double>* x, const lapack_int* ldx, double* ferr, double* berr,
    std::complex<double>* work, double* rwork, lapack_int* info,
    FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kRefinement);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_zgtrfs_(trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b, ldb, x,
                 ldx, ferr, berr, work, rwork, info, trans_length);
  if (*nrhs > 0) {
    Diagnostic(mode, ferr);
  }
}

decltype(LAPACK_zgtsvx_base) __real_zgtsvx_;
void __wrap_zgtsvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* dl,
                    const std::complex<double>* d,
                    const std::complex<double>* du, std::complex<double>* dlf,
                    std::complex<double>* df, std::complex<double>* duf,
                    std::complex<double>* du2, lapack_int* ipiv,
                    const std::complex<double>* b, const lapack_int* ldb,
                    std::complex<double>* x, const lapack_int* ldx,
                    double* rcond, double* ferr, double* berr,
                    std::complex<double>* work, double* rwork, lapack_int* info,
                    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length) {
  const Mode mode = Enter(Operation::kExpert);
  lapack_int withheld_info = 0;
  if (mode == Mode::kUnwrittenInfo) {
    info = &withheld_info;
  }
  if (PivotOutput(mode, *n, ipiv, info)) {
    return;
  }
  g_character_lengths_valid = g_character_lengths_valid && fact_length == 1;
  g_character_lengths_valid = g_character_lengths_valid && trans_length == 1;
  if (Preflight(mode, *n, info)) {
    return;
  }
  __real_zgtsvx_(fact, trans, n, nrhs, dl, d, du, dlf, df, duf, du2, ipiv, b,
                 ldb, x, ldx, rcond, ferr, berr, work, rwork, info, fact_length,
                 trans_length);
  if (mode == Mode::kInvalidPivot && *n > 0) {
    ipiv[0] = 0;
  }
  Diagnostic(mode, rcond);
  if (mode == Mode::kWarningNegativeError && *nrhs > 0) {
    *info = *n + 1;
    ferr[0] = -1;
  }
}

}  // extern "C"

// Compare the complete actual ABI, including hidden CHARACTER lengths.
static_assert(
    std::is_same_v<decltype(__wrap_sgttrf_), decltype(LAPACK_sgttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgttrs_), decltype(LAPACK_sgttrs_base)>);
static_assert(std::is_same_v<decltype(__wrap_sgtsv_), decltype(LAPACK_sgtsv)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgtcon_), decltype(LAPACK_sgtcon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgtrfs_), decltype(LAPACK_sgtrfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgtsvx_), decltype(LAPACK_sgtsvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgttrf_), decltype(LAPACK_dgttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgttrs_), decltype(LAPACK_dgttrs_base)>);
static_assert(std::is_same_v<decltype(__wrap_dgtsv_), decltype(LAPACK_dgtsv)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgtcon_), decltype(LAPACK_dgtcon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgtrfs_), decltype(LAPACK_dgtrfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgtsvx_), decltype(LAPACK_dgtsvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgttrf_), decltype(LAPACK_cgttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgttrs_), decltype(LAPACK_cgttrs_base)>);
static_assert(std::is_same_v<decltype(__wrap_cgtsv_), decltype(LAPACK_cgtsv)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgtcon_), decltype(LAPACK_cgtcon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgtrfs_), decltype(LAPACK_cgtrfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgtsvx_), decltype(LAPACK_cgtsvx_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgttrf_), decltype(LAPACK_zgttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgttrs_), decltype(LAPACK_zgttrs_base)>);
static_assert(std::is_same_v<decltype(__wrap_zgtsv_), decltype(LAPACK_zgtsv)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgtcon_), decltype(LAPACK_zgtcon_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgtrfs_), decltype(LAPACK_zgtrfs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgtsvx_), decltype(LAPACK_zgtsvx_base)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_tridiagonal_fault
