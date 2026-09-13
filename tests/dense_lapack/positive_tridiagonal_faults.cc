#include "positive_tridiagonal_faults.h"

#include <complex>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_pt_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local bool g_lengths_valid = true;
bool Intercept(lapack_int n, lapack_int* info) {
  ++g_calls;
  switch (g_mode) {
    case Mode::kPass:
      return false;
    case Mode::kNegative:
    case Mode::kWriteThenNegative:
      *info = -1;
      break;
    case Mode::kExcess:
      *info = n + 1;
      break;
    case Mode::kUnwritten:
      break;
    case Mode::kPartialWidth:
      // Both ABIs execute this check; only true ILP64 has a wider INFO.
      if constexpr (sizeof(lapack_int) > sizeof(std::int32_t)) {
        const std::int32_t low = 0;
        std::memcpy(info, &low, sizeof(low));
      }
      break;
  }
  return true;
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_lengths_valid = true;
}
std::size_t Calls() { return g_calls; }
bool LengthsValid() { return g_lengths_valid; }
// GNU ld's required spellings are confined to this test-only ABI interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_spttrf) __real_spttrf_;
void __wrap_spttrf_(const lapack_int* n, float* d, float* e, lapack_int* info) {
  if (g_mode == Mode::kWriteThenNegative && *n > 0) {
    d[0] = -17;
  }
  if (!Intercept(*n, info)) {
    __real_spttrf_(n, d, e, info);
  }
}
decltype(LAPACK_spttrs) __real_spttrs_;
void __wrap_spttrs_(const lapack_int* n, const lapack_int* nrhs, const float* d,
                    const float* e, float* b, const lapack_int* ldb,
                    lapack_int* info) {
  if (g_mode == Mode::kWriteThenNegative && *n > 0 && *nrhs > 0) {
    b[0] = float{-19};
  }
  if (!Intercept(*n, info)) {
    __real_spttrs_(n, nrhs, d, e, b, ldb, info);
  }
}
decltype(LAPACK_dpttrf) __real_dpttrf_;
void __wrap_dpttrf_(const lapack_int* n, double* d, double* e,
                    lapack_int* info) {
  if (g_mode == Mode::kWriteThenNegative && *n > 0) {
    d[0] = -17;
  }
  if (!Intercept(*n, info)) {
    __real_dpttrf_(n, d, e, info);
  }
}
decltype(LAPACK_dpttrs) __real_dpttrs_;
void __wrap_dpttrs_(const lapack_int* n, const lapack_int* nrhs,
                    const double* d, const double* e, double* b,
                    const lapack_int* ldb, lapack_int* info) {
  if (g_mode == Mode::kWriteThenNegative && *n > 0 && *nrhs > 0) {
    b[0] = double{-19};
  }
  if (!Intercept(*n, info)) {
    __real_dpttrs_(n, nrhs, d, e, b, ldb, info);
  }
}
decltype(LAPACK_cpttrf) __real_cpttrf_;
void __wrap_cpttrf_(const lapack_int* n, float* d, std::complex<float>* e,
                    lapack_int* info) {
  if (g_mode == Mode::kWriteThenNegative && *n > 0) {
    d[0] = -17;
  }
  if (!Intercept(*n, info)) {
    __real_cpttrf_(n, d, e, info);
  }
}
decltype(LAPACK_cpttrs_base) __real_cpttrs_;
void __wrap_cpttrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const float* d,
                    const std::complex<float>* e, std::complex<float>* b,
                    const lapack_int* ldb, lapack_int* info,
                    FORTRAN_STRLEN length) {
  g_lengths_valid = g_lengths_valid && length == 1;
  if (g_mode == Mode::kWriteThenNegative && *n > 0 && *nrhs > 0) {
    b[0] = std::complex<float>{-19};
  }
  if (!Intercept(*n, info)) {
    __real_cpttrs_(uplo, n, nrhs, d, e, b, ldb, info, length);
  }
}
decltype(LAPACK_zpttrf) __real_zpttrf_;
void __wrap_zpttrf_(const lapack_int* n, double* d, std::complex<double>* e,
                    lapack_int* info) {
  if (g_mode == Mode::kWriteThenNegative && *n > 0) {
    d[0] = -17;
  }
  if (!Intercept(*n, info)) {
    __real_zpttrf_(n, d, e, info);
  }
}
decltype(LAPACK_zpttrs_base) __real_zpttrs_;
void __wrap_zpttrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const double* d,
                    const std::complex<double>* e, std::complex<double>* b,
                    const lapack_int* ldb, lapack_int* info,
                    FORTRAN_STRLEN length) {
  g_lengths_valid = g_lengths_valid && length == 1;
  if (g_mode == Mode::kWriteThenNegative && *n > 0 && *nrhs > 0) {
    b[0] = std::complex<double>{-19};
  }
  if (!Intercept(*n, info)) {
    __real_zpttrs_(uplo, n, nrhs, d, e, b, ldb, info, length);
  }
}
}
// Compile against each exact installed prototype, including hidden lengths.
static_assert(
    std::is_same_v<decltype(__wrap_spttrf_), decltype(LAPACK_spttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_spttrs_), decltype(LAPACK_spttrs)>);
static_assert(
    std::is_same_v<decltype(__wrap_dpttrf_), decltype(LAPACK_dpttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_dpttrs_), decltype(LAPACK_dpttrs)>);
static_assert(
    std::is_same_v<decltype(__wrap_cpttrf_), decltype(LAPACK_cpttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_cpttrs_), decltype(LAPACK_cpttrs_base)>);
static_assert(
    std::is_same_v<decltype(__wrap_zpttrf_), decltype(LAPACK_zpttrf)>);
static_assert(
    std::is_same_v<decltype(__wrap_zpttrs_), decltype(LAPACK_zpttrs_base)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_pt_fault
