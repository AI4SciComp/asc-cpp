#include "packed_cholesky_driver_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_cholesky_driver_test {
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
bool Intercept(std::size_t routine, char uplo, lapack_int n, lapack_int nrhs,
               lapack_int ldb, std::size_t length, lapack_int* info) {
  ++calls[routine];
  arguments[routine] = {uplo, n, nrhs, ldb, length};
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
// Deliberate mutation sentinels distinguish direct writes from row publication.
// Positive INFO labels a synthetic partial factor; real failures are tested
// independently against the source-defined first/last nonpositive minors.
template <typename T>
void Mutate(T* a, T* b, lapack_int nrhs) {
  a[0] = T{-37};
  if (fault == Fault::kPositive) {
    a[1] = T{-38};
    a[2] = T{-39};
  } else if (nrhs > 0) {
    b[0] = T{-41};
  }
}
}  // namespace asc_packed_cholesky_driver_test

// Complete installed signatures are checked against the emitted GNU ABI.
extern "C" {
decltype(LAPACK_sppsv_base) RealSppsv asm("__real_sppsv_");
decltype(LAPACK_sppsv_base) WrapSppsv asm("__wrap_sppsv_");
void WrapSppsv(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
               float* a, float* b, const lapack_int* ldb, lapack_int* info,
               std::size_t length) {
  if (asc_packed_cholesky_driver_test::Intercept(0, *uplo, *n, *nrhs, *ldb,
                                                 length, info)) {
    asc_packed_cholesky_driver_test::Mutate(a, b, *nrhs);
  } else {
    RealSppsv(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
decltype(LAPACK_dppsv_base) RealDppsv asm("__real_dppsv_");
decltype(LAPACK_dppsv_base) WrapDppsv asm("__wrap_dppsv_");
void WrapDppsv(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
               double* a, double* b, const lapack_int* ldb, lapack_int* info,
               std::size_t length) {
  if (asc_packed_cholesky_driver_test::Intercept(1, *uplo, *n, *nrhs, *ldb,
                                                 length, info)) {
    asc_packed_cholesky_driver_test::Mutate(a, b, *nrhs);
  } else {
    RealDppsv(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
decltype(LAPACK_cppsv_base) RealCppsv asm("__real_cppsv_");
decltype(LAPACK_cppsv_base) WrapCppsv asm("__wrap_cppsv_");
void WrapCppsv(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
               lapack_complex_float* a, lapack_complex_float* b,
               const lapack_int* ldb, lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_driver_test::Intercept(2, *uplo, *n, *nrhs, *ldb,
                                                 length, info)) {
    asc_packed_cholesky_driver_test::Mutate(a, b, *nrhs);
  } else {
    RealCppsv(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
decltype(LAPACK_zppsv_base) RealZppsv asm("__real_zppsv_");
decltype(LAPACK_zppsv_base) WrapZppsv asm("__wrap_zppsv_");
void WrapZppsv(const char* uplo, const lapack_int* n, const lapack_int* nrhs,
               lapack_complex_double* a, lapack_complex_double* b,
               const lapack_int* ldb, lapack_int* info, std::size_t length) {
  if (asc_packed_cholesky_driver_test::Intercept(3, *uplo, *n, *nrhs, *ldb,
                                                 length, info)) {
    asc_packed_cholesky_driver_test::Mutate(a, b, *nrhs);
  } else {
    RealZppsv(uplo, n, nrhs, a, b, ldb, info, length);
  }
}
}  // extern "C"
