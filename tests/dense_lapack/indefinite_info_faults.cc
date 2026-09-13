#include "indefinite_info_faults.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_prototypes.h"

namespace {
using asc_indefinite_info_test::Fault;
using asc_indefinite_info_test::Routine;
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

namespace asc_indefinite_info_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
  g_calls = 0;
  g_native_info = std::numeric_limits<std::int64_t>::min();
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
}  // namespace asc_indefinite_info_test

// Test-only GNU ld entry points. All signatures are checked in full against
// the pinned header or the existing compiler-derived private declarations.
// Every numerical operation still runs its actual native routine.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(ssytrf_) __real_ssytrf_;
void __wrap_ssytrf_(const char* uplo, const lapack_int* n, float* a,
                    const lapack_int* lda, lapack_int* pivots, float* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_ssytrf_(uplo, n, a, lda, pivots, work, lwork, native_info, length);
  });
}
decltype(ssytf2_) __real_ssytf2_;
void __wrap_ssytf2_(char* uplo, lapack_int* n, float* a, lapack_int* lda,
                    lapack_int* pivots, lapack_int* info, std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_ssytf2_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(ssytrs_) __real_ssytrs_;
void __wrap_ssytrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const lapack_int* lda, const lapack_int* pivots, float* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_ssytrs_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info, length);
  });
}
decltype(dsytrf_) __real_dsytrf_;
void __wrap_dsytrf_(const char* uplo, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots, double* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_dsytrf_(uplo, n, a, lda, pivots, work, lwork, native_info, length);
  });
}
decltype(dsytf2_) __real_dsytf2_;
void __wrap_dsytf2_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                    lapack_int* pivots, lapack_int* info, std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_dsytf2_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(dsytrs_) __real_dsytrs_;
void __wrap_dsytrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const lapack_int* pivots, double* b,
                    const lapack_int* ldb, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_dsytrs_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info, length);
  });
}
decltype(csytrf_) __real_csytrf_;
void __wrap_csytrf_(const char* uplo, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_float* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_csytrf_(uplo, n, a, lda, pivots, work, lwork, native_info, length);
  });
}
decltype(csytf2_) __real_csytf2_;
void __wrap_csytf2_(char* uplo, lapack_int* n, lapack_complex_float* a,
                    lapack_int* lda, lapack_int* pivots, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_csytf2_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(csytrs_) __real_csytrs_;
void __wrap_csytrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_float* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    lapack_complex_float* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_csytrs_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info, length);
  });
}
decltype(zsytrf_) __real_zsytrf_;
void __wrap_zsytrf_(const char* uplo, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_double* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_zsytrf_(uplo, n, a, lda, pivots, work, lwork, native_info, length);
  });
}
decltype(zsytf2_) __real_zsytf2_;
void __wrap_zsytf2_(char* uplo, lapack_int* n, lapack_complex_double* a,
                    lapack_int* lda, lapack_int* pivots, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_zsytf2_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(zsytrs_) __real_zsytrs_;
void __wrap_zsytrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_double* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    lapack_complex_double* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_zsytrs_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info, length);
  });
}
decltype(chetrf_) __real_chetrf_;
void __wrap_chetrf_(const char* uplo, const lapack_int* n,
                    lapack_complex_float* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_float* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_chetrf_(uplo, n, a, lda, pivots, work, lwork, native_info, length);
  });
}
decltype(chetf2_) __real_chetf2_;
void __wrap_chetf2_(char* uplo, lapack_int* n, lapack_complex_float* a,
                    lapack_int* lda, lapack_int* pivots, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_chetf2_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(chetrs_) __real_chetrs_;
void __wrap_chetrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_float* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    lapack_complex_float* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_chetrs_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info, length);
  });
}
decltype(zhetrf_) __real_zhetrf_;
void __wrap_zhetrf_(const char* uplo, const lapack_int* n,
                    lapack_complex_double* a, const lapack_int* lda,
                    lapack_int* pivots, lapack_complex_double* work,
                    const lapack_int* lwork, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTrf, info, [&](lapack_int* native_info) {
    __real_zhetrf_(uplo, n, a, lda, pivots, work, lwork, native_info, length);
  });
}
decltype(zhetf2_) __real_zhetf2_;
void __wrap_zhetf2_(char* uplo, lapack_int* n, lapack_complex_double* a,
                    lapack_int* lda, lapack_int* pivots, lapack_int* info,
                    std::size_t length) {
  Invoke(Routine::kTf2, info, [&](lapack_int* native_info) {
    __real_zhetf2_(uplo, n, a, lda, pivots, native_info, length);
  });
}
decltype(zhetrs_) __real_zhetrs_;
void __wrap_zhetrs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_double* a,
                    const lapack_int* lda, const lapack_int* pivots,
                    lapack_complex_double* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  Invoke(Routine::kTrs, info, [&](lapack_int* native_info) {
    __real_zhetrs_(uplo, n, nrhs, a, lda, pivots, b, ldb, native_info, length);
  });
}
}
static_assert(std::is_same_v<decltype(&__wrap_ssytrf_), decltype(&ssytrf_)>);
static_assert(std::is_same_v<decltype(&__wrap_ssytf2_), decltype(&ssytf2_)>);
static_assert(std::is_same_v<decltype(&__wrap_ssytrs_), decltype(&ssytrs_)>);
static_assert(std::is_same_v<decltype(&__wrap_dsytrf_), decltype(&dsytrf_)>);
static_assert(std::is_same_v<decltype(&__wrap_dsytf2_), decltype(&dsytf2_)>);
static_assert(std::is_same_v<decltype(&__wrap_dsytrs_), decltype(&dsytrs_)>);
static_assert(std::is_same_v<decltype(&__wrap_csytrf_), decltype(&csytrf_)>);
static_assert(std::is_same_v<decltype(&__wrap_csytf2_), decltype(&csytf2_)>);
static_assert(std::is_same_v<decltype(&__wrap_csytrs_), decltype(&csytrs_)>);
static_assert(std::is_same_v<decltype(&__wrap_zsytrf_), decltype(&zsytrf_)>);
static_assert(std::is_same_v<decltype(&__wrap_zsytf2_), decltype(&zsytf2_)>);
static_assert(std::is_same_v<decltype(&__wrap_zsytrs_), decltype(&zsytrs_)>);
static_assert(std::is_same_v<decltype(&__wrap_chetrf_), decltype(&chetrf_)>);
static_assert(std::is_same_v<decltype(&__wrap_chetf2_), decltype(&chetf2_)>);
static_assert(std::is_same_v<decltype(&__wrap_chetrs_), decltype(&chetrs_)>);
static_assert(std::is_same_v<decltype(&__wrap_zhetrf_), decltype(&zhetrf_)>);
static_assert(std::is_same_v<decltype(&__wrap_zhetf2_), decltype(&zhetf2_)>);
static_assert(std::is_same_v<decltype(&__wrap_zhetrs_), decltype(&zhetrs_)>);
// NOLINTEND(bugprone-reserved-identifier)
