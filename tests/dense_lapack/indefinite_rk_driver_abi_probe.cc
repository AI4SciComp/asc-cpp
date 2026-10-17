#if defined(ASC_RK_DRIVER_EMITTED_PROTOTYPES)
// Original compiler emissions are renamed only inside this probe.
#include ASC_RK_DRIVER_EMITTED_SSYSV_RK_HEADER
#include ASC_RK_DRIVER_EMITTED_DSYSV_RK_HEADER
#include ASC_RK_DRIVER_EMITTED_CSYSV_RK_HEADER
#include ASC_RK_DRIVER_EMITTED_ZSYSV_RK_HEADER
#include ASC_RK_DRIVER_EMITTED_CHESV_RK_HEADER
#include ASC_RK_DRIVER_EMITTED_ZHESV_RK_HEADER
#undef ssysv_rk_
#undef dsysv_rk_
#undef csysv_rk_
#undef zsysv_rk_
#undef chesv_rk_
#undef zhesv_rk_
#endif
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_RK_DRIVER_EMITTED_PROTOTYPES)
#include <type_traits>
namespace {
// LAPACK's C header adds const to read-only input pointees. Erase only that
// qualifier; scalar types, pointer levels, argument order and hidden CHARACTER
// length type must still match the actual GNU emissions exactly.
template <typename T>
struct AbiArgument {
  using Type = T;
};
template <typename T>
struct AbiArgument<T*> {
  using Type = std::remove_const_t<T>*;
};
template <typename T>
struct AbiSignature;
template <typename R, typename... Args>
struct AbiSignature<R(Args...)> {
  using Type = R(typename AbiArgument<Args>::Type...);
};
static_assert(
    std::is_same_v<
        typename AbiSignature<decltype(LAPACK_ssysv_rk_base)>::Type,
        typename AbiSignature<decltype(asc_probe_emitted_ssysv_rk)>::Type>);
static_assert(
    std::is_same_v<
        typename AbiSignature<decltype(LAPACK_dsysv_rk_base)>::Type,
        typename AbiSignature<decltype(asc_probe_emitted_dsysv_rk)>::Type>);
static_assert(
    std::is_same_v<
        typename AbiSignature<decltype(LAPACK_csysv_rk_base)>::Type,
        typename AbiSignature<decltype(asc_probe_emitted_csysv_rk)>::Type>);
static_assert(
    std::is_same_v<
        typename AbiSignature<decltype(LAPACK_zsysv_rk_base)>::Type,
        typename AbiSignature<decltype(asc_probe_emitted_zsysv_rk)>::Type>);
static_assert(
    std::is_same_v<
        typename AbiSignature<decltype(LAPACK_chesv_rk_base)>::Type,
        typename AbiSignature<decltype(asc_probe_emitted_chesv_rk)>::Type>);
static_assert(
    std::is_same_v<
        typename AbiSignature<decltype(LAPACK_zhesv_rk_base)>::Type,
        typename AbiSignature<decltype(asc_probe_emitted_zhesv_rk)>::Type>);
}  // namespace
#endif
namespace {
void Call([[maybe_unused]] bool hermitian, const char* uplo,
          const lapack_int* n, const lapack_int* nrhs, float* a,
          const lapack_int* lda, float* extra, lapack_int* pivots, float* b,
          const lapack_int* ldb, float* work, const lapack_int* lwork,
          lapack_int* info) {
  LAPACK_ssysv_rk(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, work, lwork,
                  info);
}
void Call([[maybe_unused]] bool hermitian, const char* uplo,
          const lapack_int* n, const lapack_int* nrhs, double* a,
          const lapack_int* lda, double* extra, lapack_int* pivots, double* b,
          const lapack_int* ldb, double* work, const lapack_int* lwork,
          lapack_int* info) {
  LAPACK_dsysv_rk(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, work, lwork,
                  info);
}
void Call([[maybe_unused]] bool hermitian, const char* uplo,
          const lapack_int* n, const lapack_int* nrhs, std::complex<float>* a,
          const lapack_int* lda, std::complex<float>* extra, lapack_int* pivots,
          std::complex<float>* b, const lapack_int* ldb,
          std::complex<float>* work, const lapack_int* lwork,
          lapack_int* info) {
  if (hermitian) {
    LAPACK_chesv_rk(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, work, lwork,
                    info);
  } else {
    LAPACK_csysv_rk(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, work, lwork,
                    info);
  }
}
void Call([[maybe_unused]] bool hermitian, const char* uplo,
          const lapack_int* n, const lapack_int* nrhs, std::complex<double>* a,
          const lapack_int* lda, std::complex<double>* extra,
          lapack_int* pivots, std::complex<double>* b, const lapack_int* ldb,
          std::complex<double>* work, const lapack_int* lwork,
          lapack_int* info) {
  if (hermitian) {
    LAPACK_zhesv_rk(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, work, lwork,
                    info);
  } else {
    LAPACK_zsysv_rk(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, work, lwork,
                    info);
  }
}
template <typename T>
T Value(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}
template <typename T>
bool ArraysGuarded(const std::array<T, 6>& a, const std::array<T, 6>& old_a,
                   const std::array<T, 6>& b, const std::array<T, 6>& old_b,
                   lapack_int order, int columns, char triangle) {
  bool ok = true;
  for (std::size_t k = 0; k < a.size(); ++k) {
    bool active_a = false;
    bool active_b = false;
    for (lapack_int j = 0; j < order; ++j) {
      for (lapack_int i = 0; i < order; ++i) {
        active_a =
            active_a || (k == (1U + 2 * static_cast<std::size_t>(j) + i) &&
                         (triangle == 'U' ? i <= j : i >= j));
      }
    }
    for (int j = 0; j < columns; ++j) {
      for (lapack_int i = 0; i < order; ++i) {
        active_b = active_b || k == (1U + 2 * static_cast<std::size_t>(j) + i);
      }
    }
    if (!active_a) {
      ok = ok && a[k] == old_a[k];
    }
    if (!active_b) {
      ok = ok && b[k] == old_b[k];
    }
  }
  return ok;
}

template <typename T>
std::array<T, 6> RightHandSide(int mode, lapack_int order, int columns,
                               const std::array<T, 6>& a) {
  std::array<T, 6> b;
  b.fill(T{-7});
  for (int j = 0; j < columns; ++j) {
    if (order == 1) {
      b[1 + 2 * j] = mode == 1 ? Value<T>(4 * (j + 1)) : T{2};
    }
    if (order == 2) {
      b[1 + 2 * j] = a[3] * Value<T>(j + 1);
      b[2 + 2 * j] = a[2] * Value<T>(j + 1);
    }
  }
  return b;
}

template <typename T>
bool ActiveOutputs(int mode, lapack_int order, int columns,
                   lapack_int expected_info,
                   const std::array<lapack_int, 4>& pivots,
                   const std::array<T, 6>& b, const std::array<T, 6>& old_b) {
  using Real = asc::DenseBlasRealType<T>;
  bool ok = true;
  for (lapack_int i = 0; i < order; ++i) {
    const lapack_int expected = mode == 3 ? -(i + 1) : i + 1;
    ok = ok && pivots[static_cast<std::size_t>(i) + 1] == expected;
  }
  if (expected_info != 0) {
    ok = ok && b == old_b;
  } else {
    for (int j = 0; j < columns; ++j) {
      for (lapack_int i = 0; i < order; ++i) {
        const auto actual = b[1 + 2 * j + i];
        ok = ok && std::abs(actual - Value<T>(j + 1)) <=
                       32 * std::numeric_limits<Real>::epsilon();
      }
    }
  }
  return ok;
}

template <typename T>
bool Probe(bool hermitian, char triangle, int mode, int columns, int policy) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  const lapack_int nonempty_order = mode == 3 ? 2 : 1;
  const lapack_int order = mode == 0 ? 0 : nonempty_order;
  const lapack_int preferred = order == 0 ? 1 : 64 * order;
  const lapack_int nonquery_capacity = policy == 1 ? 1 : preferred;
  const lapack_int capacity = policy == 0 ? -1 : nonquery_capacity;
  std::array<char, 3> uplo{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> nrhs{kGuard, columns, kGuard};
  std::array<lapack_int, 3> lda{kGuard, 2, kGuard};
  std::array<lapack_int, 3> ldb{kGuard, 2, kGuard};
  std::array<lapack_int, 3> lwork{kGuard, capacity, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  std::array<lapack_int, 4> pivots{kGuard, kGuard, kGuard, kGuard};
  std::array<T, 6> a{T{-3},
                     mode == 1 ? T{4} : T{},
                     Value<T>(3, 4),
                     Value<T>(3, hermitian ? -4 : 4),
                     T{},
                     T{-5}};
  std::array<T, 4> extra{T{-13}, T{-13}, T{-13}, T{-13}};
  const auto old_extra = extra;
  auto b = RightHandSide(mode, order, columns, a);
  std::array<T, 130> work;
  work.fill(T{-11});
  const auto old_a = a;
  const auto old_b = b;
  const auto old_pivots = pivots;
  const auto old_n = n;
  const auto old_nrhs = nrhs;
  const auto old_lda = lda;
  const auto old_ldb = ldb;
  const auto old_lwork = lwork;
  const auto old_uplo = uplo;
  Call(hermitian, &uplo[1], &n[1], &nrhs[1], &a[1], &lda[1], &extra[1],
       &pivots[1], &b[1], &ldb[1], &work[1], &lwork[1], &info[1]);
  const lapack_int expected_info = mode == 2 && policy != 0 ? 1 : 0;
  bool ok = info[1] == expected_info && info.front() == kGuard &&
            info.back() == kGuard && n == old_n && nrhs == old_nrhs &&
            lda == old_lda && ldb == old_ldb && lwork == old_lwork &&
            uplo == old_uplo &&
            work[1] == Value<T>(static_cast<int>(preferred));
  if (policy == 0 || order == 0) {
    ok = ok && a == old_a && b == old_b && pivots == old_pivots &&
         extra == old_extra;
  } else {
    ok = ActiveOutputs(mode, order, columns, expected_info, pivots, b, old_b) &&
         ok;
  }
  if (policy != 0 && order > 0) {
    for (lapack_int i = 0; i < order; ++i) {
      const T expected = mode == 3 && (triangle == 'U' ? i == 1 : i == 0)
                             ? old_a[triangle == 'U' ? 3 : 2]
                             : T{};
      ok = ok && extra[static_cast<std::size_t>(i) + 1] == expected;
    }
  }
  ok = ok && extra.front() == T{-13} && extra.back() == T{-13};
  for (std::size_t i = static_cast<std::size_t>(order) + 1; i < extra.size();
       ++i) {
    ok = ok && extra[i] == old_extra[i];
  }
  ok = ok && pivots.front() == kGuard && pivots.back() == kGuard &&
       work.front() == T{-11};
  for (std::size_t i =
           static_cast<std::size_t>(capacity < 1 ? 1 : capacity) + 1;
       i < work.size(); ++i) {
    ok = ok && work[i] == T{-11};
  }
  ok = ArraysGuarded(a, old_a, b, old_b, order, columns, triangle) && ok;
  std::printf(
      "driver ABI bytes=%zu complex=%d he=%d uplo=%c mode=%d nrhs=%d policy=%d "
      "INFO=%lld pass=%d\n",
      sizeof(Real), asc::DenseBlasComplex<T>, hermitian, triangle, mode,
      columns, policy, static_cast<long long>(info[1]), ok);
  return ok;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  int passed = 0;
  int total = 0;
  for (const char triangle : {'U', 'L'}) {
    for (const int mode : {0, 1, 2, 3}) {
      for (const int nrhs : {0, 2}) {
        for (const int policy : {0, 1, 2}) {
          passed += static_cast<int>(
              Probe<float>(false, triangle, mode, nrhs, policy));
          passed += static_cast<int>(
              Probe<double>(false, triangle, mode, nrhs, policy));
          passed += static_cast<int>(
              Probe<std::complex<float>>(false, triangle, mode, nrhs, policy));
          passed += static_cast<int>(
              Probe<std::complex<double>>(false, triangle, mode, nrhs, policy));
          passed += static_cast<int>(
              Probe<std::complex<float>>(true, triangle, mode, nrhs, policy));
          passed += static_cast<int>(
              Probe<std::complex<double>>(true, triangle, mode, nrhs, policy));
          total += 6;
        }
      }
    }
  }
  std::printf("RK driver ABI probes=%d passed=%d\n", total, passed);
  return passed == total ? 0 : 1;
}
