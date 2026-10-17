#include "matrix_permutation_faults.h"

#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/dense/blas.h"

namespace asc_matrix_permutation_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local bool g_baseline = true;

template <typename T>
T Value(int x) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (std::is_same_v<T, Real>) {
    return static_cast<T>(x);
  } else {
    return {static_cast<Real>(x), static_cast<Real>(x + 11)};
  }
}

template <typename T, typename Function>
bool Baseline(bool rows, lapack_logical forward, Function real) {
  std::array<T, 17> matrix;
  matrix.fill(Value<T>(-7));
  for (std::size_t j = 0; j < 3; ++j) {
    for (std::size_t i = 0; i < 3; ++i) {
      matrix[1 + 5 * j + i] = Value<T>(static_cast<int>(1 + 3 * j + i));
    }
  }
  const auto before = matrix;
  auto expected = matrix;
  const std::array<std::size_t, 3> permutation{2, 0, 1};
  for (std::size_t j = 0; j < 3; ++j) {
    for (std::size_t i = 0; i < 3; ++i) {
      const auto pi = rows ? permutation[i] : i;
      const auto pj = rows ? j : permutation[j];
      if (forward != 0) {
        expected[1 + 5 * j + i] = before[1 + 5 * pj + pi];
      } else {
        expected[1 + 5 * pj + pi] = before[1 + 5 * j + i];
      }
    }
  }
  std::array<lapack_int, 5> k{79, 3, 1, 2, 83};
  const auto k_before = k;
  std::array<lapack_int, 3> m{89, 3, 97};
  std::array<lapack_int, 3> n{101, 3, 103};
  std::array<lapack_int, 3> ld{107, 5, 109};
  std::array<lapack_logical, 3> direction{113, forward, 127};
  const auto m_before = m;
  const auto n_before = n;
  const auto ld_before = ld;
  const auto direction_before = direction;
  real(&direction[1], &m[1], &n[1], matrix.data() + 1, &ld[1], k.data() + 1);
  return matrix == expected && k == k_before && m == m_before &&
         n == n_before && ld == ld_before && direction == direction_before;
}

template <typename T, typename Function>
void Inject(bool rows, const lapack_logical* forward, const lapack_int* m,
            const lapack_int* n, T* matrix, const lapack_int* ld, lapack_int* k,
            Function real) {
  ++g_calls;
  g_baseline = g_baseline && (*forward == 0 || *forward == 1) && *m == 3 &&
               *n == 3 && *ld == 3 && Baseline<T>(rows, *forward, real);
  if (g_mode != Mode::kOmitValues) {
    real(forward, m, n, matrix, ld, k);
  }
  // Only mutate returned, valid local data; malformed K never enters Fortran.
  switch (g_mode) {
    case Mode::kNegativeIndex:
      k[0] = -k[0];
      break;
    case Mode::kChangedPermutation:
      std::swap(k[0], k[1]);
      break;
    case Mode::kMaximumIndex:
      k[0] = std::numeric_limits<lapack_int>::max();
      break;
    case Mode::kWrongValues:
      for (lapack_int j = 0; j < *n; ++j) {
        for (lapack_int i = 0; i < *m; ++i) {
          matrix[j * *ld + i] = T{42};
        }
      }
      break;
    case Mode::kOmitValues:
    case Mode::kPass:
      break;
  }
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_baseline = true;
}
std::size_t Calls() { return g_calls; }
bool BaselinePassed() { return g_baseline; }
// Private GNU ELF interception names.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slapmr) __real_slapmr_;
extern "C" void __wrap_slapmr_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               float* matrix, const lapack_int* ld,
                               lapack_int* k) {
  Inject(true, forward, m, n, matrix, ld, k, __real_slapmr_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_slapmr), decltype(__wrap_slapmr_)>);
extern "C" decltype(LAPACK_slapmt) __real_slapmt_;
extern "C" void __wrap_slapmt_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               float* matrix, const lapack_int* ld,
                               lapack_int* k) {
  Inject(false, forward, m, n, matrix, ld, k, __real_slapmt_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_slapmt), decltype(__wrap_slapmt_)>);
extern "C" decltype(LAPACK_dlapmr) __real_dlapmr_;
extern "C" void __wrap_dlapmr_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               double* matrix, const lapack_int* ld,
                               lapack_int* k) {
  Inject(true, forward, m, n, matrix, ld, k, __real_dlapmr_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlapmr), decltype(__wrap_dlapmr_)>);
extern "C" decltype(LAPACK_dlapmt) __real_dlapmt_;
extern "C" void __wrap_dlapmt_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               double* matrix, const lapack_int* ld,
                               lapack_int* k) {
  Inject(false, forward, m, n, matrix, ld, k, __real_dlapmt_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_dlapmt), decltype(__wrap_dlapmt_)>);
extern "C" decltype(LAPACK_clapmr) __real_clapmr_;
extern "C" void __wrap_clapmr_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               std::complex<float>* matrix,
                               const lapack_int* ld, lapack_int* k) {
  Inject(true, forward, m, n, matrix, ld, k, __real_clapmr_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_clapmr), decltype(__wrap_clapmr_)>);
extern "C" decltype(LAPACK_clapmt) __real_clapmt_;
extern "C" void __wrap_clapmt_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               std::complex<float>* matrix,
                               const lapack_int* ld, lapack_int* k) {
  Inject(false, forward, m, n, matrix, ld, k, __real_clapmt_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_clapmt), decltype(__wrap_clapmt_)>);
extern "C" decltype(LAPACK_zlapmr) __real_zlapmr_;
extern "C" void __wrap_zlapmr_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               std::complex<double>* matrix,
                               const lapack_int* ld, lapack_int* k) {
  Inject(true, forward, m, n, matrix, ld, k, __real_zlapmr_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlapmr), decltype(__wrap_zlapmr_)>);
extern "C" decltype(LAPACK_zlapmt) __real_zlapmt_;
extern "C" void __wrap_zlapmt_(const lapack_logical* forward,
                               const lapack_int* m, const lapack_int* n,
                               std::complex<double>* matrix,
                               const lapack_int* ld, lapack_int* k) {
  Inject(false, forward, m, n, matrix, ld, k, __real_zlapmt_);
}
static_assert(
    std::is_same_v<decltype(LAPACK_zlapmt), decltype(__wrap_zlapmt_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_matrix_permutation_fault
