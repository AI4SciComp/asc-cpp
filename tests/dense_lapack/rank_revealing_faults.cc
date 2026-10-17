#include "rank_revealing_faults.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace asc_rank_revealing_test {
namespace {
Routine selected = Routine::kGeqp3;
Fault active = Fault::kNone;
std::size_t queries = 0;
std::size_t executions = 0;
std::size_t non_normalized_flags = 0;

template <typename T>
using Real = decltype(std::real(T{}));

template <typename T>
bool Intercept(Routine routine, lapack_int n, lapack_int lwork,
               const lapack_int* pivots, T* work, lapack_int* info) {
  if (lwork == -1) {
    ++queries;
  } else {
    ++executions;
    for (lapack_int i = 0; i < n; ++i) {
      if (pivots[i] != 0 && pivots[i] != 1) {
        ++non_normalized_flags;
      }
    }
  }
  if (selected != routine || active == Fault::kNone) {
    return false;
  }
  if (active == Fault::kNegativeInfo || active == Fault::kMinimumInfo ||
      active == Fault::kPositiveInfo) {
    *info = 1;
    if (active == Fault::kNegativeInfo) {
      *info = -3;
    } else if (active == Fault::kMinimumInfo) {
      *info = std::numeric_limits<lapack_int>::min();
    }
    return true;
  }
  if (lwork != -1) {
    return false;
  }
  *info = 0;
  switch (active) {
    case Fault::kQueryNan:
      *work = T{std::numeric_limits<Real<T>>::quiet_NaN()};
      return true;
    case Fault::kQueryNegative:
      *work = T{-1};
      return true;
    case Fault::kQueryImaginary:
      if constexpr (!std::is_floating_point_v<T>) {
        *work = T{32, 1};
      } else {
        *work = T{33};
      }
      return true;
    case Fault::kQueryOverflow:
      *work = T{std::ldexp(Real<T>{1}, ASC_LAPACK_INTEGER_BITS - 1)};
      return true;
    default:
      return false;
  }
}

void Corrupt(Routine routine, lapack_int m, lapack_int n, lapack_int lwork,
             lapack_int* pivots, lapack_int* rank) {
  if (lwork == -1 || selected != routine) {
    return;
  }
  switch (active) {
    case Fault::kPivotZero:
      if (n > 0) {
        pivots[0] = 0;
      }
      break;
    case Fault::kPivotLarge:
      if (n > 0) {
        pivots[0] = n + 1;
      }
      break;
    case Fault::kPivotDuplicate:
      if (n > 1) {
        pivots[1] = pivots[0];
      }
      break;
    case Fault::kRankNegative:
      if (rank != nullptr) {
        *rank = -1;
      }
      break;
    case Fault::kRankLarge:
      if (rank != nullptr) {
        *rank = std::min(m, n) + 1;
      }
      break;
    default:
      break;
  }
}
}  // namespace

void SetFault(Routine routine, Fault fault) {
  selected = routine;
  active = fault;
}
std::size_t ForeignQueries() { return queries; }
std::size_t ForeignExecutions() { return executions; }
std::size_t NonNormalizedFlags() { return non_normalized_flags; }
}  // namespace asc_rank_revealing_test

// GNU test-only wrapping requires these reserved linker spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgeqp3_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, lapack_int* pivots, float* tau,
                    float* work, const lapack_int* lwork, lapack_int* info);
void __wrap_sgeqp3_(const lapack_int* m, const lapack_int* n, float* a,
                    const lapack_int* lda, lapack_int* pivots, float* tau,
                    float* work, const lapack_int* lwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGeqp3, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_sgeqp3_(m, n, a, lda, pivots, tau, work, lwork, info);
  asc_rank_revealing_test::Corrupt(Routine::kGeqp3, *m, *n, *lwork, pivots,
                                   nullptr);
}

void __real_sgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, float* a, const lapack_int* lda,
                    float* b, const lapack_int* ldb, lapack_int* pivots,
                    const float* rcond, lapack_int* rank, float* work,
                    const lapack_int* lwork, lapack_int* info);
void __wrap_sgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, float* a, const lapack_int* lda,
                    float* b, const lapack_int* ldb, lapack_int* pivots,
                    const float* rcond, lapack_int* rank, float* work,
                    const lapack_int* lwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGelsy, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_sgelsy_(m, n, nrhs, a, lda, b, ldb, pivots, rcond, rank, work, lwork,
                 info);
  asc_rank_revealing_test::Corrupt(Routine::kGelsy, *m, *n, *lwork, pivots,
                                   rank);
}

void __real_dgeqp3_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots, double* tau,
                    double* work, const lapack_int* lwork, lapack_int* info);
void __wrap_dgeqp3_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots, double* tau,
                    double* work, const lapack_int* lwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGeqp3, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_dgeqp3_(m, n, a, lda, pivots, tau, work, lwork, info);
  asc_rank_revealing_test::Corrupt(Routine::kGeqp3, *m, *n, *lwork, pivots,
                                   nullptr);
}

void __real_dgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* b, const lapack_int* ldb, lapack_int* pivots,
                    const double* rcond, lapack_int* rank, double* work,
                    const lapack_int* lwork, lapack_int* info);
void __wrap_dgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* b, const lapack_int* ldb, lapack_int* pivots,
                    const double* rcond, lapack_int* rank, double* work,
                    const lapack_int* lwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGelsy, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_dgelsy_(m, n, nrhs, a, lda, b, ldb, pivots, rcond, rank, work, lwork,
                 info);
  asc_rank_revealing_test::Corrupt(Routine::kGelsy, *m, *n, *lwork, pivots,
                                   rank);
}

void __real_cgeqp3_(const lapack_int* m, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_float* tau,
                    lapack_complex_float* work, const lapack_int* lwork,
                    float* rwork, lapack_int* info);
void __wrap_cgeqp3_(const lapack_int* m, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_float* tau,
                    lapack_complex_float* work, const lapack_int* lwork,
                    float* rwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGeqp3, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_cgeqp3_(m, n, a, lda, pivots, tau, work, lwork, rwork, info);
  asc_rank_revealing_test::Corrupt(Routine::kGeqp3, *m, *n, *lwork, pivots,
                                   nullptr);
}

void __real_cgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, lapack_complex_float* a,
                    const lapack_int* lda, lapack_complex_float* b,
                    const lapack_int* ldb, lapack_int* pivots,
                    const float* rcond, lapack_int* rank,
                    lapack_complex_float* work, const lapack_int* lwork,
                    float* rwork, lapack_int* info);
void __wrap_cgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, lapack_complex_float* a,
                    const lapack_int* lda, lapack_complex_float* b,
                    const lapack_int* ldb, lapack_int* pivots,
                    const float* rcond, lapack_int* rank,
                    lapack_complex_float* work, const lapack_int* lwork,
                    float* rwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGelsy, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_cgelsy_(m, n, nrhs, a, lda, b, ldb, pivots, rcond, rank, work, lwork,
                 rwork, info);
  asc_rank_revealing_test::Corrupt(Routine::kGelsy, *m, *n, *lwork, pivots,
                                   rank);
}

void __real_zgeqp3_(const lapack_int* m, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_double* tau,
                    lapack_complex_double* work, const lapack_int* lwork,
                    double* rwork, lapack_int* info);
void __wrap_zgeqp3_(const lapack_int* m, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_double* tau,
                    lapack_complex_double* work, const lapack_int* lwork,
                    double* rwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGeqp3, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_zgeqp3_(m, n, a, lda, pivots, tau, work, lwork, rwork, info);
  asc_rank_revealing_test::Corrupt(Routine::kGeqp3, *m, *n, *lwork, pivots,
                                   nullptr);
}

void __real_zgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, lapack_complex_double* a,
                    const lapack_int* lda, lapack_complex_double* b,
                    const lapack_int* ldb, lapack_int* pivots,
                    const double* rcond, lapack_int* rank,
                    lapack_complex_double* work, const lapack_int* lwork,
                    double* rwork, lapack_int* info);
void __wrap_zgelsy_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, lapack_complex_double* a,
                    const lapack_int* lda, lapack_complex_double* b,
                    const lapack_int* ldb, lapack_int* pivots,
                    const double* rcond, lapack_int* rank,
                    lapack_complex_double* work, const lapack_int* lwork,
                    double* rwork, lapack_int* info) {
  using asc_rank_revealing_test::Routine;
  if (asc_rank_revealing_test::Intercept(Routine::kGelsy, *n, *lwork, pivots,
                                         work, info)) {
    return;
  }
  __real_zgelsy_(m, n, nrhs, a, lda, b, ldb, pivots, rcond, rank, work, lwork,
                 rwork, info);
  asc_rank_revealing_test::Corrupt(Routine::kGelsy, *m, *n, *lwork, pivots,
                                   rank);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
