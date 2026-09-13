#if defined(ASC_INVERSE_EMITTED_PROTOTYPES)
// Untouched GNU emissions, renamed only inside this test translation unit.
#include ASC_INVERSE_EMITTED_SSYTRI_HEADER
#include ASC_INVERSE_EMITTED_DSYTRI_HEADER
#include ASC_INVERSE_EMITTED_CSYTRI_HEADER
#include ASC_INVERSE_EMITTED_ZSYTRI_HEADER
#include ASC_INVERSE_EMITTED_CHETRI_HEADER
#include ASC_INVERSE_EMITTED_ZHETRI_HEADER
#undef ssytri_
#undef dsytri_
#undef csytri_
#undef zsytri_
#undef chetri_
#undef zhetri_
#endif
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "indefinite_inverse_native.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_INVERSE_EMITTED_PROTOTYPES)
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
static_assert(std::is_same_v<
              typename AbiSignature<decltype(LAPACK_ssytri_base)>::Type,
              typename AbiSignature<decltype(asc_probe_emitted_ssytri)>::Type>);
static_assert(std::is_same_v<
              typename AbiSignature<decltype(LAPACK_dsytri_base)>::Type,
              typename AbiSignature<decltype(asc_probe_emitted_dsytri)>::Type>);
static_assert(std::is_same_v<
              typename AbiSignature<decltype(LAPACK_csytri_base)>::Type,
              typename AbiSignature<decltype(asc_probe_emitted_csytri)>::Type>);
static_assert(std::is_same_v<
              typename AbiSignature<decltype(LAPACK_zsytri_base)>::Type,
              typename AbiSignature<decltype(asc_probe_emitted_zsytri)>::Type>);
static_assert(std::is_same_v<
              typename AbiSignature<decltype(LAPACK_chetri_base)>::Type,
              typename AbiSignature<decltype(asc_probe_emitted_chetri)>::Type>);
static_assert(std::is_same_v<
              typename AbiSignature<decltype(LAPACK_zhetri_base)>::Type,
              typename AbiSignature<decltype(asc_probe_emitted_zhetri)>::Type>);
}  // namespace
#endif
namespace {
template <typename T>
T Value(int real, int imaginary = 0) {
  if constexpr (asc::DenseBlasComplex<T>) {
    using Real = asc::DenseBlasRealType<T>;
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
std::array<T, 14> MakeMatrix(lapack_int order, char triangle, int mode) {
  std::array<T, 14> a;
  a.fill(Value<T>(-173, 23));
  for (lapack_int j = 0; j < order; ++j) {
    for (lapack_int i = 0; i < order; ++i) {
      if ((triangle == 'U' && i <= j) || (triangle == 'L' && i >= j)) {
        a[1 + j * 4 + i] = T{};
      }
    }
  }
  if (mode == 1) {
    a[1] = T{4};
  }
  if (mode == 3) {
    a[triangle == 'U' ? 5U : 2U] = Value<T>(3, 4);
  }
  if (mode == 4) {
    a[6] = T{4};
  }
  return a;
}

template <typename T>
bool MatrixGuards(const std::array<T, 14>& a, const std::array<T, 14>& before,
                  lapack_int order, char triangle, lapack_int expected_info) {
  bool guards = true;
  for (std::size_t k = 0; k < a.size(); ++k) {
    const int i = (static_cast<int>(k) - 1) % 4;
    const int j = (static_cast<int>(k) - 1) / 4;
    const bool selected =
        k != 0 && i < order && j < order && (triangle == 'U' ? i <= j : i >= j);
    if (!selected || expected_info > 0) {
      guards = guards && a[k] == before[k];
    }
  }
  return guards;
}

template <typename T>
bool Probe(bool hermitian, char triangle, int mode) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr lapack_int kGuard = std::numeric_limits<lapack_int>::max() - 79;
  lapack_int order = 1;
  if (mode == 0) {
    order = 0;
  }
  if (mode == 3) {
    order = 2;
  }
  if (mode == 4) {
    order = 3;
  }
  lapack_int expected_info = mode == 2 ? 1 : 0;
  if (mode == 4) {
    expected_info = triangle == 'U' ? 3 : 1;
  }
  std::array<char, 3> uplo{'a', triangle, 'z'};
  std::array<lapack_int, 3> n{kGuard, order, kGuard};
  std::array<lapack_int, 3> lda{kGuard, 4, kGuard};
  std::array<lapack_int, 3> info{kGuard, std::numeric_limits<lapack_int>::min(),
                                 kGuard};
  const lapack_int paired_pivot = triangle == 'U' ? -1 : -2;
  std::array<lapack_int, 5> pivots{kGuard, mode == 3 ? paired_pivot : 1,
                                   mode == 3 ? paired_pivot : 2, 3, kGuard};
  auto a = MakeMatrix<T>(order, triangle, mode);
  std::array<T, 8> work;
  work.fill(Value<T>(-179, 29));
  const auto before = a;
  const auto pivot_before = pivots;
  asc_inverse_test::Native(hermitian, &uplo[1], &n[1], &a[1], &lda[1],
                           &pivots[1], &work[1], &info[1]);
  bool output = info[1] == expected_info;
  if (mode == 1) {
    output = output && a[1] == T{0.25};
  }
  if (mode == 3) {
    const auto off = triangle == 'U' ? 5U : 2U;
    T denominator = before[off];
    if constexpr (asc::DenseBlasComplex<T>) {
      if (hermitian) {
        denominator = std::conj(denominator);
      }
    }
    output = output && a[1] == T{} && a[6] == T{} &&
             std::abs(a[off] - T{1} / denominator) <=
                 32 * std::numeric_limits<Real>::epsilon();
  }
  bool guards = info.front() == kGuard && info.back() == kGuard &&
                pivots == pivot_before &&
                uplo == std::array<char, 3>{'a', triangle, 'z'} &&
                n == std::array<lapack_int, 3>{kGuard, order, kGuard} &&
                lda == std::array<lapack_int, 3>{kGuard, 4, kGuard} &&
                work.front() == Value<T>(-179, 29);
  guards = MatrixGuards(a, before, order, triangle, expected_info) && guards;
  for (std::size_t k = 1 + static_cast<std::size_t>(order); k < work.size();
       ++k) {
    guards = guards && work[k] == Value<T>(-179, 29);
  }
  if (order <= 1 || expected_info > 0) {
    for (const T value : work) {
      guards = guards && value == Value<T>(-179, 29);
    }
  }
  std::printf(
      "ABI real-bytes=%zu complex=%d hermitian=%d triangle=%c mode=%d "
      "info=%lld pass=%d\n",
      sizeof(Real), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(hermitian), triangle, mode,
      static_cast<long long>(info[1]), static_cast<int>(output && guards));
  return output && guards;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  bool passed = true;
  int cases = 0;
  for (const char triangle : {'U', 'L'}) {
    for (const int mode : {0, 1, 2, 3, 4}) {
      passed = Probe<float>(false, triangle, mode) && passed;
      passed = Probe<double>(false, triangle, mode) && passed;
      passed = Probe<std::complex<float>>(false, triangle, mode) && passed;
      passed = Probe<std::complex<double>>(false, triangle, mode) && passed;
      passed = Probe<std::complex<float>>(true, triangle, mode) && passed;
      passed = Probe<std::complex<double>>(true, triangle, mode) && passed;
      cases += 6;
    }
  }
  std::printf("direct classic inverse ABI cases=%d\n", cases);
  return passed ? 0 : 1;
}
