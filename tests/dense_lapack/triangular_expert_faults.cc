#include "triangular_expert_faults.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "internal_indefinite.h"

namespace asc_triangular_expert_test {
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
}  // namespace asc_triangular_expert_test

extern "C" {
decltype(LAPACK_strcon_base) RealStrcon asm("__real_strcon_");
decltype(LAPACK_strcon_base) WrapStrcon asm("__wrap_strcon_");
void WrapStrcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const float* a, const lapack_int* lda,
                float* rcond, float* work, lapack_int* extra, lapack_int* info,
                std::size_t nl, std::size_t ul, std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(0, info, rcond,
                                             static_cast<float*>(nullptr))) {
    RealStrcon(norm, uplo, diag, n, a, lda, rcond, work, extra, info, nl, ul,
               dl);
  }
}
decltype(LAPACK_strrfs_base) RealStrrfs asm("__real_strrfs_");
decltype(LAPACK_strrfs_base) WrapStrrfs asm("__wrap_strrfs_");
void WrapStrrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const float* a,
                const lapack_int* lda, const float* b, const lapack_int* ldb,
                const float* x, const lapack_int* ldx, float* ferr, float* berr,
                float* work, lapack_int* extra, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(1, info, ferr, berr)) {
    RealStrrfs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, x, ldx, ferr, berr,
               work, extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_dtrcon_base) RealDtrcon asm("__real_dtrcon_");
decltype(LAPACK_dtrcon_base) WrapDtrcon asm("__wrap_dtrcon_");
void WrapDtrcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const double* a, const lapack_int* lda,
                double* rcond, double* work, lapack_int* extra,
                lapack_int* info, std::size_t nl, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(2, info, rcond,
                                             static_cast<double*>(nullptr))) {
    RealDtrcon(norm, uplo, diag, n, a, lda, rcond, work, extra, info, nl, ul,
               dl);
  }
}
decltype(LAPACK_dtrrfs_base) RealDtrrfs asm("__real_dtrrfs_");
decltype(LAPACK_dtrrfs_base) WrapDtrrfs asm("__wrap_dtrrfs_");
void WrapDtrrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs, const double* a,
                const lapack_int* lda, const double* b, const lapack_int* ldb,
                const double* x, const lapack_int* ldx, double* ferr,
                double* berr, double* work, lapack_int* extra, lapack_int* info,
                std::size_t ul, std::size_t tl, std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(3, info, ferr, berr)) {
    RealDtrrfs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, x, ldx, ferr, berr,
               work, extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_ctrcon_base) RealCtrcon asm("__real_ctrcon_");
decltype(LAPACK_ctrcon_base) WrapCtrcon asm("__wrap_ctrcon_");
void WrapCtrcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_complex_float* a,
                const lapack_int* lda, float* rcond, lapack_complex_float* work,
                float* extra, lapack_int* info, std::size_t nl, std::size_t ul,
                std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(4, info, rcond,
                                             static_cast<float*>(nullptr))) {
    RealCtrcon(norm, uplo, diag, n, a, lda, rcond, work, extra, info, nl, ul,
               dl);
  }
}
decltype(LAPACK_ctrrfs_base) RealCtrrfs asm("__real_ctrrfs_");
decltype(LAPACK_ctrrfs_base) WrapCtrrfs asm("__wrap_ctrrfs_");
void WrapCtrrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_float* a, const lapack_int* lda,
                const lapack_complex_float* b, const lapack_int* ldb,
                const lapack_complex_float* x, const lapack_int* ldx,
                float* ferr, float* berr, lapack_complex_float* work,
                float* extra, lapack_int* info, std::size_t ul, std::size_t tl,
                std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(5, info, ferr, berr)) {
    RealCtrrfs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, x, ldx, ferr, berr,
               work, extra, info, ul, tl, dl);
  }
}
decltype(LAPACK_ztrcon_base) RealZtrcon asm("__real_ztrcon_");
decltype(LAPACK_ztrcon_base) WrapZtrcon asm("__wrap_ztrcon_");
void WrapZtrcon(const char* norm, const char* uplo, const char* diag,
                const lapack_int* n, const lapack_complex_double* a,
                const lapack_int* lda, double* rcond,
                lapack_complex_double* work, double* extra, lapack_int* info,
                std::size_t nl, std::size_t ul, std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(6, info, rcond,
                                             static_cast<double*>(nullptr))) {
    RealZtrcon(norm, uplo, diag, n, a, lda, rcond, work, extra, info, nl, ul,
               dl);
  }
}
decltype(LAPACK_ztrrfs_base) RealZtrrfs asm("__real_ztrrfs_");
decltype(LAPACK_ztrrfs_base) WrapZtrrfs asm("__wrap_ztrrfs_");
void WrapZtrrfs(const char* uplo, const char* trans, const char* diag,
                const lapack_int* n, const lapack_int* nrhs,
                const lapack_complex_double* a, const lapack_int* lda,
                const lapack_complex_double* b, const lapack_int* ldb,
                const lapack_complex_double* x, const lapack_int* ldx,
                double* ferr, double* berr, lapack_complex_double* work,
                double* extra, lapack_int* info, std::size_t ul, std::size_t tl,
                std::size_t dl) {
  if (!asc_triangular_expert_test::Intercept(7, info, ferr, berr)) {
    RealZtrrfs(uplo, trans, diag, n, nrhs, a, lda, b, ldb, x, ldx, ferr, berr,
               work, extra, info, ul, tl, dl);
  }
}
}  // extern "C"
