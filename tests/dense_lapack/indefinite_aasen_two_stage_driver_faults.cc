#include "indefinite_aasen_two_stage_driver_faults.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
namespace {
using asc_aasen_two_stage_driver_fault_test::Fault;
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_info = 0;
void Pivots(lapack_int n, lapack_int* destination, const lapack_int* source,
            Fault omitted, Fault partial) {
  for (lapack_int i = 0; i < n; ++i) {
    if (g_fault == partial) {
      const auto value = static_cast<std::int32_t>(source[i]);
      std::memcpy(destination + i, &value, sizeof(value));
    } else if (g_fault != omitted) {
      destination[i] = source[i];
    }
  }
}
template <typename T>
void ScalarFaults(T* tb, lapack_int last, T old_nb, T old_witness) {
  if (g_fault == Fault::kOmitNb) {
    tb[0] = old_nb;
  }
  if (g_fault == Fault::kNanNb) {
    tb[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  }
  if (g_fault == Fault::kZeroNb) {
    tb[0] = T{};
  }
  if (g_fault == Fault::kWrongNb) {
    tb[0] += T{1};
  }
  if (g_fault == Fault::kRealOnlyNb) {
    if constexpr (asc::DenseBlasComplex<T>) {
      tb[0].imag(old_nb.imag());
    }
  }
  if (g_fault == Fault::kNonzeroWitness) {
    tb[last] = T{1};
  }
  if (g_fault == Fault::kNanWitness) {
    tb[last] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  }
  if (g_fault == Fault::kOmitWitness) {
    tb[last] = old_witness;
  }
  if (g_fault == Fault::kRealOnlyWitness) {
    if constexpr (asc::DenseBlasComplex<T>) {
      tb[last].imag(old_witness.imag());
    }
  }
}
template <typename T, typename Operation>
void Invoke(lapack_int n, T* tb, lapack_int ltb, T* work, lapack_int lwork,
            lapack_int* pivots, lapack_int* band_pivots, lapack_int* info,
            Operation operation) {
  assert(n == 1 || n == 3);
  ++g_calls;
  const auto nb = std::min<lapack_int>({192, (ltb / n - 1) / 3, lwork / n});
  const auto last = (n - 1) * (ltb / n) + 2 * nb;
  const T old_work = work[0];
  const T old_nb = tb[0];
  const T old_witness = tb[last];
  std::array<lapack_int, 3> p{};
  std::array<lapack_int, 3> q{};
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  operation(p.data(), q.data(), &native_info);
  g_info = native_info;
  if (g_fault == Fault::kWrite32BitInfo) {
    const auto partial = static_cast<std::int32_t>(native_info);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native_info;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -7;
  }
  if (g_fault == Fault::kOutOfRangeInfo) {
    *info = n + 1;
  }
  if (g_fault == Fault::kFalseSingular) {
    *info = n;
  }
  Pivots(n, pivots, p.data(), Fault::kOmitOuter, Fault::kWrite32BitOuter);
  Pivots(n, band_pivots, q.data(), Fault::kOmitBand, Fault::kWrite32BitBand);
  if (g_fault == Fault::kZeroOuter) {
    pivots[0] = 0;
  }
  if (g_fault == Fault::kNegativeOuter) {
    pivots[0] = -1;
  }
  if (g_fault == Fault::kOutOfRangeOuter) {
    pivots[n - 1] = n + 1;
  }
  if (g_fault == Fault::kWrongPrefixOuter) {
    pivots[0] = 2;
  }
  if (g_fault == Fault::kBackwardOuter) {
    pivots[n - 1] = n - 1;
  }
  if (g_fault == Fault::kZeroBand) {
    band_pivots[0] = 0;
  }
  if (g_fault == Fault::kNegativeBand) {
    band_pivots[0] = -1;
  }
  if (g_fault == Fault::kOutOfRangeBand) {
    band_pivots[n - 1] = n + 1;
  }
  if (g_fault == Fault::kBackwardBand) {
    band_pivots[n - 1] = n - 1;
  }
  if (g_fault == Fault::kOutsideBandReach) {
    band_pivots[0] = std::min(n, nb + 1) + 1;
  }
  ScalarFaults(tb, last, old_nb, old_witness);
  if (g_fault == Fault::kOmitWork) {
    work[0] = old_work;
  }
  if (g_fault == Fault::kNanWork) {
    work[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
  }
  if (g_fault == Fault::kZeroWork) {
    work[0] = T{};
  }
  if (g_fault == Fault::kWrongWork) {
    work[0] += T{1};
  }
  if (g_fault == Fault::kRealOnlyWork) {
    if constexpr (asc::DenseBlasComplex<T>) {
      work[0].imag(old_work.imag());
    }
  }
}
}  // namespace
namespace asc_aasen_two_stage_driver_fault_test {
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_info = 0;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_info; }
bool SingularInput(Fault fault) {
  return fault == Fault::kRealSingular ||
         (fault >= Fault::kNonzeroWitness && fault <= Fault::kRealOnlyWitness);
}
}  // namespace asc_aasen_two_stage_driver_fault_test
// Output injection follows a real call to the exact inventoried driver.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssysv_aa_2stage_base) __real_ssysv_aa_2stage_;
void __wrap_ssysv_aa_2stage_(
    const char* tri, const lapack_int* n, const lapack_int* nrhs, float* a,
    const lapack_int* lda, float* tb, const lapack_int* ltb, lapack_int* pivots,
    lapack_int* band_pivots, float* b, const lapack_int* ldb, float* work,
    const lapack_int* lwork, lapack_int* info, std::size_t length) {
  Invoke(*n, tb, *ltb, work, *lwork, pivots, band_pivots, info,
         [&](lapack_int* p, lapack_int* q, lapack_int* native_info) {
           __real_ssysv_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                                   work, lwork, native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_ssysv_aa_2stage_base),
                             decltype(__wrap_ssysv_aa_2stage_)>);
decltype(LAPACK_dsysv_aa_2stage_base) __real_dsysv_aa_2stage_;
void __wrap_dsysv_aa_2stage_(const char* tri, const lapack_int* n,
                             const lapack_int* nrhs, double* a,
                             const lapack_int* lda, double* tb,
                             const lapack_int* ltb, lapack_int* pivots,
                             lapack_int* band_pivots, double* b,
                             const lapack_int* ldb, double* work,
                             const lapack_int* lwork, lapack_int* info,
                             std::size_t length) {
  Invoke(*n, tb, *ltb, work, *lwork, pivots, band_pivots, info,
         [&](lapack_int* p, lapack_int* q, lapack_int* native_info) {
           __real_dsysv_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                                   work, lwork, native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_dsysv_aa_2stage_base),
                             decltype(__wrap_dsysv_aa_2stage_)>);
decltype(LAPACK_csysv_aa_2stage_base) __real_csysv_aa_2stage_;
void __wrap_csysv_aa_2stage_(const char* tri, const lapack_int* n,
                             const lapack_int* nrhs, lapack_complex_float* a,
                             const lapack_int* lda, lapack_complex_float* tb,
                             const lapack_int* ltb, lapack_int* pivots,
                             lapack_int* band_pivots, lapack_complex_float* b,
                             const lapack_int* ldb, lapack_complex_float* work,
                             const lapack_int* lwork, lapack_int* info,
                             std::size_t length) {
  Invoke(*n, tb, *ltb, work, *lwork, pivots, band_pivots, info,
         [&](lapack_int* p, lapack_int* q, lapack_int* native_info) {
           __real_csysv_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                                   work, lwork, native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_csysv_aa_2stage_base),
                             decltype(__wrap_csysv_aa_2stage_)>);
decltype(LAPACK_zsysv_aa_2stage_base) __real_zsysv_aa_2stage_;
void __wrap_zsysv_aa_2stage_(const char* tri, const lapack_int* n,
                             const lapack_int* nrhs, lapack_complex_double* a,
                             const lapack_int* lda, lapack_complex_double* tb,
                             const lapack_int* ltb, lapack_int* pivots,
                             lapack_int* band_pivots, lapack_complex_double* b,
                             const lapack_int* ldb, lapack_complex_double* work,
                             const lapack_int* lwork, lapack_int* info,
                             std::size_t length) {
  Invoke(*n, tb, *ltb, work, *lwork, pivots, band_pivots, info,
         [&](lapack_int* p, lapack_int* q, lapack_int* native_info) {
           __real_zsysv_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                                   work, lwork, native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_zsysv_aa_2stage_base),
                             decltype(__wrap_zsysv_aa_2stage_)>);
decltype(LAPACK_chesv_aa_2stage_base) __real_chesv_aa_2stage_;
void __wrap_chesv_aa_2stage_(const char* tri, const lapack_int* n,
                             const lapack_int* nrhs, lapack_complex_float* a,
                             const lapack_int* lda, lapack_complex_float* tb,
                             const lapack_int* ltb, lapack_int* pivots,
                             lapack_int* band_pivots, lapack_complex_float* b,
                             const lapack_int* ldb, lapack_complex_float* work,
                             const lapack_int* lwork, lapack_int* info,
                             std::size_t length) {
  Invoke(*n, tb, *ltb, work, *lwork, pivots, band_pivots, info,
         [&](lapack_int* p, lapack_int* q, lapack_int* native_info) {
           __real_chesv_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                                   work, lwork, native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_chesv_aa_2stage_base),
                             decltype(__wrap_chesv_aa_2stage_)>);
decltype(LAPACK_zhesv_aa_2stage_base) __real_zhesv_aa_2stage_;
void __wrap_zhesv_aa_2stage_(const char* tri, const lapack_int* n,
                             const lapack_int* nrhs, lapack_complex_double* a,
                             const lapack_int* lda, lapack_complex_double* tb,
                             const lapack_int* ltb, lapack_int* pivots,
                             lapack_int* band_pivots, lapack_complex_double* b,
                             const lapack_int* ldb, lapack_complex_double* work,
                             const lapack_int* lwork, lapack_int* info,
                             std::size_t length) {
  Invoke(*n, tb, *ltb, work, *lwork, pivots, band_pivots, info,
         [&](lapack_int* p, lapack_int* q, lapack_int* native_info) {
           __real_zhesv_aa_2stage_(tri, n, nrhs, a, lda, tb, ltb, p, q, b, ldb,
                                   work, lwork, native_info, length);
         });
}
static_assert(std::is_same_v<decltype(LAPACK_zhesv_aa_2stage_base),
                             decltype(__wrap_zhesv_aa_2stage_)>);
}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier)
