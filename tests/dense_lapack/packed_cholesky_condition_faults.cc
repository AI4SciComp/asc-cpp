#include "packed_cholesky_condition_faults.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_cholesky_condition_test {
namespace {
std::array<std::size_t, 4> calls{};
std::array<Arguments, 4> arguments{};
std::size_t selected = 0;
Fault fault = Fault::kNone;
static_assert(std::endian::native == std::endian::little);
}  // namespace
void Arm(std::size_t routine, Fault value) {
  selected = routine;
  fault = value;
}
void Disarm() { fault = Fault::kNone; }
std::size_t Calls(std::size_t routine) { return calls[routine]; }
Arguments LastArguments(std::size_t routine) { return arguments[routine]; }

template <typename Real>
bool Intercept(std::size_t routine, Arguments values, Real* output,
               lapack_int* info) {
  arguments[routine] = values;
  ++calls[routine];
  if (routine != selected || fault == Fault::kNone) {
    return false;
  }
  // Synthetic direct outputs isolate INFO/report translation from mathematics.
  if (fault != Fault::kNoOutput) {
    *output = Real{37};
  }
  switch (fault) {
    case Fault::kNoWrite:
      break;
    case Fault::kNegative:
      *info = -2;
      break;
    case Fault::kPositive:
      *info = 1;
      break;
    case Fault::kLowZero:
    case Fault::kLowOnes: {
      auto bytes = std::as_writable_bytes(std::span(info, 1));
      const auto low = bytes.first(sizeof(lapack_int) / 2);
      std::fill(low.begin(), low.end(),
                fault == Fault::kLowZero ? std::byte{0} : std::byte{0xff});
      break;
    }
    case Fault::kNegativeOutput:
      *output = Real{-37};
      *info = 0;
      break;
    case Fault::kNanOutput:
      *output = std::numeric_limits<Real>::quiet_NaN();
      *info = 0;
      break;
    case Fault::kInfiniteOutput:
      *output = std::numeric_limits<Real>::infinity();
      *info = 0;
      break;
    case Fault::kNoOutput:
      *info = 0;
      break;
    case Fault::kNone:
      break;
  }
  return true;
}
}  // namespace asc_packed_cholesky_condition_test

extern "C" {
decltype(LAPACK_sppcon_base) RealSppcon asm("__real_sppcon_");
decltype(LAPACK_sppcon_base) WrapSppcon asm("__wrap_sppcon_");
void WrapSppcon(const char* uplo, const lapack_int* n, const float* a,
                const float* norm, float* rcond, float* work,
                lapack_int* auxiliary, lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_condition_test::Arguments values{
      *uplo, *n, length, a, rcond, work, auxiliary, *norm};
  if (!asc_packed_cholesky_condition_test::Intercept(0, values, rcond, info)) {
    RealSppcon(uplo, n, a, norm, rcond, work, auxiliary, info, length);
  }
}
decltype(LAPACK_dppcon_base) RealDppcon asm("__real_dppcon_");
decltype(LAPACK_dppcon_base) WrapDppcon asm("__wrap_dppcon_");
void WrapDppcon(const char* uplo, const lapack_int* n, const double* a,
                const double* norm, double* rcond, double* work,
                lapack_int* auxiliary, lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_condition_test::Arguments values{
      *uplo, *n, length, a, rcond, work, auxiliary, *norm};
  if (!asc_packed_cholesky_condition_test::Intercept(1, values, rcond, info)) {
    RealDppcon(uplo, n, a, norm, rcond, work, auxiliary, info, length);
  }
}
decltype(LAPACK_cppcon_base) RealCppcon asm("__real_cppcon_");
decltype(LAPACK_cppcon_base) WrapCppcon asm("__wrap_cppcon_");
void WrapCppcon(const char* uplo, const lapack_int* n,
                const lapack_complex_float* a, const float* norm, float* rcond,
                lapack_complex_float* work, float* auxiliary, lapack_int* info,
                std::size_t length) {
  const asc_packed_cholesky_condition_test::Arguments values{
      *uplo, *n, length, a, rcond, work, auxiliary, *norm};
  if (!asc_packed_cholesky_condition_test::Intercept(2, values, rcond, info)) {
    RealCppcon(uplo, n, a, norm, rcond, work, auxiliary, info, length);
  }
}
decltype(LAPACK_zppcon_base) RealZppcon asm("__real_zppcon_");
decltype(LAPACK_zppcon_base) WrapZppcon asm("__wrap_zppcon_");
void WrapZppcon(const char* uplo, const lapack_int* n,
                const lapack_complex_double* a, const double* norm,
                double* rcond, lapack_complex_double* work, double* auxiliary,
                lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_condition_test::Arguments values{
      *uplo, *n, length, a, rcond, work, auxiliary, *norm};
  if (!asc_packed_cholesky_condition_test::Intercept(3, values, rcond, info)) {
    RealZppcon(uplo, n, a, norm, rcond, work, auxiliary, info, length);
  }
}
}  // extern "C"
