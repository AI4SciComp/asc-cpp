#include "complete_pivoting_faults.h"

#include <array>
#include <complex>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_complete_pivoting_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/dense/blas.h"

namespace asc_complete_pivoting_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local bool g_baseline = true;

template <typename T>
struct Control {
  std::array<T, 17> a;
  std::array<T, 5> rhs{T{-71}, T{4}, T{4}, T{3}, T{-73}};
  std::array<lapack_int, 5> rows{79, 1, 2, 3, 83};
  std::array<lapack_int, 5> columns{89, 1, 2, 3, 97};
  std::array<lapack_int, 3> n{101, 3, 103};
  std::array<lapack_int, 3> ld{107, 5, 109};
  Control() {
    a.fill(T{-67});
    for (std::size_t j = 0; j < 3; ++j) {
      for (std::size_t i = 0; i < 3; ++i) {
        a[1 + 5 * j + i] = i == j ? T{1} : T{};
      }
    }
    a[1] = T{4};
    a[7] = T{2};
  }
  [[nodiscard]] bool Preserved(const Control& before) const {
    return a == before.a && rows == before.rows && columns == before.columns &&
           n == before.n && ld == before.ld;
  }
};

template <typename T, typename Function>
bool FactorBaseline(Function real) {
  Control<T> control;
  const auto before = control;
  std::array<lapack_int, 3> info{113, -127, 131};
  real(&control.n[1], control.a.data() + 1, &control.ld[1],
       control.rows.data() + 1, control.columns.data() + 1, &info[1]);
  return control.Preserved(before) && control.rhs == before.rhs &&
         info == std::array<lapack_int, 3>{113, 0, 131};
}

template <typename T, typename Function>
bool SolveBaseline(Function real) {
  using Real = asc::DenseBlasRealType<T>;
  Control<T> control;
  const auto before = control;
  std::array<Real, 3> scale{Real{-113}, Real{-127}, Real{-131}};
  real(&control.n[1], control.a.data() + 1, &control.ld[1],
       control.rhs.data() + 1, control.rows.data() + 1,
       control.columns.data() + 1, &scale[1]);
  return control.Preserved(before) &&
         control.rhs == std::array<T, 5>{T{-71}, T{1}, T{2}, T{3}, T{-73}} &&
         scale == std::array<Real, 3>{Real{-113}, Real{1}, Real{-131}};
}

template <typename T, typename Function>
void Factor(const lapack_int* n, T* a, const lapack_int* ld, lapack_int* rows,
            lapack_int* columns, lapack_int* info, Function real) {
  ++g_calls;
  g_baseline = g_baseline && *n > 0 && *ld == *n && FactorBaseline<T>(real);
  const auto seed = *info;
  if (g_mode == Mode::kOmitValues) {
    for (lapack_int i = 0; i < *n; ++i) {
      rows[i] = i + 1;
      columns[i] = i + 1;
    }
    *info = 0;
  } else {
    real(n, a, ld, rows, columns, info);
  }
  // These are bounded returned-data mutations. Invalid pivots never enter
  // a native solve, and the separately guarded real call remains required.
  switch (g_mode) {
    case Mode::kOmitInfo:
      *info = seed;
      break;
    case Mode::kNegativeInfo:
      *info = -1;
      break;
    case Mode::kMaximumInfo:
      *info = std::numeric_limits<lapack_int>::max();
      break;
    case Mode::kMissingRow:
      rows[0] = std::numeric_limits<lapack_int>::min();
      break;
    case Mode::kInvalidColumn:
      columns[*n - 1] = 1;
      break;
    case Mode::kNonfiniteValues:
      a[0] = T{std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN()};
      break;
    case Mode::kWrongValues:
      for (lapack_int j = 0; j < *n; ++j) {
        for (lapack_int i = 0; i < *n; ++i) {
          a[j * *ld + i] = T{42};
        }
      }
      break;
    default:
      break;
  }
}

template <typename T, typename Function>
void Solve(const lapack_int* n, const T* a, const lapack_int* ld, T* rhs,
           const lapack_int* rows, const lapack_int* columns,
           asc::DenseBlasRealType<T>* scale, Function real) {
  using Real = asc::DenseBlasRealType<T>;
  ++g_calls;
  g_baseline = g_baseline && *n > 0 && *ld == *n && SolveBaseline<T>(real);
  const Real seed = *scale;
  if (g_mode == Mode::kOmitValues) {
    *scale = Real{1};
  } else {
    real(n, a, ld, rhs, rows, columns, scale);
  }
  switch (g_mode) {
    case Mode::kOmitScale:
      *scale = seed;
      break;
    case Mode::kNegativeScale:
      *scale = Real{-1};
      break;
    case Mode::kLargeScale:
      *scale = Real{2};
      break;
    case Mode::kZeroScale:
      *scale = Real{0};
      for (lapack_int i = 0; i < *n; ++i) {
        rhs[i] = T{};
      }
      break;
    case Mode::kChangedFactor:
      const_cast<T*>(a)[0] = T{42};
      break;
    case Mode::kChangedRow:
      const_cast<lapack_int*>(rows)[0] = -1;
      break;
    case Mode::kChangedColumn:
      const_cast<lapack_int*>(columns)[0] = -1;
      break;
    case Mode::kNonfiniteValues:
      rhs[0] = T{std::numeric_limits<Real>::quiet_NaN()};
      break;
    case Mode::kWrongValues:
      for (lapack_int i = 0; i < *n; ++i) {
        rhs[i] = T{42};
      }
      break;
    default:
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

// Private GNU ELF interception names; exact private production signatures.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(ASC_LAPACK_sgetc2) __real_sgetc2_;
extern "C" void __wrap_sgetc2_(const lapack_int* n, float* a,
                               const lapack_int* ld, lapack_int* rows,
                               lapack_int* columns, lapack_int* info) {
  Factor(n, a, ld, rows, columns, info, __real_sgetc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_sgetc2), decltype(__wrap_sgetc2_)>);

extern "C" decltype(ASC_LAPACK_sgesc2) __real_sgesc2_;
extern "C" void __wrap_sgesc2_(const lapack_int* n, const float* a,
                               const lapack_int* ld, float* rhs,
                               const lapack_int* rows,
                               const lapack_int* columns, float* scale) {
  Solve(n, a, ld, rhs, rows, columns, scale, __real_sgesc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_sgesc2), decltype(__wrap_sgesc2_)>);

extern "C" decltype(ASC_LAPACK_dgetc2) __real_dgetc2_;
extern "C" void __wrap_dgetc2_(const lapack_int* n, double* a,
                               const lapack_int* ld, lapack_int* rows,
                               lapack_int* columns, lapack_int* info) {
  Factor(n, a, ld, rows, columns, info, __real_dgetc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_dgetc2), decltype(__wrap_dgetc2_)>);

extern "C" decltype(ASC_LAPACK_dgesc2) __real_dgesc2_;
extern "C" void __wrap_dgesc2_(const lapack_int* n, const double* a,
                               const lapack_int* ld, double* rhs,
                               const lapack_int* rows,
                               const lapack_int* columns, double* scale) {
  Solve(n, a, ld, rhs, rows, columns, scale, __real_dgesc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_dgesc2), decltype(__wrap_dgesc2_)>);

extern "C" decltype(ASC_LAPACK_cgetc2) __real_cgetc2_;
extern "C" void __wrap_cgetc2_(const lapack_int* n, std::complex<float>* a,
                               const lapack_int* ld, lapack_int* rows,
                               lapack_int* columns, lapack_int* info) {
  Factor(n, a, ld, rows, columns, info, __real_cgetc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_cgetc2), decltype(__wrap_cgetc2_)>);

extern "C" decltype(ASC_LAPACK_cgesc2) __real_cgesc2_;
extern "C" void __wrap_cgesc2_(const lapack_int* n,
                               const std::complex<float>* a,
                               const lapack_int* ld, std::complex<float>* rhs,
                               const lapack_int* rows,
                               const lapack_int* columns, float* scale) {
  Solve(n, a, ld, rhs, rows, columns, scale, __real_cgesc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_cgesc2), decltype(__wrap_cgesc2_)>);

extern "C" decltype(ASC_LAPACK_zgetc2) __real_zgetc2_;
extern "C" void __wrap_zgetc2_(const lapack_int* n, std::complex<double>* a,
                               const lapack_int* ld, lapack_int* rows,
                               lapack_int* columns, lapack_int* info) {
  Factor(n, a, ld, rows, columns, info, __real_zgetc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_zgetc2), decltype(__wrap_zgetc2_)>);

extern "C" decltype(ASC_LAPACK_zgesc2) __real_zgesc2_;
extern "C" void __wrap_zgesc2_(const lapack_int* n,
                               const std::complex<double>* a,
                               const lapack_int* ld, std::complex<double>* rhs,
                               const lapack_int* rows,
                               const lapack_int* columns, double* scale) {
  Solve(n, a, ld, rhs, rows, columns, scale, __real_zgesc2_);
}
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_zgesc2), decltype(__wrap_zgesc2_)>);

// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_complete_pivoting_fault
