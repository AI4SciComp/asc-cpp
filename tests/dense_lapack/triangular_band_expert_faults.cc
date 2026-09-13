#include "triangular_band_expert_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "internal_indefinite.h"

namespace asc_triangular_band_expert_test {
namespace {
std::array<std::size_t, 8> calls{};
std::size_t selected = 0;
Fault current = Fault::kNone;
}  // namespace
void Arm(std::size_t routine, Fault fault) {
  selected = routine;
  current = fault;
}
void Disarm() { current = Fault::kNone; }
std::size_t Calls(std::size_t routine) { return calls[routine]; }

template <typename Real>
bool Intercept(std::size_t routine, lapack_int* info, Real* first,
               Real* second) {
  ++calls[routine];
  if (routine != selected || current == Fault::kNone) {
    return false;
  }
  switch (current) {
    case Fault::kNoInfo:
      break;
    case Fault::kNegativeInfo:
      *info = -2;
      break;
    case Fault::kPositiveInfo:
      *info = 1;
      break;
    case Fault::kLowZero:
    case Fault::kLowOnes: {
      auto bytes = std::as_writable_bytes(std::span(info, 1))
                       .first(sizeof(lapack_int) / 2);
      std::fill(bytes.begin(), bytes.end(),
                current == Fault::kLowZero ? std::byte{0} : std::byte{0xff});
      break;
    }
    case Fault::kNegativeOutput:
    case Fault::kNanOutput:
    case Fault::kInfiniteOutput: {
      *info = 0;
      Real value = Real{-1};
      if (current == Fault::kNanOutput) {
        value = std::numeric_limits<Real>::quiet_NaN();
      }
      if (current == Fault::kInfiniteOutput) {
        value = std::numeric_limits<Real>::infinity();
      }
      *first = value;
      if (second != nullptr) {
        *second = Real{0};
      }
      break;
    }
    case Fault::kNoOutput:
      *info = 0;
      break;
    case Fault::kNone:
      break;
  }
  return true;
}
}  // namespace asc_triangular_band_expert_test

extern "C" {
decltype(LAPACK_stbcon_base) RealStbcon asm("__real_stbcon_");
decltype(LAPACK_stbcon_base) WrapStbcon asm("__wrap_stbcon_");
void WrapStbcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_int* kd, const float* a,
                const lapack_int* ldab, float* rcond, float* work,
                lapack_int* extra, lapack_int* info, std::size_t nl,
                std::size_t ul, std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(
          0, info, rcond, static_cast<float*>(nullptr))) {
    RealStbcon(norm, uplo, diag, n, kd, a, ldab, rcond, work, extra, info, nl,
               ul, dl);
  }
}
decltype(LAPACK_stbrfs_base) RealStbrfs asm("__real_stbrfs_");
decltype(LAPACK_stbrfs_base) WrapStbrfs asm("__wrap_stbrfs_");
void WrapStbrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const float* a, const lapack_int* ldab,
                const float* b, const lapack_int* ldb, const float* x,
                const lapack_int* ldx, float* ferr, float* berr, float* work,
                lapack_int* extra, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(1, info, ferr, berr)) {
    RealStbrfs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, x, ldx, ferr,
               berr, work, extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_dtbcon_base) RealDtbcon asm("__real_dtbcon_");
decltype(LAPACK_dtbcon_base) WrapDtbcon asm("__wrap_dtbcon_");
void WrapDtbcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_int* kd, const double* a,
                const lapack_int* ldab, double* rcond, double* work,
                lapack_int* extra, lapack_int* info, std::size_t nl,
                std::size_t ul, std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(
          2, info, rcond, static_cast<double*>(nullptr))) {
    RealDtbcon(norm, uplo, diag, n, kd, a, ldab, rcond, work, extra, info, nl,
               ul, dl);
  }
}
decltype(LAPACK_dtbrfs_base) RealDtbrfs asm("__real_dtbrfs_");
decltype(LAPACK_dtbrfs_base) WrapDtbrfs asm("__wrap_dtbrfs_");
void WrapDtbrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const double* a, const lapack_int* ldab,
                const double* b, const lapack_int* ldb, const double* x,
                const lapack_int* ldx, double* ferr, double* berr, double* work,
                lapack_int* extra, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(3, info, ferr, berr)) {
    RealDtbrfs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, x, ldx, ferr,
               berr, work, extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_ctbcon_base) RealCtbcon asm("__real_ctbcon_");
decltype(LAPACK_ctbcon_base) WrapCtbcon asm("__wrap_ctbcon_");
void WrapCtbcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_complex_float* a, const lapack_int* ldab,
                float* rcond, lapack_complex_float* work, float* extra,
                lapack_int* info, std::size_t nl, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(
          4, info, rcond, static_cast<float*>(nullptr))) {
    RealCtbcon(norm, uplo, diag, n, kd, a, ldab, rcond, work, extra, info, nl,
               ul, dl);
  }
}
decltype(LAPACK_ctbrfs_base) RealCtbrfs asm("__real_ctbrfs_");
decltype(LAPACK_ctbrfs_base) WrapCtbrfs asm("__wrap_ctbrfs_");
void WrapCtbrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const lapack_complex_float* a,
                const lapack_int* ldab, const lapack_complex_float* b,
                const lapack_int* ldb, const lapack_complex_float* x,
                const lapack_int* ldx, float* ferr, float* berr,
                lapack_complex_float* work, float* extra, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(5, info, ferr, berr)) {
    RealCtbrfs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, x, ldx, ferr,
               berr, work, extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_ztbcon_base) RealZtbcon asm("__real_ztbcon_");
decltype(LAPACK_ztbcon_base) WrapZtbcon asm("__wrap_ztbcon_");
void WrapZtbcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_complex_double* a, const lapack_int* ldab,
                double* rcond, lapack_complex_double* work, double* extra,
                lapack_int* info, std::size_t nl, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(
          6, info, rcond, static_cast<double*>(nullptr))) {
    RealZtbcon(norm, uplo, diag, n, kd, a, ldab, rcond, work, extra, info, nl,
               ul, dl);
  }
}
decltype(LAPACK_ztbrfs_base) RealZtbrfs asm("__real_ztbrfs_");
decltype(LAPACK_ztbrfs_base) WrapZtbrfs asm("__wrap_ztbrfs_");
void WrapZtbrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* kd,
                const lapack_int* nrhs, const lapack_complex_double* a,
                const lapack_int* ldab, const lapack_complex_double* b,
                const lapack_int* ldb, const lapack_complex_double* x,
                const lapack_int* ldx, double* ferr, double* berr,
                lapack_complex_double* work, double* extra, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_triangular_band_expert_test::Intercept(7, info, ferr, berr)) {
    RealZtbrfs(uplo, trans, diag, n, kd, nrhs, a, ldab, b, ldb, x, ldx, ferr,
               berr, work, extra, info, ul, tl, dl);
  }
}
}  // extern "C"
