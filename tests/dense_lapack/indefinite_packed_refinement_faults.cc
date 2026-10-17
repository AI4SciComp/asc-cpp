#include "indefinite_packed_refinement_faults.h"

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#include "asc/dense/blas.h"
#include "src/dense/lapack/internal_indefinite.h"
namespace asc_packed_refinement_fault_test {
namespace {
Fault g_fault = Fault::kPass;
std::size_t g_calls = 0;
std::int64_t g_native_info = 0;
std::int64_t g_published_info = 0;
std::array<long double, 3> g_native_forward{};
std::array<long double, 3> g_native_backward{};
std::array<long double, 3> g_published_forward{};
std::array<long double, 3> g_published_backward{};
std::array<std::complex<long double>, 195> g_native_x{};
std::array<std::complex<long double>, 195> g_published_x{};
bool g_full_width = false;
bool g_nan_seed = false;
bool g_native_guards = false;

template <typename T>
std::complex<long double> Wide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}
void AlterInfo(lapack_int native, lapack_int* info, const lapack_int* pivots) {
  if (g_fault == Fault::kWrite16BitInfo) {
    const auto partial = static_cast<std::int16_t>(native);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault == Fault::kWrite32BitInfo) {
    const auto partial = static_cast<std::int32_t>(native);
    std::memcpy(info, &partial, sizeof(partial));
  } else if (g_fault != Fault::kOmitInfo) {
    *info = native;
  }
  if (g_fault == Fault::kNegativeInfo) {
    *info = -13;
  }
  if (g_fault == Fault::kPositiveInfo ||
      g_fault == Fault::kInvalidInfoNegativeError) {
    *info = 7;
  }
  if (g_fault == Fault::kMaximumInfo) {
    *info = std::numeric_limits<lapack_int>::max();
  }
  if (g_fault == Fault::kChangedPivot ||
      g_fault == Fault::kChangedPivotNanError) {
    // This is the mutable converted backend copy, never the caller's pivots.
    const_cast<lapack_int*>(pivots)[0] = 0;
  }
  g_published_info = *info;
}
template <typename Real>
void AlterErrors(lapack_int nrhs, const Real* native_forward,
                 const Real* native_backward, Real* forward, Real* backward) {
  for (lapack_int j = 0; j < nrhs; ++j) {
    if (g_fault != Fault::kOmitErrors &&
        (g_fault != Fault::kOmitFirstForward || j != 0)) {
      forward[j] = native_forward[j];
    }
    if (g_fault != Fault::kOmitErrors &&
        (g_fault != Fault::kOmitLastBackward || j != nrhs - 1)) {
      backward[j] = native_backward[j];
    }
    switch (g_fault) {
      case Fault::kZeroErrors:
        forward[j] = backward[j] = Real{0};
        break;
      case Fault::kTwoErrors:
        forward[j] = backward[j] = Real{2};
        break;
      case Fault::kSubnormalErrors:
        forward[j] = backward[j] = std::numeric_limits<Real>::denorm_min();
        break;
      case Fault::kNegativeZeroErrors:
        forward[j] = backward[j] = -Real{0};
        break;
      default:
        break;
    }
  }
  switch (g_fault) {
    case Fault::kNegativeForward:
    case Fault::kInvalidInfoNegativeError:
      forward[0] = Real{-2};
      break;
    case Fault::kNegativeBackward:
      backward[nrhs - 1] = Real{-3};
      break;
    case Fault::kNanForward:
      forward[0] = std::numeric_limits<Real>::quiet_NaN();
      break;
    case Fault::kInfBackward:
      backward[nrhs - 1] = std::numeric_limits<Real>::infinity();
      break;
    case Fault::kNegativeInfForward:
      forward[0] = -std::numeric_limits<Real>::infinity();
      break;
    case Fault::kChangedPivotNanError:
      backward[nrhs - 1] = std::numeric_limits<Real>::quiet_NaN();
      break;
    default:
      break;
  }
  for (lapack_int j = 0; j < nrhs; ++j) {
    g_published_forward[j] = forward[j];
    g_published_backward[j] = backward[j];
  }
}
template <typename T>
void RecordX(lapack_int n, lapack_int nrhs, T* x, lapack_int ldx) {
  for (lapack_int j = 0; j < nrhs; ++j) {
    for (lapack_int i = 0; i < n; ++i) {
      g_native_x[i + j * 65] = Wide(x[i + j * ldx]);
    }
  }
  using Real = asc::DenseBlasRealType<T>;
  if (g_fault == Fault::kNanX) {
    x[n - 1 + (nrhs - 1) * ldx] = T{std::numeric_limits<Real>::quiet_NaN()};
  }
  if (g_fault == Fault::kInfX) {
    x[0] = T{std::numeric_limits<Real>::infinity()};
  }
  for (lapack_int j = 0; j < nrhs; ++j) {
    for (lapack_int i = 0; i < n; ++i) {
      g_published_x[i + j * 65] = Wide(x[i + j * ldx]);
    }
  }
}
template <typename T, typename Operation>
void Invoke(lapack_int n, lapack_int nrhs, const lapack_int* pivots, T* x,
            lapack_int ldx, asc::DenseBlasRealType<T>* forward,
            asc::DenseBlasRealType<T>* backward, lapack_int* info,
            Operation operation) {
  using Real = asc::DenseBlasRealType<T>;
  if (n < 1 || n > 65 || nrhs < 1 || nrhs > 3) {
    std::abort();
  }
  ++g_calls;
  g_full_width = *info == std::numeric_limits<lapack_int>::min();
  g_nan_seed = true;
  for (lapack_int j = 0; j < nrhs; ++j) {
    g_nan_seed =
        g_nan_seed && std::isnan(forward[j]) && std::isnan(backward[j]);
  }
  std::array<Real, 5> native_forward;
  std::array<Real, 5> native_backward;
  native_forward.fill(Real{-401});
  native_backward.fill(Real{-409});
  for (lapack_int j = 0; j < nrhs; ++j) {
    native_forward[j + 1] = native_backward[j + 1] =
        std::numeric_limits<Real>::quiet_NaN();
  }
  lapack_int native_info = std::numeric_limits<lapack_int>::min();
  operation(&native_info, native_forward.data() + 1,
            native_backward.data() + 1);
  g_native_info = native_info;
  g_native_guards = true;
  for (std::size_t j = 0; j < native_forward.size(); ++j) {
    if (j == 0 || j > static_cast<std::size_t>(nrhs)) {
      g_native_guards = g_native_guards && native_forward[j] == Real{-401} &&
                        native_backward[j] == Real{-409};
    }
  }
  for (lapack_int j = 0; j < nrhs; ++j) {
    g_native_forward[j] = native_forward[j + 1];
    g_native_backward[j] = native_backward[j + 1];
  }
  RecordX(n, nrhs, x, ldx);
  AlterInfo(native_info, info, pivots);
  AlterErrors(nrhs, native_forward.data() + 1, native_backward.data() + 1,
              forward, backward);
}
}  // namespace
void SetFault(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_full_width = false;
  g_nan_seed = false;
  g_native_guards = false;
}
std::size_t Calls() { return g_calls; }
std::int64_t LastNativeInfo() { return g_native_info; }
std::int64_t LastPublishedInfo() { return g_published_info; }
long double LastNativeForward(int j) { return g_native_forward[j]; }
long double LastNativeBackward(int j) { return g_native_backward[j]; }
long double LastPublishedForward(int j) { return g_published_forward[j]; }
long double LastPublishedBackward(int j) { return g_published_backward[j]; }
std::complex<long double> LastNativeX(int i, int j) {
  return g_native_x[i + j * 65];
}
std::complex<long double> LastPublishedX(int i, int j) {
  return g_published_x[i + j * 65];
}
bool SeedWasFullWidth() { return g_full_width; }
bool ErrorSeedsWereNan() { return g_nan_seed; }
bool NativeGuardsPass() { return g_native_guards; }
}  // namespace asc_packed_refinement_fault_test
using asc_packed_refinement_fault_test::Invoke;
// GNU ld --wrap requires these exact external spellings.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
decltype(LAPACK_ssprfs_base) __real_ssprfs_;
void __wrap_ssprfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const float* a,
                    const float* factors, const lapack_int* pivots,
                    const float* b, const lapack_int* ldb, float* x,
                    const lapack_int* ldx, float* forward, float* backward,
                    float* work, lapack_int* iwork, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, *nrhs, pivots, x, *ldx, forward, backward, info,
         [&](lapack_int* native_info, float* native_forward,
             float* native_backward) {
           __real_ssprfs_(uplo, n, nrhs, a, factors, pivots, b, ldb, x, ldx,
                          native_forward, native_backward, work, iwork,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_ssprfs_base), decltype(__wrap_ssprfs_)>);
decltype(LAPACK_dsprfs_base) __real_dsprfs_;
void __wrap_dsprfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const double* factors, const lapack_int* pivots,
                    const double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* forward, double* backward,
                    double* work, lapack_int* iwork, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, *nrhs, pivots, x, *ldx, forward, backward, info,
         [&](lapack_int* native_info, double* native_forward,
             double* native_backward) {
           __real_dsprfs_(uplo, n, nrhs, a, factors, pivots, b, ldb, x, ldx,
                          native_forward, native_backward, work, iwork,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_dsprfs_base), decltype(__wrap_dsprfs_)>);
decltype(LAPACK_csprfs_base) __real_csprfs_;
void __wrap_csprfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_float* a,
                    const lapack_complex_float* factors,
                    const lapack_int* pivots, const lapack_complex_float* b,
                    const lapack_int* ldb, lapack_complex_float* x,
                    const lapack_int* ldx, float* forward, float* backward,
                    lapack_complex_float* work, float* rwork, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, *nrhs, pivots, x, *ldx, forward, backward, info,
         [&](lapack_int* native_info, float* native_forward,
             float* native_backward) {
           __real_csprfs_(uplo, n, nrhs, a, factors, pivots, b, ldb, x, ldx,
                          native_forward, native_backward, work, rwork,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_csprfs_base), decltype(__wrap_csprfs_)>);
decltype(LAPACK_zsprfs_base) __real_zsprfs_;
void __wrap_zsprfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_double* a,
                    const lapack_complex_double* factors,
                    const lapack_int* pivots, const lapack_complex_double* b,
                    const lapack_int* ldb, lapack_complex_double* x,
                    const lapack_int* ldx, double* forward, double* backward,
                    lapack_complex_double* work, double* rwork,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, *nrhs, pivots, x, *ldx, forward, backward, info,
         [&](lapack_int* native_info, double* native_forward,
             double* native_backward) {
           __real_zsprfs_(uplo, n, nrhs, a, factors, pivots, b, ldb, x, ldx,
                          native_forward, native_backward, work, rwork,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zsprfs_base), decltype(__wrap_zsprfs_)>);
decltype(LAPACK_chprfs_base) __real_chprfs_;
void __wrap_chprfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_float* a,
                    const lapack_complex_float* factors,
                    const lapack_int* pivots, const lapack_complex_float* b,
                    const lapack_int* ldb, lapack_complex_float* x,
                    const lapack_int* ldx, float* forward, float* backward,
                    lapack_complex_float* work, float* rwork, lapack_int* info,
                    std::size_t length) {
  Invoke(*n, *nrhs, pivots, x, *ldx, forward, backward, info,
         [&](lapack_int* native_info, float* native_forward,
             float* native_backward) {
           __real_chprfs_(uplo, n, nrhs, a, factors, pivots, b, ldb, x, ldx,
                          native_forward, native_backward, work, rwork,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_chprfs_base), decltype(__wrap_chprfs_)>);
decltype(LAPACK_zhprfs_base) __real_zhprfs_;
void __wrap_zhprfs_(const char* uplo, const lapack_int* n,
                    const lapack_int* nrhs, const lapack_complex_double* a,
                    const lapack_complex_double* factors,
                    const lapack_int* pivots, const lapack_complex_double* b,
                    const lapack_int* ldb, lapack_complex_double* x,
                    const lapack_int* ldx, double* forward, double* backward,
                    lapack_complex_double* work, double* rwork,
                    lapack_int* info, std::size_t length) {
  Invoke(*n, *nrhs, pivots, x, *ldx, forward, backward, info,
         [&](lapack_int* native_info, double* native_forward,
             double* native_backward) {
           __real_zhprfs_(uplo, n, nrhs, a, factors, pivots, b, ldb, x, ldx,
                          native_forward, native_backward, work, rwork,
                          native_info, length);
         });
}
static_assert(
    std::is_same_v<decltype(LAPACK_zhprfs_base), decltype(__wrap_zhprfs_)>);
}
// NOLINTEND(bugprone-reserved-identifier)
