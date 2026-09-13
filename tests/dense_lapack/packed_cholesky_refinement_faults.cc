#include "packed_cholesky_refinement_faults.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_cholesky_refinement_test {
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
Real ErrorValue(bool forward) {
  if (fault == (forward ? Fault::kNegativeForward : Fault::kNegativeBackward)) {
    return Real{-43};
  }
  if (fault == (forward ? Fault::kNanForward : Fault::kNanBackward)) {
    return std::numeric_limits<Real>::quiet_NaN();
  }
  if (fault == (forward ? Fault::kInfiniteForward : Fault::kInfiniteBackward)) {
    return std::numeric_limits<Real>::infinity();
  }
  return forward ? Real{43} : Real{47};
}

void SetInfo(lapack_int* info) {
  switch (fault) {
    case Fault::kNoWrite:
      return;
    case Fault::kNegative:
      *info = -7;
      return;
    case Fault::kPositive:
      *info = 1;
      return;
    case Fault::kLowZero:
    case Fault::kLowOnes: {
      auto bytes = std::as_writable_bytes(std::span(info, 1));
      const auto low = bytes.first(sizeof(lapack_int) / 2);
      std::fill(low.begin(), low.end(),
                fault == Fault::kLowZero ? std::byte{0} : std::byte{0xff});
      return;
    }
    case Fault::kNone:
    case Fault::kNoForward:
    case Fault::kNoBackward:
    case Fault::kNegativeForward:
    case Fault::kNegativeBackward:
    case Fault::kNanForward:
    case Fault::kNanBackward:
    case Fault::kInfiniteForward:
    case Fault::kInfiniteBackward:
      *info = 0;
      return;
  }
}

template <typename T, typename Real>
bool Intercept(std::size_t routine, Arguments values, T* solution,
               Real* forward, Real* backward, lapack_int* info) {
  arguments[routine] = values;
  ++calls[routine];
  if (routine != selected || fault == Fault::kNone) {
    return false;
  }
  // Synthetic writes isolate publication/report behavior from mathematics.
  for (std::int64_t j = 0; j < values.right_hand_sides; ++j) {
    for (std::int64_t i = 0; i < values.order; ++i) {
      solution[j * values.ldx + i] = T{37};
    }
    if (fault != Fault::kNoForward) {
      forward[j] = ErrorValue<Real>(true);
    }
    if (fault != Fault::kNoBackward) {
      backward[j] = ErrorValue<Real>(false);
    }
  }
  SetInfo(info);
  return true;
}
}  // namespace asc_packed_cholesky_refinement_test

extern "C" {
decltype(LAPACK_spprfs_base) RealSpprfs asm("__real_spprfs_");
decltype(LAPACK_spprfs_base) WrapSpprfs asm("__wrap_spprfs_");
void WrapSpprfs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const float* a, const float* af, const float* b,
                const lapack_int* ldb, float* x, const lapack_int* ldx,
                float* ferr, float* berr, float* work, lapack_int* auxiliary,
                lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_refinement_test::Arguments values{
      *uplo, *n, *nrhs, *ldb, *ldx, length,    a,    af,
      b,     x,  ferr,  berr, work, auxiliary, *info};
  if (!asc_packed_cholesky_refinement_test::Intercept(0, values, x, ferr, berr,
                                                      info)) {
    RealSpprfs(uplo, n, nrhs, a, af, b, ldb, x, ldx, ferr, berr, work,
               auxiliary, info, length);
  }
}
decltype(LAPACK_dpprfs_base) RealDpprfs asm("__real_dpprfs_");
decltype(LAPACK_dpprfs_base) WrapDpprfs asm("__wrap_dpprfs_");
void WrapDpprfs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const double* a, const double* af, const double* b,
                const lapack_int* ldb, double* x, const lapack_int* ldx,
                double* ferr, double* berr, double* work, lapack_int* auxiliary,
                lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_refinement_test::Arguments values{
      *uplo, *n, *nrhs, *ldb, *ldx, length,    a,    af,
      b,     x,  ferr,  berr, work, auxiliary, *info};
  if (!asc_packed_cholesky_refinement_test::Intercept(1, values, x, ferr, berr,
                                                      info)) {
    RealDpprfs(uplo, n, nrhs, a, af, b, ldb, x, ldx, ferr, berr, work,
               auxiliary, info, length);
  }
}
decltype(LAPACK_cpprfs_base) RealCpprfs asm("__real_cpprfs_");
decltype(LAPACK_cpprfs_base) WrapCpprfs asm("__wrap_cpprfs_");
void WrapCpprfs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_float* a, const lapack_complex_float* af,
                const lapack_complex_float* b, const lapack_int* ldb,
                lapack_complex_float* x, const lapack_int* ldx, float* ferr,
                float* berr, lapack_complex_float* work, float* auxiliary,
                lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_refinement_test::Arguments values{
      *uplo, *n, *nrhs, *ldb, *ldx, length,    a,    af,
      b,     x,  ferr,  berr, work, auxiliary, *info};
  if (!asc_packed_cholesky_refinement_test::Intercept(2, values, x, ferr, berr,
                                                      info)) {
    RealCpprfs(uplo, n, nrhs, a, af, b, ldb, x, ldx, ferr, berr, work,
               auxiliary, info, length);
  }
}
decltype(LAPACK_zpprfs_base) RealZpprfs asm("__real_zpprfs_");
decltype(LAPACK_zpprfs_base) WrapZpprfs asm("__wrap_zpprfs_");
void WrapZpprfs(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_double* a, const lapack_complex_double* af,
                const lapack_complex_double* b, const lapack_int* ldb,
                lapack_complex_double* x, const lapack_int* ldx, double* ferr,
                double* berr, lapack_complex_double* work, double* auxiliary,
                lapack_int* info, std::size_t length) {
  const asc_packed_cholesky_refinement_test::Arguments values{
      *uplo, *n, *nrhs, *ldb, *ldx, length,    a,    af,
      b,     x,  ferr,  berr, work, auxiliary, *info};
  if (!asc_packed_cholesky_refinement_test::Intercept(3, values, x, ferr, berr,
                                                      info)) {
    RealZpprfs(uplo, n, nrhs, a, af, b, ldb, x, ldx, ferr, berr, work,
               auxiliary, info, length);
  }
}
}  // extern "C"
