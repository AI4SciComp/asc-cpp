#include "indefinite_rk_inverse_faults.h"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rk_inverse_prototypes.h"
#include "asc/dense/blas.h"
namespace asc_rk_inverse_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
bool g_full_width = false;
bool g_active = false;
template <typename T>
T Seed() {
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{-1, -1};
  } else {
    return T{-1};
  }
}
template <typename T>
void AlterWork(T* work, T seed) {
  static_assert(std::is_trivially_copyable_v<T>);
  // Deliberately emulate a provider writing only part of a live scalar object.
  const T native = *work;
  if (g_fault == Fault::kOmitWork) {
    *work = seed;
  }
  if (g_fault == Fault::kWrite16BitWork) {
    *work = seed;
    std::memcpy(static_cast<void*>(work), static_cast<const void*>(&native),
                sizeof(std::uint16_t));
  }
  if (g_fault == Fault::kWrite32BitWork) {
    *work = seed;
    std::memcpy(static_cast<void*>(work), static_cast<const void*>(&native),
                sizeof(std::uint32_t));
  }
  if (g_fault == Fault::kWrongWork) {
    *work = T{-7};
  }
  if (g_fault == Fault::kNanWork) {
    *work = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  }
  if (g_fault == Fault::kImagWork) {
    if constexpr (asc::DenseBlasComplex<T>) {
      work->imag(1);
    } else {
      *work += 1;
    }
  }
}
template <typename T, typename Operation>
void Invoke(lapack_int n, const lapack_int* pivots, T* work,
            bool explicit_block, lapack_int* info, Operation operation) {
  if (g_active) {
    operation(info);
    return;
  }
  ++g_calls;
  const T seed = *work;
  g_full_width = *info == std::numeric_limits<lapack_int>::min() &&
                 (explicit_block || seed == Seed<T>());
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  g_active = true;
  operation(&native_info);
  g_active = false;
  g_native_info = native_info;
  if (g_fault == Fault::kWrite16BitInfo) {
    const auto part = static_cast<std::int16_t>(native_info);
    std::memcpy(info, &part, sizeof(part));
  } else if (g_fault == Fault::kWrite32BitInfo) {
    const auto part = static_cast<std::int32_t>(native_info);
    std::memcpy(info, &part, sizeof(part));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -4;
  }
  if (g_fault == Fault::kOutOfRangeInfo) {
    *info = n + 1;
  }
  if (g_fault == Fault::kContradictoryInfo) {
    *info = native_info == 0 ? 1 : 0;
  }
  if (g_fault == Fault::kWrongIndex) {
    *info = native_info == 1 ? n + 1 : 1;
  }
  if (g_fault == Fault::kChangedPivot) {
    const_cast<lapack_int*>(pivots)[0] = 0;
  }
  AlterWork(work, seed);
  g_published_info = *info;
}
}  // namespace
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_full_width = false;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
std::int64_t LastPublishedInfo() { return g_published_info; }
bool SeedWasFullWidth() { return g_full_width; }
}  // namespace asc_rk_inverse_fault_test
using asc_rk_inverse_fault_test::Invoke;
// GNU ld --wrap requires the exact reserved external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssytri_3_base) __real_ssytri_3_;
void __wrap_ssytri_3_(const char* uplo, const lapack_int* n, float* a,
                      const lapack_int* lda, const float* e,
                      const lapack_int* pivots, float* work,
                      const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, work, false, info, [&](lapack_int* native_info) {
    __real_ssytri_3_(uplo, n, a, lda, e, pivots, work, size, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssytri_3_base), decltype(__wrap_ssytri_3_)>);
decltype(LAPACK_dsytri_3_base) __real_dsytri_3_;
void __wrap_dsytri_3_(const char* uplo, const lapack_int* n, double* a,
                      const lapack_int* lda, const double* e,
                      const lapack_int* pivots, double* work,
                      const lapack_int* size, lapack_int* info,
                      std::size_t length) {
  Invoke(*n, pivots, work, false, info, [&](lapack_int* native_info) {
    __real_dsytri_3_(uplo, n, a, lda, e, pivots, work, size, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsytri_3_base), decltype(__wrap_dsytri_3_)>);
decltype(LAPACK_csytri_3_base) __real_csytri_3_;
void __wrap_csytri_3_(const char* uplo, const lapack_int* n,
                      std::complex<float>* a, const lapack_int* lda,
                      const std::complex<float>* e, const lapack_int* pivots,
                      std::complex<float>* work, const lapack_int* size,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, false, info, [&](lapack_int* native_info) {
    __real_csytri_3_(uplo, n, a, lda, e, pivots, work, size, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csytri_3_base), decltype(__wrap_csytri_3_)>);
decltype(LAPACK_zsytri_3_base) __real_zsytri_3_;
void __wrap_zsytri_3_(const char* uplo, const lapack_int* n,
                      std::complex<double>* a, const lapack_int* lda,
                      const std::complex<double>* e, const lapack_int* pivots,
                      std::complex<double>* work, const lapack_int* size,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, false, info, [&](lapack_int* native_info) {
    __real_zsytri_3_(uplo, n, a, lda, e, pivots, work, size, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsytri_3_base), decltype(__wrap_zsytri_3_)>);
decltype(LAPACK_chetri_3_base) __real_chetri_3_;
void __wrap_chetri_3_(const char* uplo, const lapack_int* n,
                      std::complex<float>* a, const lapack_int* lda,
                      const std::complex<float>* e, const lapack_int* pivots,
                      std::complex<float>* work, const lapack_int* size,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, false, info, [&](lapack_int* native_info) {
    __real_chetri_3_(uplo, n, a, lda, e, pivots, work, size, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chetri_3_base), decltype(__wrap_chetri_3_)>);
decltype(LAPACK_zhetri_3_base) __real_zhetri_3_;
void __wrap_zhetri_3_(const char* uplo, const lapack_int* n,
                      std::complex<double>* a, const lapack_int* lda,
                      const std::complex<double>* e, const lapack_int* pivots,
                      std::complex<double>* work, const lapack_int* size,
                      lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, false, info, [&](lapack_int* native_info) {
    __real_zhetri_3_(uplo, n, a, lda, e, pivots, work, size, native_info,
                     length);
  });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhetri_3_base), decltype(__wrap_zhetri_3_)>);
decltype(ssytri_3x_) __real_ssytri_3x_;
void __wrap_ssytri_3x_(char* uplo, lapack_int* n, float* a, lapack_int* lda,
                       float* e, lapack_int* pivots, float* work,
                       lapack_int* size, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, true, info, [&](lapack_int* native_info) {
    __real_ssytri_3x_(uplo, n, a, lda, e, pivots, work, size, native_info,
                      length);
  });
}
static_assert(
    std::is_same_v<decltype(ssytri_3x_), decltype(__wrap_ssytri_3x_)>);
decltype(dsytri_3x_) __real_dsytri_3x_;
void __wrap_dsytri_3x_(char* uplo, lapack_int* n, double* a, lapack_int* lda,
                       double* e, lapack_int* pivots, double* work,
                       lapack_int* size, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, true, info, [&](lapack_int* native_info) {
    __real_dsytri_3x_(uplo, n, a, lda, e, pivots, work, size, native_info,
                      length);
  });
}
static_assert(
    std::is_same_v<decltype(dsytri_3x_), decltype(__wrap_dsytri_3x_)>);
decltype(csytri_3x_) __real_csytri_3x_;
void __wrap_csytri_3x_(char* uplo, lapack_int* n, std::complex<float>* a,
                       lapack_int* lda, std::complex<float>* e,
                       lapack_int* pivots, std::complex<float>* work,
                       lapack_int* size, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, true, info, [&](lapack_int* native_info) {
    __real_csytri_3x_(uplo, n, a, lda, e, pivots, work, size, native_info,
                      length);
  });
}
static_assert(
    std::is_same_v<decltype(csytri_3x_), decltype(__wrap_csytri_3x_)>);
decltype(zsytri_3x_) __real_zsytri_3x_;
void __wrap_zsytri_3x_(char* uplo, lapack_int* n, std::complex<double>* a,
                       lapack_int* lda, std::complex<double>* e,
                       lapack_int* pivots, std::complex<double>* work,
                       lapack_int* size, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, true, info, [&](lapack_int* native_info) {
    __real_zsytri_3x_(uplo, n, a, lda, e, pivots, work, size, native_info,
                      length);
  });
}
static_assert(
    std::is_same_v<decltype(zsytri_3x_), decltype(__wrap_zsytri_3x_)>);
decltype(chetri_3x_) __real_chetri_3x_;
void __wrap_chetri_3x_(char* uplo, lapack_int* n, std::complex<float>* a,
                       lapack_int* lda, std::complex<float>* e,
                       lapack_int* pivots, std::complex<float>* work,
                       lapack_int* size, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, true, info, [&](lapack_int* native_info) {
    __real_chetri_3x_(uplo, n, a, lda, e, pivots, work, size, native_info,
                      length);
  });
}
static_assert(
    std::is_same_v<decltype(chetri_3x_), decltype(__wrap_chetri_3x_)>);
decltype(zhetri_3x_) __real_zhetri_3x_;
void __wrap_zhetri_3x_(char* uplo, lapack_int* n, std::complex<double>* a,
                       lapack_int* lda, std::complex<double>* e,
                       lapack_int* pivots, std::complex<double>* work,
                       lapack_int* size, lapack_int* info, std::size_t length) {
  Invoke(*n, pivots, work, true, info, [&](lapack_int* native_info) {
    __real_zhetri_3x_(uplo, n, a, lda, e, pivots, work, size, native_info,
                      length);
  });
}
static_assert(
    std::is_same_v<decltype(zhetri_3x_), decltype(__wrap_zhetri_3x_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
