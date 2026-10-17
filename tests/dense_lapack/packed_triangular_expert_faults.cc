#include "packed_triangular_expert_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "internal_indefinite.h"

namespace asc_packed_triangular_expert_test {
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
}  // namespace asc_packed_triangular_expert_test

extern "C" {
decltype(LAPACK_stpcon_base) RealStpcon asm("__real_stpcon_");
decltype(LAPACK_stpcon_base) WrapStpcon asm("__wrap_stpcon_");
void WrapStpcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const float* a, float* rcond, float* work,
                lapack_int* extra, lapack_int* info, std::size_t nl,
                std::size_t ul, std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(
          0, info, rcond, static_cast<float*>(nullptr))) {
    RealStpcon(norm, uplo, diag, n, a, rcond, work, extra, info, nl, ul, dl);
  }
}
decltype(LAPACK_stprfs_base) RealStprfs asm("__real_stprfs_");
decltype(LAPACK_stprfs_base) WrapStprfs asm("__wrap_stprfs_");
void WrapStprfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const float* a,
                const float* b, const lapack_int* ldb, const float* x,
                const lapack_int* ldx, float* ferr, float* berr, float* work,
                lapack_int* extra, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(1, info, ferr, berr)) {
    RealStprfs(uplo, trans, diag, n, nrhs, a, b, ldb, x, ldx, ferr, berr, work,
               extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_dtpcon_base) RealDtpcon asm("__real_dtpcon_");
decltype(LAPACK_dtpcon_base) WrapDtpcon asm("__wrap_dtpcon_");
void WrapDtpcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const double* a, double* rcond,
                double* work, lapack_int* extra, lapack_int* info,
                std::size_t nl, std::size_t ul, std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(
          2, info, rcond, static_cast<double*>(nullptr))) {
    RealDtpcon(norm, uplo, diag, n, a, rcond, work, extra, info, nl, ul, dl);
  }
}
decltype(LAPACK_dtprfs_base) RealDtprfs asm("__real_dtprfs_");
decltype(LAPACK_dtprfs_base) WrapDtprfs asm("__wrap_dtprfs_");
void WrapDtprfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const double* a,
                const double* b, const lapack_int* ldb, const double* x,
                const lapack_int* ldx, double* ferr, double* berr, double* work,
                lapack_int* extra, lapack_int* info, std::size_t ul,
                std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(3, info, ferr, berr)) {
    RealDtprfs(uplo, trans, diag, n, nrhs, a, b, ldb, x, ldx, ferr, berr, work,
               extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_ctpcon_base) RealCtpcon asm("__real_ctpcon_");
decltype(LAPACK_ctpcon_base) WrapCtpcon asm("__wrap_ctpcon_");
void WrapCtpcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_complex_float* a,
                float* rcond, lapack_complex_float* work, float* extra,
                lapack_int* info, std::size_t nl, std::size_t ul,
                std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(
          4, info, rcond, static_cast<float*>(nullptr))) {
    RealCtpcon(norm, uplo, diag, n, a, rcond, work, extra, info, nl, ul, dl);
  }
}
decltype(LAPACK_ctprfs_base) RealCtprfs asm("__real_ctprfs_");
decltype(LAPACK_ctprfs_base) WrapCtprfs asm("__wrap_ctprfs_");
void WrapCtprfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_float* a, const lapack_complex_float* b,
                const lapack_int* ldb, const lapack_complex_float* x,
                const lapack_int* ldx, float* ferr, float* berr,
                lapack_complex_float* work, float* extra, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(5, info, ferr, berr)) {
    RealCtprfs(uplo, trans, diag, n, nrhs, a, b, ldb, x, ldx, ferr, berr, work,
               extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_ztpcon_base) RealZtpcon asm("__real_ztpcon_");
decltype(LAPACK_ztpcon_base) WrapZtpcon asm("__wrap_ztpcon_");
void WrapZtpcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_complex_double* a,
                double* rcond, lapack_complex_double* work, double* extra,
                lapack_int* info, std::size_t nl, std::size_t ul,
                std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(
          6, info, rcond, static_cast<double*>(nullptr))) {
    RealZtpcon(norm, uplo, diag, n, a, rcond, work, extra, info, nl, ul, dl);
  }
}
decltype(LAPACK_ztprfs_base) RealZtprfs asm("__real_ztprfs_");
decltype(LAPACK_ztprfs_base) WrapZtprfs asm("__wrap_ztprfs_");
void WrapZtprfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_double* a, const lapack_complex_double* b,
                const lapack_int* ldb, const lapack_complex_double* x,
                const lapack_int* ldx, double* ferr, double* berr,
                lapack_complex_double* work, double* extra, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_packed_triangular_expert_test::Intercept(7, info, ferr, berr)) {
    RealZtprfs(uplo, trans, diag, n, nrhs, a, b, ldb, x, ldx, ferr, berr, work,
               extra, info, ul, tl, dl);
  }
}
}  // extern "C"
