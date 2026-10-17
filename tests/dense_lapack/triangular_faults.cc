#include "triangular_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "internal_indefinite.h"
#include "internal_triangular_prototypes.h"

namespace asc_triangular_test {
namespace {
std::array<std::size_t, 12> calls{};
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
}  // namespace asc_triangular_test

// Ordinary C++ identifiers retain the exact GNU --wrap ELF labels.
extern "C" {
decltype(LAPACK_strtri_base) RealStrtri asm("__real_strtri_");
decltype(LAPACK_strtri_base) WrapStrtri asm("__wrap_strtri_");
void WrapStrtri(const char* uplo, const char* diag, const lapack_int* n,
                float* a, const lapack_int* lda, lapack_int* info,
                std::size_t ul, std::size_t dl) {
  if (!asc_triangular_test::Intercept(0, *n, info)) {
    RealStrtri(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_strti2_base) RealStrti2 asm("__real_strti2_");
decltype(LAPACK_strti2_base) WrapStrti2 asm("__wrap_strti2_");
void WrapStrti2(char* uplo, char* diag, lapack_int* n, float* a,
                lapack_int* lda, lapack_int* info, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(1, *n, info)) {
    RealStrti2(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_strtrs_base) RealStrtrs asm("__real_strtrs_");
decltype(LAPACK_strtrs_base) WrapStrtrs asm("__wrap_strtrs_");
void WrapStrtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const float* a,
                const lapack_int* lda, float* b, const lapack_int* ldb,
                lapack_int* info, std::size_t ul, std::size_t tl,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(2, *n, info)) {
    RealStrtrs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, info, ul, tl, dl);
  }
}
decltype(LAPACK_dtrtri_base) RealDtrtri asm("__real_dtrtri_");
decltype(LAPACK_dtrtri_base) WrapDtrtri asm("__wrap_dtrtri_");
void WrapDtrtri(const char* uplo, const char* diag, const lapack_int* n,
                double* a, const lapack_int* lda, lapack_int* info,
                std::size_t ul, std::size_t dl) {
  if (!asc_triangular_test::Intercept(3, *n, info)) {
    RealDtrtri(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_dtrti2_base) RealDtrti2 asm("__real_dtrti2_");
decltype(LAPACK_dtrti2_base) WrapDtrti2 asm("__wrap_dtrti2_");
void WrapDtrti2(char* uplo, char* diag, lapack_int* n, double* a,
                lapack_int* lda, lapack_int* info, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(4, *n, info)) {
    RealDtrti2(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_dtrtrs_base) RealDtrtrs asm("__real_dtrtrs_");
decltype(LAPACK_dtrtrs_base) WrapDtrtrs asm("__wrap_dtrtrs_");
void WrapDtrtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const double* a,
                const lapack_int* lda, double* b, const lapack_int* ldb,
                lapack_int* info, std::size_t ul, std::size_t tl,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(5, *n, info)) {
    RealDtrtrs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, info, ul, tl, dl);
  }
}
decltype(LAPACK_ctrtri_base) RealCtrtri asm("__real_ctrtri_");
decltype(LAPACK_ctrtri_base) WrapCtrtri asm("__wrap_ctrtri_");
void WrapCtrtri(const char* uplo, const char* diag, const lapack_int* n,
                lapack_complex_float* a, const lapack_int* lda,
                lapack_int* info, std::size_t ul, std::size_t dl) {
  if (!asc_triangular_test::Intercept(6, *n, info)) {
    RealCtrtri(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_ctrti2_base) RealCtrti2 asm("__real_ctrti2_");
decltype(LAPACK_ctrti2_base) WrapCtrti2 asm("__wrap_ctrti2_");
void WrapCtrti2(char* uplo, char* diag, lapack_int* n, lapack_complex_float* a,
                lapack_int* lda, lapack_int* info, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(7, *n, info)) {
    RealCtrti2(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_ctrtrs_base) RealCtrtrs asm("__real_ctrtrs_");
decltype(LAPACK_ctrtrs_base) WrapCtrtrs asm("__wrap_ctrtrs_");
void WrapCtrtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_float* a, const lapack_int* lda,
                lapack_complex_float* b, const lapack_int* ldb,
                lapack_int* info, std::size_t ul, std::size_t tl,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(8, *n, info)) {
    RealCtrtrs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, info, ul, tl, dl);
  }
}
decltype(LAPACK_ztrtri_base) RealZtrtri asm("__real_ztrtri_");
decltype(LAPACK_ztrtri_base) WrapZtrtri asm("__wrap_ztrtri_");
void WrapZtrtri(const char* uplo, const char* diag, const lapack_int* n,
                lapack_complex_double* a, const lapack_int* lda,
                lapack_int* info, std::size_t ul, std::size_t dl) {
  if (!asc_triangular_test::Intercept(9, *n, info)) {
    RealZtrtri(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_ztrti2_base) RealZtrti2 asm("__real_ztrti2_");
decltype(LAPACK_ztrti2_base) WrapZtrti2 asm("__wrap_ztrti2_");
void WrapZtrti2(char* uplo, char* diag, lapack_int* n, lapack_complex_double* a,
                lapack_int* lda, lapack_int* info, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(10, *n, info)) {
    RealZtrti2(uplo, diag, n, a, lda, info, ul, dl);
  }
}
decltype(LAPACK_ztrtrs_base) RealZtrtrs asm("__real_ztrtrs_");
decltype(LAPACK_ztrtrs_base) WrapZtrtrs asm("__wrap_ztrtrs_");
void WrapZtrtrs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_double* a, const lapack_int* lda,
                lapack_complex_double* b, const lapack_int* ldb,
                lapack_int* info, std::size_t ul, std::size_t tl,
                std::size_t dl) {
  if (!asc_triangular_test::Intercept(11, *n, info)) {
    RealZtrtrs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, info, ul, tl, dl);
  }
}
}  // extern "C"
