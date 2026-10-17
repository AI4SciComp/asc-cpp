#include "packed_triangular_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_triangular_test {
namespace {
std::array<std::size_t, 8> calls{};
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
}  // namespace asc_packed_triangular_test

// Exact installed declarations retain audited GNU --wrap labels.
extern "C" {
decltype(LAPACK_stptri_base) RealStptri asm("__real_stptri_");
decltype(LAPACK_stptri_base) WrapStptri asm("__wrap_stptri_");
void WrapStptri(const char* uplo, const char* diag, const lapack_int* n,
                float* a, lapack_int* info, std::size_t ul, std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(0, *n, info)) {
    RealStptri(uplo, diag, n, a, info, ul, dl);
  }
}
decltype(LAPACK_stptrs_base) RealStptrs asm("__real_stptrs_");
decltype(LAPACK_stptrs_base) WrapStptrs asm("__wrap_stptrs_");
void WrapStptrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const float* a,
                float* b, const lapack_int* ldb, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(1, *n, info)) {
    RealStptrs(uplo, trans, diag, n, nrhs, a, b, ldb, info, ul, tl, dl);
  }
}
decltype(LAPACK_dtptri_base) RealDtptri asm("__real_dtptri_");
decltype(LAPACK_dtptri_base) WrapDtptri asm("__wrap_dtptri_");
void WrapDtptri(const char* uplo, const char* diag, const lapack_int* n,
                double* a, lapack_int* info, std::size_t ul, std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(2, *n, info)) {
    RealDtptri(uplo, diag, n, a, info, ul, dl);
  }
}
decltype(LAPACK_dtptrs_base) RealDtptrs asm("__real_dtptrs_");
decltype(LAPACK_dtptrs_base) WrapDtptrs asm("__wrap_dtptrs_");
void WrapDtptrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const double* a,
                double* b, const lapack_int* ldb, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(3, *n, info)) {
    RealDtptrs(uplo, trans, diag, n, nrhs, a, b, ldb, info, ul, tl, dl);
  }
}
decltype(LAPACK_ctptri_base) RealCtptri asm("__real_ctptri_");
decltype(LAPACK_ctptri_base) WrapCtptri asm("__wrap_ctptri_");
void WrapCtptri(const char* uplo, const char* diag, const lapack_int* n,
                lapack_complex_float* a, lapack_int* info, std::size_t ul,
                std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(4, *n, info)) {
    RealCtptri(uplo, diag, n, a, info, ul, dl);
  }
}
decltype(LAPACK_ctptrs_base) RealCtptrs asm("__real_ctptrs_");
decltype(LAPACK_ctptrs_base) WrapCtptrs asm("__wrap_ctptrs_");
void WrapCtptrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_float* a, lapack_complex_float* b,
                const lapack_int* ldb, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(5, *n, info)) {
    RealCtptrs(uplo, trans, diag, n, nrhs, a, b, ldb, info, ul, tl, dl);
  }
}
decltype(LAPACK_ztptri_base) RealZtptri asm("__real_ztptri_");
decltype(LAPACK_ztptri_base) WrapZtptri asm("__wrap_ztptri_");
void WrapZtptri(const char* uplo, const char* diag, const lapack_int* n,
                lapack_complex_double* a, lapack_int* info, std::size_t ul,
                std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(6, *n, info)) {
    RealZtptri(uplo, diag, n, a, info, ul, dl);
  }
}
decltype(LAPACK_ztptrs_base) RealZtptrs asm("__real_ztptrs_");
decltype(LAPACK_ztptrs_base) WrapZtptrs asm("__wrap_ztptrs_");
void WrapZtptrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_double* a, lapack_complex_double* b,
                const lapack_int* ldb, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_test::Intercept(7, *n, info)) {
    RealZtptrs(uplo, trans, diag, n, nrhs, a, b, ldb, info, ul, tl, dl);
  }
}
}  // extern "C"
