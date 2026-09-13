#include "packed_cholesky_equilibration_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_cholesky_equilibration_test {
namespace {
std::array<std::size_t, 4> calls{};
std::array<Arguments, 4> arguments{};
std::size_t selected = 0;
Fault fault = Fault::kNone;
}  // namespace
void Arm(std::size_t routine, Fault value) {
  selected = routine;
  fault = value;
}
void Disarm() { fault = Fault::kNone; }
std::size_t Calls(std::size_t routine) { return calls[routine]; }
Arguments LastArguments(std::size_t routine) { return arguments[routine]; }
// Exact audited little-endian native INTEGER objects. Partial writes touch
// only the low half; they do not invent an unbacked or misaligned INFO pointer.
bool Intercept(std::size_t routine, char uplo, lapack_int n, const void* a,
               void* scales, void* condition, void* maximum, std::size_t length,
               lapack_int* info) {
  ++calls[routine];
  arguments[routine] = {uplo, n, length, a, scales, condition, maximum};
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
    case Fault::kBadScale:
    case Fault::kBadCondition:
    case Fault::kBadMaximum:
      *info = 0;
      break;
    case Fault::kNone:
      break;
  }
  return true;
}
// Synthetic outputs distinguish partial and unusable direct publication.
// Real nonpositive diagonals are tested independently, not inferred here.
template <typename Real>
void Mutate(Real* scales, Real* condition, Real* maximum) {
  if (fault == Fault::kBadScale || fault == Fault::kBadCondition ||
      fault == Fault::kBadMaximum) {
    scales[0] = fault == Fault::kBadScale ? Real{} : Real{1};
    scales[1] = Real{1};
    *condition = fault == Fault::kBadCondition ? Real{-1} : Real{1};
    *maximum = fault == Fault::kBadMaximum
                   ? std::numeric_limits<Real>::infinity()
                   : Real{1};
    return;
  }
  scales[0] = Real{-37};
  scales[1] = Real{-38};
  *maximum = Real{-41};
  if (fault != Fault::kPositive) {
    *condition = Real{-43};
  }
}
}  // namespace asc_packed_cholesky_equilibration_test

// Complete installed signatures are independently checked against GNU ABI.
extern "C" {
decltype(LAPACK_sppequ_base) RealSppequ asm("__real_sppequ_");
decltype(LAPACK_sppequ_base) WrapSppequ asm("__wrap_sppequ_");
void WrapSppequ(const char* uplo, const lapack_int* n, const float* a,
                float* scales, float* condition, float* maximum,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_equilibration_test::Intercept(
          0, *uplo, *n, a, scales, condition, maximum, length, info)) {
    asc_packed_cholesky_equilibration_test::Mutate(scales, condition, maximum);
  } else {
    RealSppequ(uplo, n, a, scales, condition, maximum, info, length);
  }
}
decltype(LAPACK_dppequ_base) RealDppequ asm("__real_dppequ_");
decltype(LAPACK_dppequ_base) WrapDppequ asm("__wrap_dppequ_");
void WrapDppequ(const char* uplo, const lapack_int* n, const double* a,
                double* scales, double* condition, double* maximum,
                lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_equilibration_test::Intercept(
          1, *uplo, *n, a, scales, condition, maximum, length, info)) {
    asc_packed_cholesky_equilibration_test::Mutate(scales, condition, maximum);
  } else {
    RealDppequ(uplo, n, a, scales, condition, maximum, info, length);
  }
}
decltype(LAPACK_cppequ_base) RealCppequ asm("__real_cppequ_");
decltype(LAPACK_cppequ_base) WrapCppequ asm("__wrap_cppequ_");
void WrapCppequ(const char* uplo, const lapack_int* n,
                const lapack_complex_float* a, float* scales, float* condition,
                float* maximum, lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_equilibration_test::Intercept(
          2, *uplo, *n, a, scales, condition, maximum, length, info)) {
    asc_packed_cholesky_equilibration_test::Mutate(scales, condition, maximum);
  } else {
    RealCppequ(uplo, n, a, scales, condition, maximum, info, length);
  }
}
decltype(LAPACK_zppequ_base) RealZppequ asm("__real_zppequ_");
decltype(LAPACK_zppequ_base) WrapZppequ asm("__wrap_zppequ_");
void WrapZppequ(const char* uplo, const lapack_int* n,
                const lapack_complex_double* a, double* scales,
                double* condition, double* maximum, lapack_int* info,
                std::size_t length) {
  if (asc_packed_cholesky_equilibration_test::Intercept(
          3, *uplo, *n, a, scales, condition, maximum, length, info)) {
    asc_packed_cholesky_equilibration_test::Mutate(scales, condition, maximum);
  } else {
    RealZppequ(uplo, n, a, scales, condition, maximum, info, length);
  }
}
}  // extern "C"
