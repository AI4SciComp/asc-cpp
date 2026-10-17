#include "triangular_band_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "internal_indefinite.h"

namespace asc_triangular_band_test {
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
}  // namespace asc_triangular_band_test

// Exact installed declarations retain all three audited GNU CHARACTER lengths.
extern "C" {
decltype(LAPACK_stbtrs_base) RealStbtrs asm("__real_stbtrs_");
decltype(LAPACK_stbtrs_base) WrapStbtrs asm("__wrap_stbtrs_");
void WrapStbtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const float* a, const lapack_int* ldab,
                float* b, const lapack_int* ldb, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_test::Intercept(0, *n, info)) {
    RealStbtrs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, info, ul, tl,
               dl);
  }
}
decltype(LAPACK_dtbtrs_base) RealDtbtrs asm("__real_dtbtrs_");
decltype(LAPACK_dtbtrs_base) WrapDtbtrs asm("__wrap_dtbtrs_");
void WrapDtbtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const double* a, const lapack_int* ldab,
                double* b, const lapack_int* ldb, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_test::Intercept(1, *n, info)) {
    RealDtbtrs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, info, ul, tl,
               dl);
  }
}
decltype(LAPACK_ctbtrs_base) RealCtbtrs asm("__real_ctbtrs_");
decltype(LAPACK_ctbtrs_base) WrapCtbtrs asm("__wrap_ctbtrs_");
void WrapCtbtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const lapack_complex_float* a,
                const lapack_int* ldab, lapack_complex_float* b,
                const lapack_int* ldb, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_test::Intercept(2, *n, info)) {
    RealCtbtrs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, info, ul, tl,
               dl);
  }
}
decltype(LAPACK_ztbtrs_base) RealZtbtrs asm("__real_ztbtrs_");
decltype(LAPACK_ztbtrs_base) WrapZtbtrs asm("__wrap_ztbtrs_");
void WrapZtbtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const lapack_complex_double* a,
                const lapack_int* ldab, lapack_complex_double* b,
                const lapack_int* ldb, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_test::Intercept(3, *n, info)) {
    RealZtbtrs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, info, ul, tl,
               dl);
  }
}
}  // extern "C"
