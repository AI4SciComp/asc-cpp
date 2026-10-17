#include "dmd_qr_entry.h"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_dmd_qr_entry {
namespace {
thread_local Mode g_mode = Mode::kReal;
thread_local std::size_t g_calls = 0;
thread_local void (*g_entry)() = nullptr;
void Enter() {
  ++g_calls;
  if (g_entry != nullptr) {
    g_entry();
  }
}
bool Stub(lapack_int n, lapack_int* rank, lapack_int* info) {
  if (g_mode == Mode::kReal || g_mode == Mode::kWarning) {
    return false;
  }
  if (g_mode != Mode::kOmitRank && g_mode != Mode::kNarrowRank) {
    *rank = n;
  }
  switch (g_mode) {
    case Mode::kOmitInfo:
      break;
    case Mode::kOmitRank:
      *info = 0;
      break;
    case Mode::kSvdFailure:
      *info = 2;
      break;
    case Mode::kEigenFailure:
      *info = 3;
      break;
    case Mode::kNegativeInfo:
      *info = -8;
      break;
    case Mode::kImpossibleInfo:
      *info = 5;
      break;
    case Mode::kImpossibleRank:
      *rank = n + 1;
      *info = 0;
      break;
    case Mode::kNarrowInfo: {
      if constexpr (sizeof(lapack_int) > sizeof(std::int32_t)) {
        const std::int32_t low = -1;
        std::memcpy(info, &low, sizeof(low));
      }
      break;
    }
    case Mode::kNarrowRank: {
      *info = 0;
      if constexpr (sizeof(lapack_int) > sizeof(std::int32_t)) {
        const std::int32_t low = 1;
        std::memcpy(rank, &low, sizeof(low));
      }
      break;
    }
    case Mode::kReal:
    case Mode::kWarning:
      break;
  }
  return true;
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_entry = nullptr;
}
std::size_t Calls() { return g_calls; }
void OnEntry(void (*callback)()) { g_entry = callback; }
// The installed S/C/Z GEDMDQ C declarations mark K const despite Fortran OUT.
// Every intercepted K points to the adapter's live mutable full-width sentinel.
// The localized cast follows that actual object lifetime; no const object is
// modified. NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_sgedmdq_base) __real_sgedmdq_;
extern "C" void __wrap_sgedmdq_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobq,
    const char* jobt, const char* jobf, const lapack_int* svd,
    const lapack_int* m, const lapack_int* n, float* f, const lapack_int* ldf,
    float* x, const lapack_int* ldx, float* y, const lapack_int* ldy,
    const lapack_int* nrnk, const float* tol, const lapack_int* rank,
    float* reig, float* imeig, float* z, const lapack_int* ldz, float* res,
    float* b, const lapack_int* ldb, float* v, const lapack_int* ldv,
    float* matrix_s, const lapack_int* lds, float* work,
    const lapack_int* lwork, lapack_int* iwork, const lapack_int* liwork,
    lapack_int* info, std::size_t jobs_length, std::size_t jobz_length,
    std::size_t jobr_length, std::size_t jobq_length, std::size_t jobt_length,
    std::size_t jobf_length) {
  Enter();
  if (Stub(*n - 1, const_cast<lapack_int*>(rank), info)) {
    std::fill_n(f, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                float{7});
    std::fill_n(x,
                static_cast<std::size_t>(*n) * static_cast<std::size_t>(*n - 1),
                float{9});
    return;
  }
  __real_sgedmdq_(jobs, jobz, jobr, jobq, jobt, jobf, svd, m, n, f, ldf, x, ldx,
                  y, ldy, nrnk, tol, rank, reig, imeig, z, ldz, res, b, ldb, v,
                  ldv, matrix_s, lds, work, lwork, iwork, liwork, info,
                  jobs_length, jobz_length, jobr_length, jobq_length,
                  jobt_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_sgedmdq_base), decltype(__wrap_sgedmdq_)>);
extern "C" decltype(LAPACK_dgedmdq_base) __real_dgedmdq_;
extern "C" void __wrap_dgedmdq_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobq,
    const char* jobt, const char* jobf, const lapack_int* svd,
    const lapack_int* m, const lapack_int* n, double* f, const lapack_int* ldf,
    double* x, const lapack_int* ldx, double* y, const lapack_int* ldy,
    const lapack_int* nrnk, const double* tol, lapack_int* rank, double* reig,
    double* imeig, double* z, const lapack_int* ldz, double* res, double* b,
    const lapack_int* ldb, double* v, const lapack_int* ldv, double* matrix_s,
    const lapack_int* lds, double* work, const lapack_int* lwork,
    lapack_int* iwork, const lapack_int* liwork, lapack_int* info,
    std::size_t jobs_length, std::size_t jobz_length, std::size_t jobr_length,
    std::size_t jobq_length, std::size_t jobt_length, std::size_t jobf_length) {
  Enter();
  if (Stub(*n - 1, rank, info)) {
    std::fill_n(f, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                double{7});
    std::fill_n(x,
                static_cast<std::size_t>(*n) * static_cast<std::size_t>(*n - 1),
                double{9});
    return;
  }
  __real_dgedmdq_(jobs, jobz, jobr, jobq, jobt, jobf, svd, m, n, f, ldf, x, ldx,
                  y, ldy, nrnk, tol, rank, reig, imeig, z, ldz, res, b, ldb, v,
                  ldv, matrix_s, lds, work, lwork, iwork, liwork, info,
                  jobs_length, jobz_length, jobr_length, jobq_length,
                  jobt_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_dgedmdq_base), decltype(__wrap_dgedmdq_)>);
extern "C" decltype(LAPACK_cgedmdq_base) __real_cgedmdq_;
extern "C" void __wrap_cgedmdq_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobq,
    const char* jobt, const char* jobf, const lapack_int* svd,
    const lapack_int* m, const lapack_int* n, std::complex<float>* f,
    const lapack_int* ldf, std::complex<float>* x, const lapack_int* ldx,
    std::complex<float>* y, const lapack_int* ldy, const lapack_int* nrnk,
    const float* tol, const lapack_int* rank, std::complex<float>* eigen,
    std::complex<float>* z, const lapack_int* ldz, float* res,
    std::complex<float>* b, const lapack_int* ldb, std::complex<float>* v,
    const lapack_int* ldv, std::complex<float>* matrix_s, const lapack_int* lds,
    std::complex<float>* work, const lapack_int* lwork, float* rwork,
    const lapack_int* lrwork, lapack_int* iwork, const lapack_int* liwork,
    lapack_int* info, std::size_t jobs_length, std::size_t jobz_length,
    std::size_t jobr_length, std::size_t jobq_length, std::size_t jobt_length,
    std::size_t jobf_length) {
  Enter();
  if (Stub(*n - 1, const_cast<lapack_int*>(rank), info)) {
    std::fill_n(f, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                std::complex<float>{7});
    std::fill_n(x,
                static_cast<std::size_t>(*n) * static_cast<std::size_t>(*n - 1),
                std::complex<float>{9});
    return;
  }
  __real_cgedmdq_(jobs, jobz, jobr, jobq, jobt, jobf, svd, m, n, f, ldf, x, ldx,
                  y, ldy, nrnk, tol, rank, eigen, z, ldz, res, b, ldb, v, ldv,
                  matrix_s, lds, work, lwork, rwork, lrwork, iwork, liwork,
                  info, jobs_length, jobz_length, jobr_length, jobq_length,
                  jobt_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_cgedmdq_base), decltype(__wrap_cgedmdq_)>);
extern "C" decltype(LAPACK_zgedmdq_base) __real_zgedmdq_;
extern "C" void __wrap_zgedmdq_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobq,
    const char* jobt, const char* jobf, const lapack_int* svd,
    const lapack_int* m, const lapack_int* n, std::complex<double>* f,
    const lapack_int* ldf, std::complex<double>* x, const lapack_int* ldx,
    std::complex<double>* y, const lapack_int* ldy, const lapack_int* nrnk,
    const double* tol, const lapack_int* rank, std::complex<double>* eigen,
    std::complex<double>* z, const lapack_int* ldz, double* res,
    std::complex<double>* b, const lapack_int* ldb, std::complex<double>* v,
    const lapack_int* ldv, std::complex<double>* matrix_s,
    const lapack_int* lds, std::complex<double>* work, const lapack_int* lwork,
    double* rwork, const lapack_int* lrwork, lapack_int* iwork,
    const lapack_int* liwork, lapack_int* info, std::size_t jobs_length,
    std::size_t jobz_length, std::size_t jobr_length, std::size_t jobq_length,
    std::size_t jobt_length, std::size_t jobf_length) {
  Enter();
  if (Stub(*n - 1, const_cast<lapack_int*>(rank), info)) {
    std::fill_n(f, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                std::complex<double>{7});
    std::fill_n(x,
                static_cast<std::size_t>(*n) * static_cast<std::size_t>(*n - 1),
                std::complex<double>{9});
    return;
  }
  __real_zgedmdq_(jobs, jobz, jobr, jobq, jobt, jobf, svd, m, n, f, ldf, x, ldx,
                  y, ldy, nrnk, tol, rank, eigen, z, ldz, res, b, ldb, v, ldv,
                  matrix_s, lds, work, lwork, rwork, lrwork, iwork, liwork,
                  info, jobs_length, jobz_length, jobr_length, jobq_length,
                  jobt_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_zgedmdq_base), decltype(__wrap_zgedmdq_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_dmd_qr_entry
