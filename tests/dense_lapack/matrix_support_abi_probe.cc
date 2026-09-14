#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <type_traits>
#include <utility>

#include "../../src/dense/lapack/internal_matrix_support_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "installed_lu/matrix_support_numerical.h"
#include "installed_lu/normal_return_guard.h"
#if defined(ASC_ILASLC_HEADER)
#define ilaslc_ emitted_ilaslc
#include ASC_ILASLC_HEADER
#undef ilaslc_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_ilaslc), decltype(emitted_ilaslc)>);
#define ilaslr_ emitted_ilaslr
#include ASC_ILASLR_HEADER
#undef ilaslr_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_ilaslr), decltype(emitted_ilaslr)>);
#define iladlc_ emitted_iladlc
#include ASC_ILADLC_HEADER
#undef iladlc_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_iladlc), decltype(emitted_iladlc)>);
#define iladlr_ emitted_iladlr
#include ASC_ILADLR_HEADER
#undef iladlr_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_iladlr), decltype(emitted_iladlr)>);
#define ilaclc_ emitted_ilaclc
#include ASC_ILACLC_HEADER
#undef ilaclc_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_ilaclc), decltype(emitted_ilaclc)>);
#define ilaclr_ emitted_ilaclr
#include ASC_ILACLR_HEADER
#undef ilaclr_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_ilaclr), decltype(emitted_ilaclr)>);
#define ilazlc_ emitted_ilazlc
#include ASC_ILAZLC_HEADER
#undef ilazlc_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_ilazlc), decltype(emitted_ilazlc)>);
#define ilazlr_ emitted_ilazlr
#include ASC_ILAZLR_HEADER
#undef ilazlr_
static_assert(
    std::is_same_v<decltype(ASC_LAPACK_ilazlr), decltype(emitted_ilazlr)>);
#endif
namespace numerical = asc_matrix_support_numerical;
struct Counts {
  int cases = 0;
  int failures = 0;
  int guards = 0;
};
template <typename T, bool Rows>
lapack_int Native(lapack_int& m, lapack_int& n, T* a, lapack_int& leading) {
  if constexpr (std::is_same_v<T, float>) {
    return Rows ? ASC_LAPACK_ilaslr(&m, &n, a, &leading)
                : ASC_LAPACK_ilaslc(&m, &n, a, &leading);
  } else if constexpr (std::is_same_v<T, double>) {
    return Rows ? ASC_LAPACK_iladlr(&m, &n, a, &leading)
                : ASC_LAPACK_iladlc(&m, &n, a, &leading);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Rows ? ASC_LAPACK_ilaclr(&m, &n, a, &leading)
                : ASC_LAPACK_ilaclc(&m, &n, a, &leading);
  } else {
    return Rows ? ASC_LAPACK_ilazlr(&m, &n, a, &leading)
                : ASC_LAPACK_ilazlc(&m, &n, a, &leading);
  }
}
template <typename T, bool Rows>
void Case(int rows, int columns, int padding, int pattern, T marker,
          Counts& counts) {
  const int leading = std::max(1, rows) + padding;
  std::array<T, 96> storage{};
  storage.fill(T{71});
  for (int j = 0; j < columns; ++j) {
    for (int i = 0; i < rows; ++i) {
      const bool selected = pattern == j * rows + i ||
                            pattern == rows * columns + 1 ||
                            (pattern == rows * columns + 2 && (i + j) % 2 == 0);
      storage[2 + j * leading + i] = selected ? marker : T{-0.0};
    }
  }
  const auto before = storage;
  lapack_int expected = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (storage[2 + j * leading + i] != T{0}) {
        expected =
            std::max(expected, static_cast<lapack_int>(Rows ? i + 1 : j + 1));
      }
    }
  }
  std::array<lapack_int, 3> m{71, rows, 73};
  std::array<lapack_int, 3> n{71, columns, 73};
  std::array<lapack_int, 3> ld{71, leading, 73};
  const auto mb = m;
  const auto nb = n;
  const auto lb = ld;
  const lapack_int actual =
      Native<T, Rows>(m[1], n[1], storage.data() + 2, ld[1]);
  ++counts.cases;
  const bool intact = m == mb && n == nb && ld == lb &&
                      numerical::Bits(storage) == numerical::Bits(before);
  if (!intact) {
    ++counts.guards;
  }
  if (actual != expected) {
    ++counts.failures;
    if (counts.failures <= 24) {
      std::printf(
          "FAIL rows_operation=%d scalar_bytes=%zu m=%d n=%d "
          "padding=%d pattern=%d actual=%lld expected=%lld guards=%d\n",
          Rows, sizeof(T), rows, columns, padding, pattern,
          static_cast<long long>(actual), static_cast<long long>(expected),
          intact);
    }
  }
}
template <typename T, bool Rows>
void Run(Counts& count) {
  for (int m : {1, 2, 3, 5}) {
    for (int n : {1, 2, 3, 5}) {
      for (int padding : {0, 2}) {
        for (T marker : numerical::Markers<T>()) {
          for (int pattern = 0; pattern < m * n + 3; ++pattern) {
            Case<T, Rows>(m, n, padding, pattern, marker, count);
          }
        }
      }
    }
  }
  // Only source-proven quick-return orientations cross the native ABI.
  // The complementary empty shape must be handled locally by the future API.
  for (int other : {0, 1, 3}) {
    Case<T, Rows>(Rows ? 0 : other, Rows ? other : 0, 2, 0, T{1}, count);
  }
}
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  Counts columns;
  Counts rows;
  Run<float, false>(columns);
  Run<double, false>(columns);
  Run<std::complex<float>, false>(columns);
  Run<std::complex<double>, false>(columns);
  Run<float, true>(rows);
  Run<double, true>(rows);
  Run<std::complex<float>, true>(rows);
  Run<std::complex<double>, true>(rows);
  for (const auto& row :
       {std::pair{"columns", columns}, std::pair{"rows", rows}}) {
    std::printf(
        "group=%s cases=%d index_failures=%d guard_failures=%d "
        "provider_integer_bytes=%zu\n",
        row.first, row.second.cases, row.second.failures, row.second.guards,
        sizeof(lapack_int));
  }
  return columns.failures + columns.guards + rows.failures + rows.guards != 0
             ? 1
             : 0;
}
