#include "packed_cholesky_solve_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_cholesky_solve_test {
namespace {
std::array<std::size_t, 4> calls{};
std::size_t selected = 0;
Fault fault = Fault::kNone;
}  // namespace
void Arm(std::size_t routine, Fault value) {
  selected = routine;
  fault = value;
}
void Disarm() { fault = Fault::kNone; }
std::size_t Calls(std::size_t routine) { return calls[routine]; }
// Exact audited little-endian native INTEGER objects. Partial writes touch
// only the low half; they do not invent an unbacked or misaligned INFO pointer.
bool Intercept(std::size_t routine, lapack_int n, lapack_int* info) {
  ++calls[routine];
  if (routine != selected || fault == Fault::kNone) {
    return false;
  }
  switch (fault) {
    case Fault::kNoWrite:
      break;
    case Fault::kNegative:
      *info = -2;
      break;
    case Fault::kImpossible:
      *info = n + 1;
      break;
    case Fault::kPositive:
      *info = n;
      break;
    case Fault::kLowZero:
    case Fault::kLowOnes: {
      auto bytes = std::as_writable_bytes(std::span(info, 1));
      const auto low = bytes.first(sizeof(lapack_int) / 2);
      std::fill(low.begin(), low.end(),
                fault == Fault::kLowZero ? std::byte{0} : std::byte{0xff});
      break;
    }
    case Fault::kNone:
      break;
  }
  return true;
}
}  // namespace asc_packed_cholesky_solve_test

// Complete installed signatures are checked against the emitted GNU ABI.
extern "C" {
decltype(LAPACK_spptrs_base) RealSpptrs asm("__real_spptrs_");
decltype(LAPACK_spptrs_base) WrapSpptrs asm("__wrap_spptrs_");
void WrapSpptrs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const float* a, float* b, const lapack_int* ldb,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_solve_test::Intercept(0, *n, info)) {
    b[0] = float{-37};
  } else {
    RealSpptrs(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
decltype(LAPACK_dpptrs_base) RealDpptrs asm("__real_dpptrs_");
decltype(LAPACK_dpptrs_base) WrapDpptrs asm("__wrap_dpptrs_");
void WrapDpptrs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const double* a, double* b, const lapack_int* ldb,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_solve_test::Intercept(1, *n, info)) {
    b[0] = double{-37};
  } else {
    RealDpptrs(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
decltype(LAPACK_cpptrs_base) RealCpptrs asm("__real_cpptrs_");
decltype(LAPACK_cpptrs_base) WrapCpptrs asm("__wrap_cpptrs_");
void WrapCpptrs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_float* a, lapack_complex_float* b,
                const lapack_int* ldb, lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_solve_test::Intercept(2, *n, info)) {
    b[0] = lapack_complex_float{-37};
  } else {
    RealCpptrs(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
decltype(LAPACK_zpptrs_base) RealZpptrs asm("__real_zpptrs_");
decltype(LAPACK_zpptrs_base) WrapZpptrs asm("__wrap_zpptrs_");
void WrapZpptrs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_double* a, lapack_complex_double* b,
                const lapack_int* ldb, lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_solve_test::Intercept(3, *n, info)) {
    b[0] = lapack_complex_double{-37};
  } else {
    RealZpptrs(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
}  // extern "C"
