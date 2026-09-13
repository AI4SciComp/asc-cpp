#include "positive_tridiagonal_driver_entry.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "asc/dense/blas.h"
#include "internal_tridiagonal.h"

namespace asc::internal_ptsv_test {
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

template <typename T>
void Inject(const lapack_int* n, const lapack_int* nrhs,
            DenseBlasRealType<T>* d, T* e, T* b, const lapack_int* ldb,
            lapack_int* info) {
  using Real = DenseBlasRealType<T>;
  for (lapack_int i = 0; i < *n; ++i) {
    d[i] = Real{2};
    if (i + 1 < *n) {
      e[i] = T{};
    }
  }
  for (lapack_int j = 0; j < *nrhs; ++j) {
    for (lapack_int i = 0; i < *n; ++i) {
      b[i + j * *ldb] = T{2};
    }
  }
  switch (active_fault) {
    case Fault::kNoInfo:
      return;
    case Fault::kPartialInfo: {
      const std::int32_t zero = 0;
      std::memcpy(info, &zero, sizeof(zero));
      return;
    }
    case Fault::kNegativeInfo:
      *info = -2;
      return;
    case Fault::kPositiveInfo:
      *info = 1;
      return;
    case Fault::kBeyondInfo:
      *info = *n + 1;
      return;
    case Fault::kNanSolution:
      if (*nrhs > 0) {
        b[(*nrhs - 1) * *ldb + *n - 1] =
            T{std::numeric_limits<Real>::quiet_NaN()};
      }
      break;
    case Fault::kInfiniteSolution:
      if (*nrhs > 0) {
        b[(*nrhs - 1) * *ldb + *n - 1] =
            T{std::numeric_limits<Real>::infinity()};
      }
      break;
    case Fault::kNanDiagonal:
      d[0] = std::numeric_limits<Real>::quiet_NaN();
      break;
    case Fault::kInfiniteDiagonal:
      d[0] = std::numeric_limits<Real>::infinity();
      break;
    case Fault::kNone:
    case Fault::kWrite:
      break;
  }
  *info = 0;
}
void Enter() {
  ++entries;
  if (entry_hook != nullptr) {
    entry_hook();
  }
}
}  // namespace asc::internal_ptsv_test

// GNU ld requires these reserved spellings for test-only foreign interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_sptsv_(const lapack_int* n, const lapack_int* nrhs,
                              float* d, float* e, float* b,
                              const lapack_int* ldb, lapack_int* info);
extern "C" void __wrap_sptsv_(const lapack_int* n, const lapack_int* nrhs,
                              float* d, float* e, float* b,
                              const lapack_int* ldb, lapack_int* info) {
  namespace probe = asc::internal_ptsv_test;
  probe::Enter();
  if (probe::active_fault == probe::Fault::kNone) {
    __real_sptsv_(n, nrhs, d, e, b, ldb, info);
  } else {
    probe::Inject(n, nrhs, d, e, b, ldb, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_sptsv_), decltype(&LAPACK_sptsv)>);
extern "C" void __real_dptsv_(const lapack_int* n, const lapack_int* nrhs,
                              double* d, double* e, double* b,
                              const lapack_int* ldb, lapack_int* info);
extern "C" void __wrap_dptsv_(const lapack_int* n, const lapack_int* nrhs,
                              double* d, double* e, double* b,
                              const lapack_int* ldb, lapack_int* info) {
  namespace probe = asc::internal_ptsv_test;
  probe::Enter();
  if (probe::active_fault == probe::Fault::kNone) {
    __real_dptsv_(n, nrhs, d, e, b, ldb, info);
  } else {
    probe::Inject(n, nrhs, d, e, b, ldb, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_dptsv_), decltype(&LAPACK_dptsv)>);
extern "C" void __real_cptsv_(const lapack_int* n, const lapack_int* nrhs,
                              float* d, std::complex<float>* e,
                              std::complex<float>* b, const lapack_int* ldb,
                              lapack_int* info);
extern "C" void __wrap_cptsv_(const lapack_int* n, const lapack_int* nrhs,
                              float* d, std::complex<float>* e,
                              std::complex<float>* b, const lapack_int* ldb,
                              lapack_int* info) {
  namespace probe = asc::internal_ptsv_test;
  probe::Enter();
  if (probe::active_fault == probe::Fault::kNone) {
    __real_cptsv_(n, nrhs, d, e, b, ldb, info);
  } else {
    probe::Inject(n, nrhs, d, e, b, ldb, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_cptsv_), decltype(&LAPACK_cptsv)>);
extern "C" void __real_zptsv_(const lapack_int* n, const lapack_int* nrhs,
                              double* d, std::complex<double>* e,
                              std::complex<double>* b, const lapack_int* ldb,
                              lapack_int* info);
extern "C" void __wrap_zptsv_(const lapack_int* n, const lapack_int* nrhs,
                              double* d, std::complex<double>* e,
                              std::complex<double>* b, const lapack_int* ldb,
                              lapack_int* info) {
  namespace probe = asc::internal_ptsv_test;
  probe::Enter();
  if (probe::active_fault == probe::Fault::kNone) {
    __real_zptsv_(n, nrhs, d, e, b, ldb, info);
  } else {
    probe::Inject(n, nrhs, d, e, b, ldb, info);
  }
}
static_assert(
    std::is_same_v<decltype(&__wrap_zptsv_), decltype(&LAPACK_zptsv)>);

// NOLINTEND(bugprone-reserved-identifier)
