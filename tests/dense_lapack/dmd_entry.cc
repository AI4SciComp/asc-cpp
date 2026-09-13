#include "dmd_entry.h"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_dmd_entry {
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
// The established ELF interception spelling is confined to this test TU.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_sgedmd_base) __real_sgedmd_;
extern "C" void __wrap_sgedmd_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobf,
    const lapack_int* svd, const lapack_int* m, const lapack_int* n, float* x,
    const lapack_int* ldx, float* y, const lapack_int* ldy,
    const lapack_int* nrnk, const float* tol, lapack_int* rank, float* reig,
    float* imeig, float* z, const lapack_int* ldz, float* res, float* b,
    const lapack_int* ldb, float* w, const lapack_int* ldw, float* matrix_s,
    const lapack_int* lds, float* work, const lapack_int* lwork,
    lapack_int* iwork, const lapack_int* liwork, lapack_int* info,
    std::size_t jobs_length, std::size_t jobz_length, std::size_t jobr_length,
    std::size_t jobf_length) {
  Enter();
  if (Stub(*n, rank, info)) {
    // Deliberately corrupt staged inputs before returning failed diagnostics.
    // The caller's original arrays must remain unchanged on these paths.
    std::fill_n(x, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                float{7});
    std::fill_n(y, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                float{9});
    return;
  }
  __real_sgedmd_(jobs, jobz, jobr, jobf, svd, m, n, x, ldx, y, ldy, nrnk, tol,
                 rank, reig, imeig, z, ldz, res, b, ldb, w, ldw, matrix_s, lds,
                 work, lwork, iwork, liwork, info, jobs_length, jobz_length,
                 jobr_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_sgedmd_base), decltype(__wrap_sgedmd_)>);
extern "C" decltype(LAPACK_dgedmd_base) __real_dgedmd_;
extern "C" void __wrap_dgedmd_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobf,
    const lapack_int* svd, const lapack_int* m, const lapack_int* n, double* x,
    const lapack_int* ldx, double* y, const lapack_int* ldy,
    const lapack_int* nrnk, const double* tol, lapack_int* rank, double* reig,
    double* imeig, double* z, const lapack_int* ldz, double* res, double* b,
    const lapack_int* ldb, double* w, const lapack_int* ldw, double* matrix_s,
    const lapack_int* lds, double* work, const lapack_int* lwork,
    lapack_int* iwork, const lapack_int* liwork, lapack_int* info,
    std::size_t jobs_length, std::size_t jobz_length, std::size_t jobr_length,
    std::size_t jobf_length) {
  Enter();
  if (Stub(*n, rank, info)) {
    // Deliberately corrupt staged inputs before returning failed diagnostics.
    // The caller's original arrays must remain unchanged on these paths.
    std::fill_n(x, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                double{7});
    std::fill_n(y, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                double{9});
    return;
  }
  __real_dgedmd_(jobs, jobz, jobr, jobf, svd, m, n, x, ldx, y, ldy, nrnk, tol,
                 rank, reig, imeig, z, ldz, res, b, ldb, w, ldw, matrix_s, lds,
                 work, lwork, iwork, liwork, info, jobs_length, jobz_length,
                 jobr_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_dgedmd_base), decltype(__wrap_dgedmd_)>);
extern "C" decltype(LAPACK_cgedmd_base) __real_cgedmd_;
extern "C" void __wrap_cgedmd_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobf,
    const lapack_int* svd, const lapack_int* m, const lapack_int* n,
    std::complex<float>* x, const lapack_int* ldx, std::complex<float>* y,
    const lapack_int* ldy, const lapack_int* nrnk, const float* tol,
    lapack_int* rank, std::complex<float>* eigen, std::complex<float>* z,
    const lapack_int* ldz, float* res, std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* w, const lapack_int* ldw,
    std::complex<float>* matrix_s, const lapack_int* lds,
    std::complex<float>* work, const lapack_int* lwork, float* rwork,
    const lapack_int* lrwork, lapack_int* iwork, const lapack_int* liwork,
    lapack_int* info, std::size_t jobs_length, std::size_t jobz_length,
    std::size_t jobr_length, std::size_t jobf_length) {
  Enter();
  if (Stub(*n, rank, info)) {
    // Deliberately corrupt staged inputs before returning failed diagnostics.
    // The caller's original arrays must remain unchanged on these paths.
    std::fill_n(x, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                std::complex<float>{7});
    std::fill_n(y, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                std::complex<float>{9});
    return;
  }
  __real_cgedmd_(jobs, jobz, jobr, jobf, svd, m, n, x, ldx, y, ldy, nrnk, tol,
                 rank, eigen, z, ldz, res, b, ldb, w, ldw, matrix_s, lds, work,
                 lwork, rwork, lrwork, iwork, liwork, info, jobs_length,
                 jobz_length, jobr_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_cgedmd_base), decltype(__wrap_cgedmd_)>);
extern "C" decltype(LAPACK_zgedmd_base) __real_zgedmd_;
extern "C" void __wrap_zgedmd_(
    const char* jobs, const char* jobz, const char* jobr, const char* jobf,
    const lapack_int* svd, const lapack_int* m, const lapack_int* n,
    std::complex<double>* x, const lapack_int* ldx, std::complex<double>* y,
    const lapack_int* ldy, const lapack_int* nrnk, const double* tol,
    lapack_int* rank, std::complex<double>* eigen, std::complex<double>* z,
    const lapack_int* ldz, double* res, std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* w, const lapack_int* ldw,
    std::complex<double>* matrix_s, const lapack_int* lds,
    std::complex<double>* work, const lapack_int* lwork, double* rwork,
    const lapack_int* lrwork, lapack_int* iwork, const lapack_int* liwork,
    lapack_int* info, std::size_t jobs_length, std::size_t jobz_length,
    std::size_t jobr_length, std::size_t jobf_length) {
  Enter();
  if (Stub(*n, rank, info)) {
    // Deliberately corrupt staged inputs before returning failed diagnostics.
    // The caller's original arrays must remain unchanged on these paths.
    std::fill_n(x, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                std::complex<double>{7});
    std::fill_n(y, static_cast<std::size_t>(*m) * static_cast<std::size_t>(*n),
                std::complex<double>{9});
    return;
  }
  __real_zgedmd_(jobs, jobz, jobr, jobf, svd, m, n, x, ldx, y, ldy, nrnk, tol,
                 rank, eigen, z, ldz, res, b, ldb, w, ldw, matrix_s, lds, work,
                 lwork, rwork, lrwork, iwork, liwork, info, jobs_length,
                 jobz_length, jobr_length, jobf_length);
  if (g_mode == Mode::kWarning) {
    *info = 4;
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_zgedmd_base), decltype(__wrap_zgedmd_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_dmd_entry
