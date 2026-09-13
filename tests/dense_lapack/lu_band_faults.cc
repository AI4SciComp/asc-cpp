#include "lu_band_faults.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_lu_band_abi.h"

namespace {
using asc_lu_band_faults::Fault;
Fault g_fault = Fault::kPass;
int g_calls = 0;

template <typename T>
void Factor(lapack_int m, lapack_int n, lapack_int kl, lapack_int ku, T* ab,
            lapack_int* pivots, lapack_int* info) {
  const auto count = std::min(m, n);
  if (count > 0) {
    ab[kl + ku] = T{123};
  }
  if (g_fault != Fault::kPartialWidth) {
    for (lapack_int i = 0; i < count; ++i) {
      pivots[i] = i + 1;
    }
  }
  if (g_fault != Fault::kUnwrittenInfo) {
    *info = 0;
  }
  switch (g_fault) {
    case Fault::kNegative:
      *info = -4;
      break;
    case Fault::kMinimum:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case Fault::kLargePositive:
      *info = count + 1;
      break;
    case Fault::kZeroPivot:
      pivots[0] = 0;
      break;
    case Fault::kNegativePivot:
      pivots[0] = -1;
      break;
    case Fault::kOutOfBandPivot:
      pivots[0] = kl + 2;
      break;
    case Fault::kLatePivot:
      pivots[count - 1] = 0;
      break;
    case Fault::kPartialWidth: {
      if constexpr (sizeof(lapack_int) == 8) {
        const std::int32_t low = 1;
        std::memcpy(pivots, &low, sizeof(low));
      }
      break;
    }
    case Fault::kUnwrittenInfo:
    case Fault::kPass:
      break;
  }
}

template <typename T>
void Solve(T* rhs, lapack_int* info) {
  rhs[0] = T{123};
  if (g_fault == Fault::kUnwrittenInfo) {
    return;
  }
  *info = 1;
  if (g_fault == Fault::kNegative) {
    *info = -4;
  }
  if (g_fault == Fault::kMinimum) {
    *info = std::numeric_limits<lapack_int>::min();
  }
}
}  // namespace

namespace asc_lu_band_faults {
void Select(Fault fault) {
  g_fault = fault;
  g_calls = 0;
}
int Calls() { return g_calls; }
}  // namespace asc_lu_band_faults

// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgbtf2_(lapack_int*, lapack_int*, lapack_int*, lapack_int*, float*,
                    lapack_int*, lapack_int*, lapack_int*);
void __wrap_sgbtf2_(lapack_int* m, lapack_int* n, lapack_int* kl,
                    lapack_int* ku, float* ab, lapack_int* ld,
                    lapack_int* pivots, lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_sgbtf2_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(std::is_same_v<decltype(__wrap_sgbtf2_),
                             decltype(LAPACK_GLOBAL_SUFFIX(sgbtf2, SGBTF2))>);
void __real_dgbtf2_(lapack_int*, lapack_int*, lapack_int*, lapack_int*, double*,
                    lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgbtf2_(lapack_int* m, lapack_int* n, lapack_int* kl,
                    lapack_int* ku, double* ab, lapack_int* ld,
                    lapack_int* pivots, lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_dgbtf2_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(std::is_same_v<decltype(__wrap_dgbtf2_),
                             decltype(LAPACK_GLOBAL_SUFFIX(dgbtf2, DGBTF2))>);
void __real_cgbtf2_(lapack_int*, lapack_int*, lapack_int*, lapack_int*,
                    lapack_complex_float*, lapack_int*, lapack_int*,
                    lapack_int*);
void __wrap_cgbtf2_(lapack_int* m, lapack_int* n, lapack_int* kl,
                    lapack_int* ku, lapack_complex_float* ab, lapack_int* ld,
                    lapack_int* pivots, lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_cgbtf2_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(std::is_same_v<decltype(__wrap_cgbtf2_),
                             decltype(LAPACK_GLOBAL_SUFFIX(cgbtf2, CGBTF2))>);
void __real_zgbtf2_(lapack_int*, lapack_int*, lapack_int*, lapack_int*,
                    lapack_complex_double*, lapack_int*, lapack_int*,
                    lapack_int*);
void __wrap_zgbtf2_(lapack_int* m, lapack_int* n, lapack_int* kl,
                    lapack_int* ku, lapack_complex_double* ab, lapack_int* ld,
                    lapack_int* pivots, lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_zgbtf2_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(std::is_same_v<decltype(__wrap_zgbtf2_),
                             decltype(LAPACK_GLOBAL_SUFFIX(zgbtf2, ZGBTF2))>);
}
// NOLINTEND(bugprone-reserved-identifier)

// Exact GNU ld wrapper names are confined to this diagnostic executable.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgbtrf_(const lapack_int*, const lapack_int*, const lapack_int*,
                    const lapack_int*, float*, const lapack_int*, lapack_int*,
                    lapack_int*);
void __wrap_sgbtrf_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku, float* ab,
                    const lapack_int* ld, lapack_int* pivots,
                    lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_sgbtrf_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbtrf_), decltype(LAPACK_sgbtrf)>);

void __real_sgbtrs_(const char*, const lapack_int*, const lapack_int*,
                    const lapack_int*, const lapack_int*, const float*,
                    const lapack_int*, const lapack_int*, float*,
                    const lapack_int*, lapack_int*, FORTRAN_STRLEN);
void __wrap_sgbtrs_(const char* trans, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku,
                    const lapack_int* nrhs, const float* ab,
                    const lapack_int* ld, const lapack_int* pivots, float* rhs,
                    const lapack_int* ldb, lapack_int* info,
                    FORTRAN_STRLEN length) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_sgbtrs_(trans, n, kl, ku, nrhs, ab, ld, pivots, rhs, ldb, info,
                   length);
  } else {
    Solve(rhs, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbtrs_), decltype(LAPACK_sgbtrs_base)>);

void __real_dgbtrf_(const lapack_int*, const lapack_int*, const lapack_int*,
                    const lapack_int*, double*, const lapack_int*, lapack_int*,
                    lapack_int*);
void __wrap_dgbtrf_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku, double* ab,
                    const lapack_int* ld, lapack_int* pivots,
                    lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_dgbtrf_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbtrf_), decltype(LAPACK_dgbtrf)>);

void __real_dgbtrs_(const char*, const lapack_int*, const lapack_int*,
                    const lapack_int*, const lapack_int*, const double*,
                    const lapack_int*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, FORTRAN_STRLEN);
void __wrap_dgbtrs_(const char* trans, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku,
                    const lapack_int* nrhs, const double* ab,
                    const lapack_int* ld, const lapack_int* pivots, double* rhs,
                    const lapack_int* ldb, lapack_int* info,
                    FORTRAN_STRLEN length) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_dgbtrs_(trans, n, kl, ku, nrhs, ab, ld, pivots, rhs, ldb, info,
                   length);
  } else {
    Solve(rhs, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbtrs_), decltype(LAPACK_dgbtrs_base)>);

void __real_cgbtrf_(const lapack_int*, const lapack_int*, const lapack_int*,
                    const lapack_int*, lapack_complex_float*, const lapack_int*,
                    lapack_int*, lapack_int*);
void __wrap_cgbtrf_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku,
                    lapack_complex_float* ab, const lapack_int* ld,
                    lapack_int* pivots, lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_cgbtrf_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbtrf_), decltype(LAPACK_cgbtrf)>);

void __real_cgbtrs_(const char*, const lapack_int*, const lapack_int*,
                    const lapack_int*, const lapack_int*,
                    const lapack_complex_float*, const lapack_int*,
                    const lapack_int*, lapack_complex_float*, const lapack_int*,
                    lapack_int*, FORTRAN_STRLEN);
void __wrap_cgbtrs_(const char* trans, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku,
                    const lapack_int* nrhs, const lapack_complex_float* ab,
                    const lapack_int* ld, const lapack_int* pivots,
                    lapack_complex_float* rhs, const lapack_int* ldb,
                    lapack_int* info, FORTRAN_STRLEN length) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_cgbtrs_(trans, n, kl, ku, nrhs, ab, ld, pivots, rhs, ldb, info,
                   length);
  } else {
    Solve(rhs, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbtrs_), decltype(LAPACK_cgbtrs_base)>);

void __real_zgbtrf_(const lapack_int*, const lapack_int*, const lapack_int*,
                    const lapack_int*, lapack_complex_double*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_zgbtrf_(const lapack_int* m, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku,
                    lapack_complex_double* ab, const lapack_int* ld,
                    lapack_int* pivots, lapack_int* info) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_zgbtrf_(m, n, kl, ku, ab, ld, pivots, info);
  } else {
    Factor(*m, *n, *kl, *ku, ab, pivots, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbtrf_), decltype(LAPACK_zgbtrf)>);

void __real_zgbtrs_(const char*, const lapack_int*, const lapack_int*,
                    const lapack_int*, const lapack_int*,
                    const lapack_complex_double*, const lapack_int*,
                    const lapack_int*, lapack_complex_double*,
                    const lapack_int*, lapack_int*, FORTRAN_STRLEN);
void __wrap_zgbtrs_(const char* trans, const lapack_int* n,
                    const lapack_int* kl, const lapack_int* ku,
                    const lapack_int* nrhs, const lapack_complex_double* ab,
                    const lapack_int* ld, const lapack_int* pivots,
                    lapack_complex_double* rhs, const lapack_int* ldb,
                    lapack_int* info, FORTRAN_STRLEN length) {
  ++g_calls;
  if (g_fault == Fault::kPass) {
    __real_zgbtrs_(trans, n, kl, ku, nrhs, ab, ld, pivots, rhs, ldb, info,
                   length);
  } else {
    Solve(rhs, info);
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbtrs_), decltype(LAPACK_zgbtrs_base)>);
}
// NOLINTEND(bugprone-reserved-identifier)
