#include "least_squares_faults.h"

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
#include <lapacke_config.h>

namespace {
using asc_least_squares_test::Fault;
using asc_least_squares_test::Routine;
Routine g_routine = Routine::kGels;
Fault g_fault = Fault::kNone;
std::array<std::size_t, 3> g_calls{};
std::size_t g_queries = 0;
std::size_t g_minimum_queries = 0;

template <typename T>
bool Inject(Routine routine, lapack_int lwork, T* a, T* b, T* work,
            lapack_int* info) {
  ++g_calls[static_cast<std::size_t>(routine)];
  const bool query = lwork < 0;
  g_queries += query ? 1 : 0;
  g_minimum_queries += lwork == -2 ? 1 : 0;
  if (routine != g_routine || g_fault == Fault::kNone) {
    return false;
  }
  *info = 0;
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
    case Fault::kQueryZero:
      *work = T{};
      break;
    case Fault::kQueryNan:
      *work = T{std::numeric_limits<float>::quiet_NaN()};
      break;
    case Fault::kQueryImaginary:
      if constexpr (!std::is_floating_point_v<T>) {
        *work = T{32, 1};
      }
      break;
    case Fault::kNone:
      break;
  }
  if (!query) {
    a[0] = T{-881};
    b[0] = T{-882};
  }
  return true;
}
}  // namespace

namespace asc_least_squares_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
}
std::size_t ForeignCalls() { return g_calls[0] + g_calls[1] + g_calls[2]; }
std::size_t QueryCalls() { return g_queries; }
std::size_t MinimumQueries() { return g_minimum_queries; }
std::size_t RoutineCalls(Routine routine) {
  return g_calls[static_cast<std::size_t>(routine)];
}
}  // namespace asc_least_squares_test

// GNU test-only wrapping, matched to source/compiler-derived declarations.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   float* a, lapack_int* lda, float* b, lapack_int* ldb,
                   float* work, lapack_int* lwork, lapack_int* info,
                   std::size_t length);
void __wrap_sgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   float* a, lapack_int* lda, float* b, lapack_int* ldb,
                   float* work, lapack_int* lwork, lapack_int* info,
                   std::size_t length) {
  if (!Inject(Routine::kGels, *lwork, a, b, work, info)) {
    __real_sgels_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info, length);
  }
}
void __real_sgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    float* a, lapack_int* lda, float* b, lapack_int* ldb,
                    float* work, lapack_int* lwork, lapack_int* info,
                    std::size_t length);
void __wrap_sgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    float* a, lapack_int* lda, float* b, lapack_int* ldb,
                    float* work, lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  if (!Inject(Routine::kGelst, *lwork, a, b, work, info)) {
    __real_sgelst_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                   length);
  }
}
void __real_sgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, float* a, lapack_int* lda, float* b,
                     lapack_int* ldb, float* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length);
void __wrap_sgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, float* a, lapack_int* lda, float* b,
                     lapack_int* ldb, float* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGetsls, *lwork, a, b, work, info)) {
    __real_sgetsls_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                    length);
  }
}
void __real_dgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   double* a, lapack_int* lda, double* b, lapack_int* ldb,
                   double* work, lapack_int* lwork, lapack_int* info,
                   std::size_t length);
void __wrap_dgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   double* a, lapack_int* lda, double* b, lapack_int* ldb,
                   double* work, lapack_int* lwork, lapack_int* info,
                   std::size_t length) {
  if (!Inject(Routine::kGels, *lwork, a, b, work, info)) {
    __real_dgels_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info, length);
  }
}
void __real_dgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    double* a, lapack_int* lda, double* b, lapack_int* ldb,
                    double* work, lapack_int* lwork, lapack_int* info,
                    std::size_t length);
void __wrap_dgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    double* a, lapack_int* lda, double* b, lapack_int* ldb,
                    double* work, lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  if (!Inject(Routine::kGelst, *lwork, a, b, work, info)) {
    __real_dgelst_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                   length);
  }
}
void __real_dgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, double* a, lapack_int* lda, double* b,
                     lapack_int* ldb, double* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length);
void __wrap_dgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, double* a, lapack_int* lda, double* b,
                     lapack_int* ldb, double* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGetsls, *lwork, a, b, work, info)) {
    __real_dgetsls_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                    length);
  }
}
void __real_cgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   std::complex<float>* a, lapack_int* lda,
                   std::complex<float>* b, lapack_int* ldb,
                   std::complex<float>* work, lapack_int* lwork,
                   lapack_int* info, std::size_t length);
void __wrap_cgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   std::complex<float>* a, lapack_int* lda,
                   std::complex<float>* b, lapack_int* ldb,
                   std::complex<float>* work, lapack_int* lwork,
                   lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGels, *lwork, a, b, work, info)) {
    __real_cgels_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info, length);
  }
}
void __real_cgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    std::complex<float>* a, lapack_int* lda,
                    std::complex<float>* b, lapack_int* ldb,
                    std::complex<float>* work, lapack_int* lwork,
                    lapack_int* info, std::size_t length);
void __wrap_cgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    std::complex<float>* a, lapack_int* lda,
                    std::complex<float>* b, lapack_int* ldb,
                    std::complex<float>* work, lapack_int* lwork,
                    lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGelst, *lwork, a, b, work, info)) {
    __real_cgelst_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                   length);
  }
}
void __real_cgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, std::complex<float>* a, lapack_int* lda,
                     std::complex<float>* b, lapack_int* ldb,
                     std::complex<float>* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length);
void __wrap_cgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, std::complex<float>* a, lapack_int* lda,
                     std::complex<float>* b, lapack_int* ldb,
                     std::complex<float>* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGetsls, *lwork, a, b, work, info)) {
    __real_cgetsls_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                    length);
  }
}
void __real_zgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   std::complex<double>* a, lapack_int* lda,
                   std::complex<double>* b, lapack_int* ldb,
                   std::complex<double>* work, lapack_int* lwork,
                   lapack_int* info, std::size_t length);
void __wrap_zgels_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                   std::complex<double>* a, lapack_int* lda,
                   std::complex<double>* b, lapack_int* ldb,
                   std::complex<double>* work, lapack_int* lwork,
                   lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGels, *lwork, a, b, work, info)) {
    __real_zgels_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info, length);
  }
}
void __real_zgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    std::complex<double>* a, lapack_int* lda,
                    std::complex<double>* b, lapack_int* ldb,
                    std::complex<double>* work, lapack_int* lwork,
                    lapack_int* info, std::size_t length);
void __wrap_zgelst_(char* trans, lapack_int* m, lapack_int* n, lapack_int* nrhs,
                    std::complex<double>* a, lapack_int* lda,
                    std::complex<double>* b, lapack_int* ldb,
                    std::complex<double>* work, lapack_int* lwork,
                    lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGelst, *lwork, a, b, work, info)) {
    __real_zgelst_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                   length);
  }
}
void __real_zgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, std::complex<double>* a, lapack_int* lda,
                     std::complex<double>* b, lapack_int* ldb,
                     std::complex<double>* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length);
void __wrap_zgetsls_(char* trans, lapack_int* m, lapack_int* n,
                     lapack_int* nrhs, std::complex<double>* a, lapack_int* lda,
                     std::complex<double>* b, lapack_int* ldb,
                     std::complex<double>* work, lapack_int* lwork,
                     lapack_int* info, std::size_t length) {
  if (!Inject(Routine::kGetsls, *lwork, a, b, work, info)) {
    __real_zgetsls_(trans, m, n, nrhs, a, lda, b, ldb, work, lwork, info,
                    length);
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
