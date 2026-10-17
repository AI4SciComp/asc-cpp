#include "positive_tridiagonal_expert_entry.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#include "asc/dense/blas.h"
#include "internal_tridiagonal.h"
namespace asc::internal_ptsvx_test {
namespace {
thread_local Fault active_fault = Fault::kNone;
thread_local std::size_t entries = 0;
thread_local void (*entry_hook)() = nullptr;
}  // namespace
void SetFault(Fault fault) {
  active_fault = fault;
  entries = 0;
}
void SetEntryHook(void (*hook)()) { entry_hook = hook; }
std::size_t EntryCount() { return entries; }
void Enter(const char* fact, const lapack_int* n, const lapack_int* nrhs,
           const lapack_int* ldb, const lapack_int* ldx) {
  if ((*fact != 'N' && *fact != 'F') || *n != 3 || (*nrhs != 0 && *nrhs != 2) ||
      *ldb < *n || *ldx != *n) {
    std::abort();
  }
  ++entries;
  if (entry_hook != nullptr) {
    entry_hook();
  }
}
template <typename R>
bool InfoFault(lapack_int n, R* condition, lapack_int* info) {
  switch (active_fault) {
    case Fault::kNoInfo:
      return true;
    case Fault::kPartialInfo: {
      const std::int32_t zero = 0;
      std::memcpy(info, &zero, sizeof(zero));
      return true;
    }
    case Fault::kNegativeInfo:
      *info = -2;
      return true;
    case Fault::kPositiveInfo:
      *info = 2;
      *condition = 0;
      return true;
    case Fault::kBeyondInfo:
      *info = n + 2;
      return true;
    case Fault::kPivotCondition:
      *info = 2;
      return true;
    case Fault::kConditionWarning:
      *info = n + 1;
      return true;
    default:
      return false;
  }
}
template <typename T>
void Inject(const char* fact, const lapack_int* n, const lapack_int* nrhs,
            DenseBlasRealType<T>* df, T* ef, T* x, const lapack_int* ldx,
            DenseBlasRealType<T>* condition, DenseBlasRealType<T>* ferr,
            DenseBlasRealType<T>* berr, lapack_int* info) {
  using R = DenseBlasRealType<T>;
  if (*fact == 'N') {
    for (lapack_int i = 0; i < *n; ++i) {
      df[i] = R{2};
      if (i + 1 < *n) {
        ef[i] = T{};
      }
    }
  }
  if (active_fault != Fault::kNoCondition) {
    *condition = R{0.5};
  }
  for (lapack_int j = 0; j < *nrhs; ++j) {
    for (lapack_int i = 0; i < *n; ++i) {
      x[i + j * *ldx] = T{2};
    }
    if (active_fault != Fault::kNoFerr) {
      ferr[j] = R{0.125};
    }
    if (active_fault != Fault::kNoBerr) {
      berr[j] = R{0.0625};
    }
  }
  if (InfoFault(*n, condition, info)) {
    return;
  }
  switch (active_fault) {
    case Fault::kNegativeCondition:
      *condition = -2;
      break;
    case Fault::kNegativeFerr:
      if (*nrhs > 0) {
        ferr[*nrhs - 1] = -2;
      }
      break;
    case Fault::kNegativeBerr:
      if (*nrhs > 0) {
        berr[*nrhs - 1] = -2;
      }
      break;
    case Fault::kNanCondition:
      *condition = std::numeric_limits<R>::quiet_NaN();
      break;
    case Fault::kInfiniteCondition:
      *condition = std::numeric_limits<R>::infinity();
      break;
    case Fault::kNanSolution:
      if (*nrhs > 0) {
        x[(*nrhs - 1) * *ldx + *n - 1] = T{std::numeric_limits<R>::quiet_NaN()};
      }
      break;
    case Fault::kInfiniteSolution:
      if (*nrhs > 0) {
        x[(*nrhs - 1) * *ldx + *n - 1] = T{std::numeric_limits<R>::infinity()};
      }
      break;
    case Fault::kNanFerr:
      if (*nrhs > 0) {
        ferr[*nrhs - 1] = std::numeric_limits<R>::quiet_NaN();
      }
      break;
    case Fault::kInfiniteBerr:
      if (*nrhs > 0) {
        berr[*nrhs - 1] = std::numeric_limits<R>::infinity();
      }
      break;
    case Fault::kNanDiagonal:
      if (*fact == 'N') {
        df[0] = std::numeric_limits<R>::quiet_NaN();
      }
      break;
    default:
      break;
  }
  *info = 0;
}
}  // namespace asc::internal_ptsvx_test
// GNU ld requires these reserved spellings for test-only foreign interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_sptsvx_(const char* fact, const lapack_int* n,
                               const lapack_int* nrhs, const float* d,
                               const float* e, float* df, float* ef,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* condition,
                               float* ferr, float* berr, float* work,
                               lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_sptsvx_(const char* fact, const lapack_int* n,
                               const lapack_int* nrhs, const float* d,
                               const float* e, float* df, float* ef,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* condition,
                               float* ferr, float* berr, float* work,
                               lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
) {
  namespace probe = asc::internal_ptsvx_test;
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  probe::Enter(fact, n, nrhs, ldb, ldx);
  if (probe::active_fault == probe::Fault::kNone) {
    __real_sptsvx_(fact, n, nrhs, d, e, df, ef, b, ldb, x, ldx, condition, ferr,
                   berr, work, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                   ,
                   length
#endif
    );
  } else {
    probe::Inject(fact, n, nrhs, df, ef, x, ldx, condition, ferr, berr, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_sptsvx_), decltype(&LAPACK_sptsvx_base)>);
extern "C" void __real_dptsvx_(const char* fact, const lapack_int* n,
                               const lapack_int* nrhs, const double* d,
                               const double* e, double* df, double* ef,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx,
                               double* condition, double* ferr, double* berr,
                               double* work, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_dptsvx_(const char* fact, const lapack_int* n,
                               const lapack_int* nrhs, const double* d,
                               const double* e, double* df, double* ef,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx,
                               double* condition, double* ferr, double* berr,
                               double* work, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
) {
  namespace probe = asc::internal_ptsvx_test;
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  probe::Enter(fact, n, nrhs, ldb, ldx);
  if (probe::active_fault == probe::Fault::kNone) {
    __real_dptsvx_(fact, n, nrhs, d, e, df, ef, b, ldb, x, ldx, condition, ferr,
                   berr, work, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                   ,
                   length
#endif
    );
  } else {
    probe::Inject(fact, n, nrhs, df, ef, x, ldx, condition, ferr, berr, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_dptsvx_), decltype(&LAPACK_dptsvx_base)>);
extern "C" void __real_cptsvx_(
    const char* fact, const lapack_int* n, const lapack_int* nrhs,
    const float* d, const std::complex<float>* e, float* df,
    std::complex<float>* ef, const std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* x, const lapack_int* ldx,
    float* condition, float* ferr, float* berr, std::complex<float>* work,
    float* real_work, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_cptsvx_(
    const char* fact, const lapack_int* n, const lapack_int* nrhs,
    const float* d, const std::complex<float>* e, float* df,
    std::complex<float>* ef, const std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* x, const lapack_int* ldx,
    float* condition, float* ferr, float* berr, std::complex<float>* work,
    float* real_work, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN length
#endif
) {
  namespace probe = asc::internal_ptsvx_test;
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  probe::Enter(fact, n, nrhs, ldb, ldx);
  if (probe::active_fault == probe::Fault::kNone) {
    __real_cptsvx_(fact, n, nrhs, d, e, df, ef, b, ldb, x, ldx, condition, ferr,
                   berr, work, real_work, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                   ,
                   length
#endif
    );
  } else {
    probe::Inject(fact, n, nrhs, df, ef, x, ldx, condition, ferr, berr, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_cptsvx_), decltype(&LAPACK_cptsvx_base)>);
extern "C" void __real_zptsvx_(
    const char* fact, const lapack_int* n, const lapack_int* nrhs,
    const double* d, const std::complex<double>* e, double* df,
    std::complex<double>* ef, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* condition, double* ferr, double* berr, std::complex<double>* work,
    double* real_work, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_zptsvx_(
    const char* fact, const lapack_int* n, const lapack_int* nrhs,
    const double* d, const std::complex<double>* e, double* df,
    std::complex<double>* ef, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* condition, double* ferr, double* berr, std::complex<double>* work,
    double* real_work, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN length
#endif
) {
  namespace probe = asc::internal_ptsvx_test;
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  probe::Enter(fact, n, nrhs, ldb, ldx);
  if (probe::active_fault == probe::Fault::kNone) {
    __real_zptsvx_(fact, n, nrhs, d, e, df, ef, b, ldb, x, ldx, condition, ferr,
                   berr, work, real_work, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                   ,
                   length
#endif
    );
  } else {
    probe::Inject(fact, n, nrhs, df, ef, x, ldx, condition, ferr, berr, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_zptsvx_), decltype(&LAPACK_zptsvx_base)>);
// NOLINTEND(bugprone-reserved-identifier)
