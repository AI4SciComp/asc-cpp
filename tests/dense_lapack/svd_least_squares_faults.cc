#include "svd_least_squares_faults.h"

#include <algorithm>
#include <array>
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
#include <lapack.h>
#include <lapacke_config.h>

namespace {
using asc_svd_least_squares_test::Fault;
using asc_svd_least_squares_test::Routine;
Fault g_fault = Fault::kNone;
Routine g_routine = Routine::kGelss;
std::array<std::size_t, 2> g_calls{};
std::size_t g_queries = 0;

template <typename T>
using Real = decltype(std::real(T{}));

template <typename T>
bool Intercept(Routine routine, lapack_int m, lapack_int n, lapack_int nrhs,
               T* a, lapack_int lda, T* b, lapack_int ldb, Real<T>* s,
               lapack_int* rank, lapack_int lwork, lapack_int* info) {
  ++g_calls[static_cast<std::size_t>(routine)];
  g_queries += lwork == -1 ? 1 : 0;
  if (routine != g_routine || g_fault == Fault::kNone ||
      g_fault >= Fault::kQueryZero) {
    return false;
  }
  *info = 0;
  *rank = -91;
  switch (g_fault) {
    case Fault::kNegative:
      *info = -8;
      break;
    case Fault::kMinimumInteger:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case Fault::kPositive:
      *info = 1;
      break;
    case Fault::kImpossiblePositive:
      *info = std::numeric_limits<lapack_int>::max();
      break;
    case Fault::kRankNegative:
      *rank = -1;
      break;
    case Fault::kRankTooLarge:
      *rank = std::min(m, n) + 1;
      break;
    default:
      break;
  }
  if (lwork != -1) {
    for (lapack_int j = 0; j < n; ++j) {
      for (lapack_int i = 0; i < m; ++i) {
        a[j * lda + i] = T{-881};
      }
    }
    for (lapack_int j = 0; j < nrhs; ++j) {
      for (lapack_int i = 0; i < std::max(m, n); ++i) {
        b[j * ldb + i] = i < m ? T{-882} : T{-884};
      }
    }
    for (lapack_int i = 0; i < std::min(m, n); ++i) {
      s[i] = Real<T>{-883};
    }
  }
  return true;
}

template <typename T>
void QueryFault(Routine routine, lapack_int lwork, T* work, Real<T>* real_work,
                lapack_int* integer_work) {
  if (routine != g_routine || lwork != -1) {
    return;
  }
  switch (g_fault) {
    case Fault::kQueryZero:
      work[0] = T{};
      break;
    case Fault::kQueryNan:
      work[0] = T{std::numeric_limits<Real<T>>::quiet_NaN()};
      break;
    case Fault::kQueryInfinity:
      work[0] = T{std::numeric_limits<Real<T>>::infinity()};
      break;
    case Fault::kQueryImaginary:
      if constexpr (!std::is_floating_point_v<T>) {
        work[0].imag(1);
      }
      break;
    case Fault::kRealQueryZero:
      if (real_work != nullptr) {
        real_work[0] = 0;
      }
      break;
    case Fault::kRealQueryNan:
      if (real_work != nullptr) {
        real_work[0] = std::numeric_limits<Real<T>>::quiet_NaN();
      }
      break;
    case Fault::kIntegerQueryZero:
      if (integer_work != nullptr) {
        integer_work[0] = 0;
      }
      break;
    default:
      break;
  }
}
}  // namespace

namespace asc_svd_least_squares_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
}
std::size_t ForeignCalls() { return g_calls[0] + g_calls[1]; }
std::size_t QueryCalls() { return g_queries; }
std::size_t RoutineCalls(Routine routine) {
  return g_calls[static_cast<std::size_t>(routine)];
}
}  // namespace asc_svd_least_squares_test

// GNU test-only wrapper declarations use the exact authoritative lapack.h
// function types. No guessed character or hidden-length ABI is involved.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_sgelss) __real_sgelss_;
decltype(LAPACK_dgelss) __real_dgelss_;
decltype(LAPACK_cgelss) __real_cgelss_;
decltype(LAPACK_zgelss) __real_zgelss_;
decltype(LAPACK_sgelsd) __real_sgelsd_;
decltype(LAPACK_dgelsd) __real_dgelsd_;
decltype(LAPACK_cgelsd) __real_cgelsd_;
decltype(LAPACK_zgelsd) __real_zgelsd_;

void __wrap_sgelss_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, float* a, const lapack_int* lda,
                    float* b, const lapack_int* ldb, float* s,
                    const float* rcond, lapack_int* rank, float* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (!Intercept(Routine::kGelss, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_sgelss_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   info);
    QueryFault(Routine::kGelss, *lwork, work, static_cast<float*>(nullptr),
               nullptr);
  }
}
void __wrap_dgelss_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* b, const lapack_int* ldb, double* s,
                    const double* rcond, lapack_int* rank, double* work,
                    const lapack_int* lwork, lapack_int* info) {
  if (!Intercept(Routine::kGelss, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_dgelss_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   info);
    QueryFault(Routine::kGelss, *lwork, work, static_cast<double*>(nullptr),
               nullptr);
  }
}
void __wrap_cgelss_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<float>* a,
                    const lapack_int* lda, std::complex<float>* b,
                    const lapack_int* ldb, float* s, const float* rcond,
                    lapack_int* rank, std::complex<float>* work,
                    const lapack_int* lwork, float* rwork, lapack_int* info) {
  if (!Intercept(Routine::kGelss, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_cgelss_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   rwork, info);
    QueryFault(Routine::kGelss, *lwork, work, rwork, nullptr);
  }
}
void __wrap_zgelss_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<double>* a,
                    const lapack_int* lda, std::complex<double>* b,
                    const lapack_int* ldb, double* s, const double* rcond,
                    lapack_int* rank, std::complex<double>* work,
                    const lapack_int* lwork, double* rwork, lapack_int* info) {
  if (!Intercept(Routine::kGelss, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_zgelss_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   rwork, info);
    QueryFault(Routine::kGelss, *lwork, work, rwork, nullptr);
  }
}
void __wrap_sgelsd_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, float* a, const lapack_int* lda,
                    float* b, const lapack_int* ldb, float* s,
                    const float* rcond, lapack_int* rank, float* work,
                    const lapack_int* lwork, lapack_int* iwork,
                    lapack_int* info) {
  if (!Intercept(Routine::kGelsd, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_sgelsd_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   iwork, info);
    QueryFault(Routine::kGelsd, *lwork, work, static_cast<float*>(nullptr),
               iwork);
  }
}
void __wrap_dgelsd_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* b, const lapack_int* ldb, double* s,
                    const double* rcond, lapack_int* rank, double* work,
                    const lapack_int* lwork, lapack_int* iwork,
                    lapack_int* info) {
  if (!Intercept(Routine::kGelsd, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_dgelsd_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   iwork, info);
    QueryFault(Routine::kGelsd, *lwork, work, static_cast<double*>(nullptr),
               iwork);
  }
}
void __wrap_cgelsd_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<float>* a,
                    const lapack_int* lda, std::complex<float>* b,
                    const lapack_int* ldb, float* s, const float* rcond,
                    lapack_int* rank, std::complex<float>* work,
                    const lapack_int* lwork, float* rwork, lapack_int* iwork,
                    lapack_int* info) {
  if (!Intercept(Routine::kGelsd, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_cgelsd_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   rwork, iwork, info);
    QueryFault(Routine::kGelsd, *lwork, work, rwork, iwork);
  }
}
void __wrap_zgelsd_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* nrhs, std::complex<double>* a,
                    const lapack_int* lda, std::complex<double>* b,
                    const lapack_int* ldb, double* s, const double* rcond,
                    lapack_int* rank, std::complex<double>* work,
                    const lapack_int* lwork, double* rwork, lapack_int* iwork,
                    lapack_int* info) {
  if (!Intercept(Routine::kGelsd, *m, *n, *nrhs, a, *lda, b, *ldb, s, rank,
                 *lwork, info)) {
    __real_zgelsd_(m, n, nrhs, a, lda, b, ldb, s, rcond, rank, work, lwork,
                   rwork, iwork, info);
    QueryFault(Routine::kGelsd, *lwork, work, rwork, iwork);
  }
}
}  // extern "C"
static_assert(
    std::is_same_v<decltype(__wrap_sgelss_), decltype(LAPACK_sgelss)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgelss_), decltype(LAPACK_dgelss)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgelss_), decltype(LAPACK_cgelss)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgelss_), decltype(LAPACK_zgelss)>);
static_assert(
    std::is_same_v<decltype(__wrap_sgelsd_), decltype(LAPACK_sgelsd)>);
static_assert(
    std::is_same_v<decltype(__wrap_dgelsd_), decltype(LAPACK_dgelsd)>);
static_assert(
    std::is_same_v<decltype(__wrap_cgelsd_), decltype(LAPACK_cgelsd)>);
static_assert(
    std::is_same_v<decltype(__wrap_zgelsd_), decltype(LAPACK_zgelsd)>);
// NOLINTEND(bugprone-reserved-identifier)
