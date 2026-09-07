#if defined(ASC_LU_BAND_EMITTED_PROTOTYPES_HEADER)
// Untouched pinned-source compiler emissions are supplied by the diagnostic
// build with test-only function renames. They never enter an installed header.
#include ASC_LU_BAND_EMITTED_PROTOTYPES_HEADER
#endif

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

namespace {
#if defined(ASC_LU_BAND_EMITTED_PROTOTYPES_HEADER)
// Fortran's non-BIND(C) emissions omit C const input annotations. Compare all
// representation types, pointer levels and hidden length after removing only
// those annotations, preserving return and integer/complex/length types.
template <typename T>
struct Unqualified {
  using type = T;
};
template <typename T>
struct Unqualified<T*> {
  using type = std::remove_const_t<T>*;
};
template <typename T>
struct Signature;
template <typename Return, typename... Args>
struct Signature<Return(Args...)> {
  using type = Return(typename Unqualified<Args>::type...);
};
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_sgbtrf)>::type,
                   typename Signature<decltype(LAPACK_sgbtrf)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_sgbtrs)>::type,
                   typename Signature<decltype(LAPACK_sgbtrs_base)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_dgbtrf)>::type,
                   typename Signature<decltype(LAPACK_dgbtrf)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_dgbtrs)>::type,
                   typename Signature<decltype(LAPACK_dgbtrs_base)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_cgbtrf)>::type,
                   typename Signature<decltype(LAPACK_cgbtrf)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_cgbtrs)>::type,
                   typename Signature<decltype(LAPACK_cgbtrs_base)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_zgbtrf)>::type,
                   typename Signature<decltype(LAPACK_zgbtrf)>::type>);
static_assert(
    std::is_same_v<typename Signature<decltype(asc_emitted_zgbtrs)>::type,
                   typename Signature<decltype(LAPACK_zgbtrs_base)>::type>);
#endif

template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kFactor = LAPACK_sgbtrf;
  static constexpr auto kSolve = LAPACK_sgbtrs_base;
};
template <>
struct Native<double> {
  static constexpr auto kFactor = LAPACK_dgbtrf;
  static constexpr auto kSolve = LAPACK_dgbtrs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kFactor = LAPACK_cgbtrf;
  static constexpr auto kSolve = LAPACK_cgbtrs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kFactor = LAPACK_zgbtrf;
  static constexpr auto kSolve = LAPACK_zgbtrs_base;
};

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
T Conjugate(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename T>
bool CheckRhs(const std::array<T, 10>& rhs, const std::array<T, 4>& expected,
              T guard) {
  for (std::size_t i = 0; i < rhs.size(); ++i) {
    if (i == 1 || i == 2 || i == 5 || i == 6) {
      const auto row = (i - 1) % 4;
      const auto column = (i - 1) / 4;
      const auto error = std::abs(rhs[i] - expected[2 * row + column]);
      if (!std::isfinite(error) ||
          error >
              32 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon()) {
        return false;
      }
    } else if (rhs[i] != guard) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool ProbeOperation(char op) {
  constexpr auto kGuard = std::numeric_limits<lapack_int>::max() - 23;
  const std::array<T, 4> a{T{}, Value<T>(2, 1), Value<T>(3, -1), T{1}};
  const T guard = Value<T>(-151, 47);
  std::array<lapack_int, 3> m{kGuard, 2, kGuard};
  std::array<lapack_int, 3> n{kGuard, 2, kGuard};
  std::array<lapack_int, 3> kl{kGuard, 1, kGuard};
  std::array<lapack_int, 3> ku{kGuard, 1, kGuard};
  std::array<lapack_int, 3> ld{kGuard, 5, kGuard};
  std::array<lapack_int, 3> nrhs{kGuard, 2, kGuard};
  std::array<lapack_int, 3> ldb{kGuard, 4, kGuard};
  std::array<lapack_int, 3> info{kGuard, kGuard, kGuard};
  std::array<lapack_int, 4> pivots{kGuard, kGuard, kGuard, kGuard};
  std::array<T, 12> band{};
  band.fill(guard);
  band[3] = a[0];
  band[4] = a[2];
  band[7] = a[1];
  band[8] = a[3];
  Native<T>::kFactor(&m[1], &n[1], &kl[1], &ku[1], band.data() + 1, &ld[1],
                     pivots.data() + 1, &info[1]);
  if (info[1] != 0 || pivots[1] != 2 || pivots[2] != 2 ||
      info.front() != kGuard || info.back() != kGuard ||
      pivots.front() != kGuard || pivots.back() != kGuard ||
      band.front() != guard || band.back() != guard || band[5] != guard ||
      band[10] != guard) {
    return false;
  }
  const auto factored = band;
  const auto swaps = pivots;
  std::array<T, 10> rhs{};
  rhs.fill(guard);
  const std::array<T, 4> expected{Value<T>(1, 1), Value<T>(2, -1),
                                  Value<T>(-1, 2), Value<T>(3, 1)};
  for (std::size_t j = 0; j < 2; ++j) {
    for (std::size_t i = 0; i < 2; ++i) {
      rhs[1 + 4 * j + i] = T{};
      for (std::size_t k = 0; k < 2; ++k) {
        T coefficient = a[2 * i + k];
        if (op != 'N') {
          coefficient = a[2 * k + i];
        }
        if (op == 'C') {
          coefficient = Conjugate(coefficient);
        }
        rhs[1 + 4 * j + i] += coefficient * expected[2 * k + j];
      }
    }
  }
  std::array<char, 3> trans{'a', op, 'z'};
  info[1] = kGuard;
  Native<T>::kSolve(&trans[1], &n[1], &kl[1], &ku[1], &nrhs[1], band.data() + 1,
                    &ld[1], pivots.data() + 1, rhs.data() + 1, &ldb[1],
                    &info[1], FORTRAN_STRLEN{1});
  if (info[1] != 0 || info.front() != kGuard || info.back() != kGuard ||
      band != factored || pivots != swaps ||
      trans != std::array<char, 3>{'a', op, 'z'}) {
    return false;
  }
  for (const auto* integer : {&m, &n, &kl, &ku, &ld, &nrhs, &ldb}) {
    if (integer->front() != kGuard || integer->back() != kGuard) {
      return false;
    }
  }
  if (m[1] != 2 || n[1] != 2 || kl[1] != 1 || ku[1] != 1 || ld[1] != 5 ||
      nrhs[1] != 2 || ldb[1] != 4) {
    return false;
  }
  return CheckRhs(rhs, expected, guard);
}
template <typename T>
bool Probe() {
  return ProbeOperation<T>('N') && ProbeOperation<T>('T') &&
         ProbeOperation<T>('C');
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
  static_assert(std::is_signed_v<lapack_int>);
  static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
  static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);
  if (!Probe<float>() || !Probe<double>() || !Probe<std::complex<float>>() ||
      !Probe<std::complex<double>>()) {
    return 1;
  }
  std::printf(
      "Actual all-eight GB prototypes, native INTEGER=%d, CHARACTER "
      "length=%zu, guarded N/T/C solve checks passed.\n",
      ASC_LAPACK_INTEGER_BITS, sizeof(FORTRAN_STRLEN));
  return 0;
}
