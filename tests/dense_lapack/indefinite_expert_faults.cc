#include "indefinite_expert_faults.h"

#include <complex>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"

namespace {
using asc_indefinite_expert_test::Fault;
using asc_indefinite_expert_test::Route;
Route g_route = Route::kCon;
Fault g_fault = Fault::kNone;
std::size_t g_calls = 0;

bool Negative(Route route, lapack_int* info) {
  if (g_route == route) {
    ++g_calls;
  }
  if (g_route == route && g_fault == Fault::kNegativeInfo) {
    *info = -4;
    return true;
  }
  return false;
}

void Info(Route route, lapack_int n, lapack_int* info) {
  if (g_route == route && g_fault == Fault::kExcessInfo) {
    *info = n + 2;
  }
  // Only FACT=N can return a newly discovered factorization failure. The
  // FACT=F adapter must reject this positive INFO as a provider defect.
  if (g_route == route && g_fault == Fault::kUnexpectedFactorInfo) {
    *info = n;
  }
}

template <typename Real>
void Diagnostic(Route route, Real* value) {
  if (g_route == route && g_fault == Fault::kNegativeDiagnostic) {
    *value = -1;
  }
  if (g_route == route && g_fault == Fault::kNanDiagnostic) {
    *value = std::numeric_limits<Real>::quiet_NaN();
  }
}

template <typename T>
void FactorOutput(Route route, T* work, lapack_int* pivots) {
  if (g_route == route && g_fault == Fault::kWorkNan) {
    *work = std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
  }
  if (g_route == route && g_fault == Fault::kPivotMinimum) {
    *pivots = std::numeric_limits<lapack_int>::min();
  }
}
}  // namespace

namespace asc_indefinite_expert_test {
void SetFault(Route route, Fault fault) {
  g_route = route;
  g_fault = fault;
  g_calls = 0;
}
std::size_t ObservedCalls() { return g_calls; }
}  // namespace asc_indefinite_expert_test

// These reserved spellings are required by GNU ld --wrap; exact function
// types are checked against the pinned typed declarations in both ABIs.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(ssycon_) __real_ssycon_;
void __wrap_ssycon_(const char* uplo, const lapack_int* n, const float* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    const float* anorm, float* rcond, float* work,
                    lapack_int* extra, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kCon, info)) {
    return;
  }
  __real_ssycon_(uplo, n, a, lda, pivots, anorm, rcond, work, extra, info,
                 uplo_length);
  Info(Route::kCon, *n, info);
  Diagnostic(Route::kCon, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_ssycon_), decltype(ssycon_)>);

decltype(ssyrfs_) __real_ssyrfs_;
void __wrap_ssyrfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const lapack_int* lda, const float* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const float* b, const lapack_int* ldb, float* x,
                    const lapack_int* ldx, float* ferr, float* berr,
                    float* work, lapack_int* extra, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kRfs, info)) {
    return;
  }
  __real_ssyrfs_(uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info, uplo_length);
  Info(Route::kRfs, *n, info);
  Diagnostic(Route::kRfs, ferr);
}
static_assert(std::is_same_v<decltype(__wrap_ssyrfs_), decltype(ssyrfs_)>);

decltype(ssysv_) __real_ssysv_;
void __wrap_ssysv_(const char* uplo, const lapack_int* n,
                   const lapack_int* nrhs, float* a, const lapack_int* lda,
                   lapack_int* pivots, float* b, const lapack_int* ldb,
                   float* work, const lapack_int* lwork, lapack_int* info,
                   std::size_t uplo_length) {
  if (Negative(Route::kSv, info)) {
    return;
  }
  __real_ssysv_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork, info,
                uplo_length);
  Info(Route::kSv, *n, info);
  FactorOutput(Route::kSv, work, pivots);
}
static_assert(std::is_same_v<decltype(__wrap_ssysv_), decltype(ssysv_)>);

decltype(ssysvx_) __real_ssysvx_;
void __wrap_ssysvx_(const char* fact, const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const lapack_int* lda, float* af, const lapack_int* ldaf,
                    lapack_int* pivots, const float* b, const lapack_int* ldb,
                    float* x, const lapack_int* ldx, float* rcond, float* ferr,
                    float* berr, float* work, const lapack_int* lwork,
                    lapack_int* extra, lapack_int* info,
                    std::size_t fact_length, std::size_t uplo_length) {
  if (Negative(Route::kVx, info)) {
    return;
  }
  __real_ssysvx_(fact, uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                 rcond, ferr, berr, work, lwork, extra, info, fact_length,
                 uplo_length);
  Info(Route::kVx, *n, info);
  FactorOutput(Route::kVx, work, pivots);
  Diagnostic(Route::kVx, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_ssysvx_), decltype(ssysvx_)>);

decltype(dsycon_) __real_dsycon_;
void __wrap_dsycon_(const char* uplo, const lapack_int* n, const double* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    const double* anorm, double* rcond, double* work,
                    lapack_int* extra, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kCon, info)) {
    return;
  }
  __real_dsycon_(uplo, n, a, lda, pivots, anorm, rcond, work, extra, info,
                 uplo_length);
  Info(Route::kCon, *n, info);
  Diagnostic(Route::kCon, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_dsycon_), decltype(dsycon_)>);

decltype(dsyrfs_) __real_dsyrfs_;
void __wrap_dsyrfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const double* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* ferr, double* berr,
                    double* work, lapack_int* extra, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kRfs, info)) {
    return;
  }
  __real_dsyrfs_(uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info, uplo_length);
  Info(Route::kRfs, *n, info);
  Diagnostic(Route::kRfs, ferr);
}
static_assert(std::is_same_v<decltype(__wrap_dsyrfs_), decltype(dsyrfs_)>);

decltype(dsysv_) __real_dsysv_;
void __wrap_dsysv_(const char* uplo, const lapack_int* n,
                   const lapack_int* nrhs, double* a, const lapack_int* lda,
                   lapack_int* pivots, double* b, const lapack_int* ldb,
                   double* work, const lapack_int* lwork, lapack_int* info,
                   std::size_t uplo_length) {
  if (Negative(Route::kSv, info)) {
    return;
  }
  __real_dsysv_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork, info,
                uplo_length);
  Info(Route::kSv, *n, info);
  FactorOutput(Route::kSv, work, pivots);
}
static_assert(std::is_same_v<decltype(__wrap_dsysv_), decltype(dsysv_)>);

decltype(dsysvx_) __real_dsysvx_;
void __wrap_dsysvx_(const char* fact, const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, double* af, const lapack_int* ldaf,
                    lapack_int* pivots, const double* b, const lapack_int* ldb,
                    double* x, const lapack_int* ldx, double* rcond,
                    double* ferr, double* berr, double* work,
                    const lapack_int* lwork, lapack_int* extra,
                    lapack_int* info, std::size_t fact_length,
                    std::size_t uplo_length) {
  if (Negative(Route::kVx, info)) {
    return;
  }
  __real_dsysvx_(fact, uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                 rcond, ferr, berr, work, lwork, extra, info, fact_length,
                 uplo_length);
  Info(Route::kVx, *n, info);
  FactorOutput(Route::kVx, work, pivots);
  Diagnostic(Route::kVx, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_dsysvx_), decltype(dsysvx_)>);

decltype(csycon_) __real_csycon_;
void __wrap_csycon_(const char* uplo, const lapack_int* n,
                    const std::complex<float>* a, const lapack_int* lda,
                    const lapack_int* pivots, const float* anorm, float* rcond,
                    std::complex<float>* work, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kCon, info)) {
    return;
  }
  __real_csycon_(uplo, n, a, lda, pivots, anorm, rcond, work, info,
                 uplo_length);
  Info(Route::kCon, *n, info);
  Diagnostic(Route::kCon, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_csycon_), decltype(csycon_)>);

decltype(csyrfs_) __real_csyrfs_;
void __wrap_csyrfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const std::complex<float>* b, const lapack_int* ldb,
                    std::complex<float>* x, const lapack_int* ldx, float* ferr,
                    float* berr, std::complex<float>* work, float* extra,
                    lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kRfs, info)) {
    return;
  }
  __real_csyrfs_(uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info, uplo_length);
  Info(Route::kRfs, *n, info);
  Diagnostic(Route::kRfs, ferr);
}
static_assert(std::is_same_v<decltype(__wrap_csyrfs_), decltype(csyrfs_)>);

decltype(csysv_) __real_csysv_;
void __wrap_csysv_(const char* uplo, const lapack_int* n,
                   const lapack_int* nrhs, std::complex<float>* a,
                   const lapack_int* lda, lapack_int* pivots,
                   std::complex<float>* b, const lapack_int* ldb,
                   std::complex<float>* work, const lapack_int* lwork,
                   lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kSv, info)) {
    return;
  }
  __real_csysv_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork, info,
                uplo_length);
  Info(Route::kSv, *n, info);
  FactorOutput(Route::kSv, work, pivots);
}
static_assert(std::is_same_v<decltype(__wrap_csysv_), decltype(csysv_)>);

decltype(csysvx_) __real_csysvx_;
void __wrap_csysvx_(const char* fact, const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* a,
                    const lapack_int* lda, std::complex<float>* af,
                    const lapack_int* ldaf, lapack_int* pivots,
                    const std::complex<float>* b, const lapack_int* ldb,
                    std::complex<float>* x, const lapack_int* ldx, float* rcond,
                    float* ferr, float* berr, std::complex<float>* work,
                    const lapack_int* lwork, float* extra, lapack_int* info,
                    std::size_t fact_length, std::size_t uplo_length) {
  if (Negative(Route::kVx, info)) {
    return;
  }
  __real_csysvx_(fact, uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                 rcond, ferr, berr, work, lwork, extra, info, fact_length,
                 uplo_length);
  Info(Route::kVx, *n, info);
  FactorOutput(Route::kVx, work, pivots);
  Diagnostic(Route::kVx, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_csysvx_), decltype(csysvx_)>);

decltype(checon_) __real_checon_;
void __wrap_checon_(const char* uplo, const lapack_int* n,
                    const std::complex<float>* a, const lapack_int* lda,
                    const lapack_int* pivots, const float* anorm, float* rcond,
                    std::complex<float>* work, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kCon, info)) {
    return;
  }
  __real_checon_(uplo, n, a, lda, pivots, anorm, rcond, work, info,
                 uplo_length);
  Info(Route::kCon, *n, info);
  Diagnostic(Route::kCon, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_checon_), decltype(checon_)>);

decltype(cherfs_) __real_cherfs_;
void __wrap_cherfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* a,
                    const lapack_int* lda, const std::complex<float>* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const std::complex<float>* b, const lapack_int* ldb,
                    std::complex<float>* x, const lapack_int* ldx, float* ferr,
                    float* berr, std::complex<float>* work, float* extra,
                    lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kRfs, info)) {
    return;
  }
  __real_cherfs_(uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info, uplo_length);
  Info(Route::kRfs, *n, info);
  Diagnostic(Route::kRfs, ferr);
}
static_assert(std::is_same_v<decltype(__wrap_cherfs_), decltype(cherfs_)>);

decltype(chesv_) __real_chesv_;
void __wrap_chesv_(const char* uplo, const lapack_int* n,
                   const lapack_int* nrhs, std::complex<float>* a,
                   const lapack_int* lda, lapack_int* pivots,
                   std::complex<float>* b, const lapack_int* ldb,
                   std::complex<float>* work, const lapack_int* lwork,
                   lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kSv, info)) {
    return;
  }
  __real_chesv_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork, info,
                uplo_length);
  Info(Route::kSv, *n, info);
  FactorOutput(Route::kSv, work, pivots);
}
static_assert(std::is_same_v<decltype(__wrap_chesv_), decltype(chesv_)>);

decltype(chesvx_) __real_chesvx_;
void __wrap_chesvx_(const char* fact, const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<float>* a,
                    const lapack_int* lda, std::complex<float>* af,
                    const lapack_int* ldaf, lapack_int* pivots,
                    const std::complex<float>* b, const lapack_int* ldb,
                    std::complex<float>* x, const lapack_int* ldx, float* rcond,
                    float* ferr, float* berr, std::complex<float>* work,
                    const lapack_int* lwork, float* extra, lapack_int* info,
                    std::size_t fact_length, std::size_t uplo_length) {
  if (Negative(Route::kVx, info)) {
    return;
  }
  __real_chesvx_(fact, uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                 rcond, ferr, berr, work, lwork, extra, info, fact_length,
                 uplo_length);
  Info(Route::kVx, *n, info);
  FactorOutput(Route::kVx, work, pivots);
  Diagnostic(Route::kVx, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_chesvx_), decltype(chesvx_)>);

decltype(zsycon_) __real_zsycon_;
void __wrap_zsycon_(const char* uplo, const lapack_int* n,
                    const std::complex<double>* a, const lapack_int* lda,
                    const lapack_int* pivots, const double* anorm,
                    double* rcond, std::complex<double>* work, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kCon, info)) {
    return;
  }
  __real_zsycon_(uplo, n, a, lda, pivots, anorm, rcond, work, info,
                 uplo_length);
  Info(Route::kCon, *n, info);
  Diagnostic(Route::kCon, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_zsycon_), decltype(zsycon_)>);

decltype(zsyrfs_) __real_zsyrfs_;
void __wrap_zsyrfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const std::complex<double>* b, const lapack_int* ldb,
                    std::complex<double>* x, const lapack_int* ldx,
                    double* ferr, double* berr, std::complex<double>* work,
                    double* extra, lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kRfs, info)) {
    return;
  }
  __real_zsyrfs_(uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info, uplo_length);
  Info(Route::kRfs, *n, info);
  Diagnostic(Route::kRfs, ferr);
}
static_assert(std::is_same_v<decltype(__wrap_zsyrfs_), decltype(zsyrfs_)>);

decltype(zsysv_) __real_zsysv_;
void __wrap_zsysv_(const char* uplo, const lapack_int* n,
                   const lapack_int* nrhs, std::complex<double>* a,
                   const lapack_int* lda, lapack_int* pivots,
                   std::complex<double>* b, const lapack_int* ldb,
                   std::complex<double>* work, const lapack_int* lwork,
                   lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kSv, info)) {
    return;
  }
  __real_zsysv_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork, info,
                uplo_length);
  Info(Route::kSv, *n, info);
  FactorOutput(Route::kSv, work, pivots);
}
static_assert(std::is_same_v<decltype(__wrap_zsysv_), decltype(zsysv_)>);

decltype(zsysvx_) __real_zsysvx_;
void __wrap_zsysvx_(const char* fact, const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* a,
                    const lapack_int* lda, std::complex<double>* af,
                    const lapack_int* ldaf, lapack_int* pivots,
                    const std::complex<double>* b, const lapack_int* ldb,
                    std::complex<double>* x, const lapack_int* ldx,
                    double* rcond, double* ferr, double* berr,
                    std::complex<double>* work, const lapack_int* lwork,
                    double* extra, lapack_int* info, std::size_t fact_length,
                    std::size_t uplo_length) {
  if (Negative(Route::kVx, info)) {
    return;
  }
  __real_zsysvx_(fact, uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                 rcond, ferr, berr, work, lwork, extra, info, fact_length,
                 uplo_length);
  Info(Route::kVx, *n, info);
  FactorOutput(Route::kVx, work, pivots);
  Diagnostic(Route::kVx, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_zsysvx_), decltype(zsysvx_)>);

decltype(zhecon_) __real_zhecon_;
void __wrap_zhecon_(const char* uplo, const lapack_int* n,
                    const std::complex<double>* a, const lapack_int* lda,
                    const lapack_int* pivots, const double* anorm,
                    double* rcond, std::complex<double>* work, lapack_int* info,
                    std::size_t uplo_length) {
  if (Negative(Route::kCon, info)) {
    return;
  }
  __real_zhecon_(uplo, n, a, lda, pivots, anorm, rcond, work, info,
                 uplo_length);
  Info(Route::kCon, *n, info);
  Diagnostic(Route::kCon, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_zhecon_), decltype(zhecon_)>);

decltype(zherfs_) __real_zherfs_;
void __wrap_zherfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* a,
                    const lapack_int* lda, const std::complex<double>* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const std::complex<double>* b, const lapack_int* ldb,
                    std::complex<double>* x, const lapack_int* ldx,
                    double* ferr, double* berr, std::complex<double>* work,
                    double* extra, lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kRfs, info)) {
    return;
  }
  __real_zherfs_(uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info, uplo_length);
  Info(Route::kRfs, *n, info);
  Diagnostic(Route::kRfs, ferr);
}
static_assert(std::is_same_v<decltype(__wrap_zherfs_), decltype(zherfs_)>);

decltype(zhesv_) __real_zhesv_;
void __wrap_zhesv_(const char* uplo, const lapack_int* n,
                   const lapack_int* nrhs, std::complex<double>* a,
                   const lapack_int* lda, lapack_int* pivots,
                   std::complex<double>* b, const lapack_int* ldb,
                   std::complex<double>* work, const lapack_int* lwork,
                   lapack_int* info, std::size_t uplo_length) {
  if (Negative(Route::kSv, info)) {
    return;
  }
  __real_zhesv_(uplo, n, nrhs, a, lda, pivots, b, ldb, work, lwork, info,
                uplo_length);
  Info(Route::kSv, *n, info);
  FactorOutput(Route::kSv, work, pivots);
}
static_assert(std::is_same_v<decltype(__wrap_zhesv_), decltype(zhesv_)>);

decltype(zhesvx_) __real_zhesvx_;
void __wrap_zhesvx_(const char* fact, const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const std::complex<double>* a,
                    const lapack_int* lda, std::complex<double>* af,
                    const lapack_int* ldaf, lapack_int* pivots,
                    const std::complex<double>* b, const lapack_int* ldb,
                    std::complex<double>* x, const lapack_int* ldx,
                    double* rcond, double* ferr, double* berr,
                    std::complex<double>* work, const lapack_int* lwork,
                    double* extra, lapack_int* info, std::size_t fact_length,
                    std::size_t uplo_length) {
  if (Negative(Route::kVx, info)) {
    return;
  }
  __real_zhesvx_(fact, uplo, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx,
                 rcond, ferr, berr, work, lwork, extra, info, fact_length,
                 uplo_length);
  Info(Route::kVx, *n, info);
  FactorOutput(Route::kVx, work, pivots);
  Diagnostic(Route::kVx, rcond);
}
static_assert(std::is_same_v<decltype(__wrap_zhesvx_), decltype(zhesvx_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
