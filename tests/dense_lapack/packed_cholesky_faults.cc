#include "packed_cholesky_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_cholesky_test {
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
}  // namespace asc_packed_cholesky_test

// Exact installed declarations retain audited GNU --wrap labels.
extern "C" {
decltype(LAPACK_spptrf_base) RealSpptrf asm("__real_spptrf_");
decltype(LAPACK_spptrf_base) WrapSpptrf asm("__wrap_spptrf_");
void WrapSpptrf(const char* uplo, const lapack_int* n, float* a,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_test::Intercept(0, *n, info)) {
    // A real live native destination is changed before the injected return.
    // Defects must withhold row publication but cannot roll back direct writes.
    a[0] = float{-37};
  } else {
    RealSpptrf(uplo, n, a, info, length);
  }
}
decltype(LAPACK_dpptrf_base) RealDpptrf asm("__real_dpptrf_");
decltype(LAPACK_dpptrf_base) WrapDpptrf asm("__wrap_dpptrf_");
void WrapDpptrf(const char* uplo, const lapack_int* n, double* a,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_test::Intercept(1, *n, info)) {
    // A real live native destination is changed before the injected return.
    // Defects must withhold row publication but cannot roll back direct writes.
    a[0] = double{-37};
  } else {
    RealDpptrf(uplo, n, a, info, length);
  }
}
decltype(LAPACK_cpptrf_base) RealCpptrf asm("__real_cpptrf_");
decltype(LAPACK_cpptrf_base) WrapCpptrf asm("__wrap_cpptrf_");
void WrapCpptrf(const char* uplo, const lapack_int* n, lapack_complex_float* a,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_test::Intercept(2, *n, info)) {
    // A real live native destination is changed before the injected return.
    // Defects must withhold row publication but cannot roll back direct writes.
    a[0] = lapack_complex_float{-37};
  } else {
    RealCpptrf(uplo, n, a, info, length);
  }
}
decltype(LAPACK_zpptrf_base) RealZpptrf asm("__real_zpptrf_");
decltype(LAPACK_zpptrf_base) WrapZpptrf asm("__wrap_zpptrf_");
void WrapZpptrf(const char* uplo, const lapack_int* n, lapack_complex_double* a,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_test::Intercept(3, *n, info)) {
    // A real live native destination is changed before the injected return.
    // Defects must withhold row publication but cannot roll back direct writes.
    a[0] = lapack_complex_double{-37};
  } else {
    RealZpptrf(uplo, n, a, info, length);
  }
}
}  // extern "C"
