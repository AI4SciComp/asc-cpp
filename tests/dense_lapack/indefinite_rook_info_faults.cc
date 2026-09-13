#include "indefinite_rook_info_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_prototypes.h"

namespace {
using asc_indefinite_rook_info_test::Fault;
using asc_indefinite_rook_info_test::Routine;
Routine g_routine = Routine::kTrf;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = std::numeric_limits<std::int64_t>::min();

template <typename Operation>
void Invoke(Routine routine, lapack_int* info, Operation operation) {
  if (routine != g_routine) {
    operation(info);
    return;
  }
  ++g_calls;
  if (g_fault == Fault::kPass) {
    operation(info);
    g_native_info = *info;
    return;
  }
  lapack_int local_info = std::numeric_limits<lapack_int>::min();
  operation(&local_info);
  g_native_info = local_info;
  if (g_fault == Fault::kWrite32BitZero) {
    static_assert(sizeof(lapack_int) >= sizeof(std::uint32_t));
    const std::uint32_t zero = 0;
    // A complete legal INFO write for LP64; an incomplete write for the
    // actually admitted little-endian ILP64 profile.
    std::memcpy(info, &zero, sizeof(zero));
  }
}
}  // namespace

namespace asc_indefinite_rook_info_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
  g_calls = 0;
  g_native_info = std::numeric_limits<std::int64_t>::min();
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
}  // namespace asc_indefinite_rook_info_test

// Test-only GNU ld entry points. All signatures are checked in full against
// the pinned header or the existing compiler-derived private declarations.
// Every numerical operation still runs its actual native routine.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(ssytrf_rook_) __real_ssytrf_rook_;
void __wrap_ssytrf_rook_(const char* uplo, const lapack_int* n, float* a,
                         const lapack_int* lda, lapack_int* pivots, float* work,
                         const lapack_int* lwork, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_ssytrf_rook_(uplo, n, a, lda, pivots, work, lwork, native_info,
                        length);
  });
}
decltype(ssytf2_rook_) __real_ssytf2_rook_;
void __wrap_ssytf2_rook_(char* uplo, lapack_int* n, float* a, lapack_int* lda,
                         lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_ssytf2_rook_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(ssytrs_rook_) __real_ssytrs_rook_;
void __wrap_ssytrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const float* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         float* b, const lapack_int* ldb, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_ssytrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info,
                        length);
  });
}
decltype(dsytrf_rook_) __real_dsytrf_rook_;
void __wrap_dsytrf_rook_(const char* uplo, const lapack_int* n, double* a,
                         const lapack_int* lda, lapack_int* pivots,
                         double* work, const lapack_int* lwork,
                         lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_dsytrf_rook_(uplo, n, a, lda, pivots, work, lwork, native_info,
                        length);
  });
}
decltype(dsytf2_rook_) __real_dsytf2_rook_;
void __wrap_dsytf2_rook_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                         lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_dsytf2_rook_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(dsytrs_rook_) __real_dsytrs_rook_;
void __wrap_dsytrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const double* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         double* b, const lapack_int* ldb, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_dsytrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info,
                        length);
  });
}
decltype(csytrf_rook_) __real_csytrf_rook_;
void __wrap_csytrf_rook_(const char* uplo, const lapack_int* n,
                         lapack_complex_float* a, const lapack_int* lda,
                         lapack_int* pivots, lapack_complex_float* work,
                         const lapack_int* lwork, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_csytrf_rook_(uplo, n, a, lda, pivots, work, lwork, native_info,
                        length);
  });
}
decltype(csytf2_rook_) __real_csytf2_rook_;
void __wrap_csytf2_rook_(char* uplo, lapack_int* n, lapack_complex_float* a,
                         lapack_int* lda, lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_csytf2_rook_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(csytrs_rook_) __real_csytrs_rook_;
void __wrap_csytrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const lapack_complex_float* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         lapack_complex_float* b, const lapack_int* ldb,
                         lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_csytrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info,
                        length);
  });
}
decltype(zsytrf_rook_) __real_zsytrf_rook_;
void __wrap_zsytrf_rook_(const char* uplo, const lapack_int* n,
                         lapack_complex_double* a, const lapack_int* lda,
                         lapack_int* pivots, lapack_complex_double* work,
                         const lapack_int* lwork, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_zsytrf_rook_(uplo, n, a, lda, pivots, work, lwork, native_info,
                        length);
  });
}
decltype(zsytf2_rook_) __real_zsytf2_rook_;
void __wrap_zsytf2_rook_(char* uplo, lapack_int* n, lapack_complex_double* a,
                         lapack_int* lda, lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_zsytf2_rook_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(zsytrs_rook_) __real_zsytrs_rook_;
void __wrap_zsytrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const lapack_complex_double* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         lapack_complex_double* b, const lapack_int* ldb,
                         lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_zsytrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info,
                        length);
  });
}
decltype(chetrf_rook_) __real_chetrf_rook_;
void __wrap_chetrf_rook_(const char* uplo, const lapack_int* n,
                         lapack_complex_float* a, const lapack_int* lda,
                         lapack_int* pivots, lapack_complex_float* work,
                         const lapack_int* lwork, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_chetrf_rook_(uplo, n, a, lda, pivots, work, lwork, native_info,
                        length);
  });
}
decltype(chetf2_rook_) __real_chetf2_rook_;
void __wrap_chetf2_rook_(char* uplo, lapack_int* n, lapack_complex_float* a,
                         lapack_int* lda, lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_chetf2_rook_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(chetrs_rook_) __real_chetrs_rook_;
void __wrap_chetrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const lapack_complex_float* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         lapack_complex_float* b, const lapack_int* ldb,
                         lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_chetrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info,
                        length);
  });
}
decltype(zhetrf_rook_) __real_zhetrf_rook_;
void __wrap_zhetrf_rook_(const char* uplo, const lapack_int* n,
                         lapack_complex_double* a, const lapack_int* lda,
                         lapack_int* pivots, lapack_complex_double* work,
                         const lapack_int* lwork, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_zhetrf_rook_(uplo, n, a, lda, pivots, work, lwork, native_info,
                        length);
  });
}
decltype(zhetf2_rook_) __real_zhetf2_rook_;
void __wrap_zhetf2_rook_(char* uplo, lapack_int* n, lapack_complex_double* a,
                         lapack_int* lda, lapack_int* pivots, lapack_int* info,
                         std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_zhetf2_rook_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(zhetrs_rook_) __real_zhetrs_rook_;
void __wrap_zhetrs_rook_(const char* uplo, const lapack_int* n,
                         const lapack_int* nrhs, const lapack_complex_double* a,
                         const lapack_int* lda, const lapack_int* pivots,
                         lapack_complex_double* b, const lapack_int* ldb,
                         lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_zhetrs_rook_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info,
                        length);
  });
}
}
static_assert(
    std::is_same_v<decltype(&__wrap_ssytrf_rook_), decltype(&ssytrf_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_ssytf2_rook_), decltype(&ssytf2_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_ssytrs_rook_), decltype(&ssytrs_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_dsytrf_rook_), decltype(&dsytrf_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_dsytf2_rook_), decltype(&dsytf2_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_dsytrs_rook_), decltype(&dsytrs_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_csytrf_rook_), decltype(&csytrf_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_csytf2_rook_), decltype(&csytf2_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_csytrs_rook_), decltype(&csytrs_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zsytrf_rook_), decltype(&zsytrf_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zsytf2_rook_), decltype(&zsytf2_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zsytrs_rook_), decltype(&zsytrs_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_chetrf_rook_), decltype(&chetrf_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_chetf2_rook_), decltype(&chetf2_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_chetrs_rook_), decltype(&chetrs_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zhetrf_rook_), decltype(&zhetrf_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zhetf2_rook_), decltype(&zhetf2_rook_)>);
static_assert(
    std::is_same_v<decltype(&__wrap_zhetrs_rook_), decltype(&zhetrs_rook_)>);
// NOLINTEND(bugprone-reserved-identifier)
