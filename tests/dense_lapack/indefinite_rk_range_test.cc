#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_fixture.h"
#include "indefinite_rk_numerical_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_rk_test::Case;
using asc_rk_test::kColumn;
using asc_rk_test::kLower;
using asc_rk_test::kRow;
using asc_rk_test::kUpper;
using asc_rk_test::Sample;
using asc_rk_test::Take;
using asc_rk_test::TestContext;
using asc_rk_test::ToWide;
using asc_rk_test::Value;

template <typename T>
Sample<T> RangeSample(int kind, bool hermitian, asc::DenseBlasTriangle triangle,
                      asc::DenseBlasLayout layout,
                      asc::DenseBlasRealType<T> scale) {
  const std::array<int, 5> orders{1, 3, 67, 2, 3};
  const int n = orders[static_cast<std::size_t>(kind)];
  Sample<T> sample(n, hermitian, triangle, layout, 0, false);
  sample.full.fill(asc_rk_test::Wide{});
  if (kind < 3) {
    for (int i = 0; i < n; ++i) {
      sample.full[i * n + i] = asc_rk_test::Wide{scale, 0};
    }
  } else {
    const int first = kind == 3 || triangle == kLower ? 0 : 1;
    const T off = Value<T>(scale, scale);
    sample.full[first * n + first + 1] = ToWide(off);
    sample.full[(first + 1) * n + first] =
        asc_rk_test::Adjoint(ToWide(off), hermitian);
    if (kind == 4) {
      // A 2-block is eliminated first. Its inverse has zero diagonal, so the
      // remaining Schur diagonal is unchanged. Coupling/scale is bounded for
      // small input; the large-input multiplier 1/off is representable.
      const int remaining = triangle == kUpper ? 0 : 2;
      constexpr int kPaired = 1;
      const long double unit =
          scale <= 1 ? static_cast<long double>(scale) : 1.0L;
      sample.full[remaining * n + remaining] = asc_rk_test::Wide{4 * unit, 0};
      sample.full[remaining * n + kPaired] = asc_rk_test::Wide{unit / 4, 0};
      sample.full[kPaired * n + remaining] = asc_rk_test::Wide{unit / 4, 0};
    }
  }
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (sample.Selected(i, j)) {
        const auto wide = sample.full[i * n + j];
        T value = Value<T>(wide.real(), wide.imag());
        if constexpr (asc::DenseBlasComplex<T>) {
          if (hermitian && i == j) {
            value.imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
        sample.a[sample.Offset(i, j)] = value;
      }
    }
  }
  sample.original = sample.a;
  return sample;
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  TestContext test;
  using Real = asc::DenseBlasRealType<T>;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const Real minimum = std::numeric_limits<Real>::min();
  const Real maximum = std::numeric_limits<Real>::max();
  const std::array<Real, 6> scales{minimum / 8, minimum / 2,         minimum, 1,
                                   maximum / 4, maximum * Real{0.75}};
  int cases = 0;
  for (int kind = 0; kind < 5; ++kind) {
    for (const Real scale : scales) {
      for (const auto triangle : {kUpper, kLower}) {
        for (const auto layout : {kColumn, kRow}) {
          for (bool blocked : {false, true}) {
            for (int mode : {0, 3}) {
              if (!blocked && mode != 0) {
                continue;
              }
              std::printf(
                  "RK range kind=%d scale=%Lg tri=%d layout=%d blocked=%d "
                  "mode=%d\n",
                  kind, static_cast<long double>(scale),
                  static_cast<int>(triangle), static_cast<int>(layout), blocked,
                  mode);
              Case(test, provider,
                   RangeSample<T>(kind, hermitian, triangle, layout, scale),
                   false, blocked, mode, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  // Exact D after the selected 1-block step is +/-2*maximum, beyond the
  // destination type. This explicitly nonrepresentable control is checked by
  // wide arithmetic and native fidelity, not granted a finite-factor claim.
  const long double exact = 2 * static_cast<long double>(maximum);
  ASC_DENSE_TEST_CHECK(
      test, std::isfinite(exact) && exact > static_cast<long double>(maximum));
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (bool blocked : {false, true}) {
        for (int mode : {0, 3}) {
          if (!blocked && mode != 0) {
            continue;
          }
          auto sample = RangeSample<T>(3, hermitian, triangle, layout, maximum);
          sample.full = {};
          sample.full[0] = asc_rk_test::Wide{maximum, 0};
          sample.full[1] = asc_rk_test::Wide{maximum, 0};
          sample.full[2] = asc_rk_test::Wide{maximum, 0};
          sample.full[3] = asc_rk_test::Wide{-maximum, 0};
          for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
              if (sample.Selected(i, j)) {
                sample.a[sample.Offset(i, j)] =
                    Value<T>(sample.full[i * 2 + j].real());
              }
            }
          }
          sample.original = sample.a;
          Case(test, provider, sample, false, blocked, mode, true);
          ++cases;
        }
      }
    }
  }
  std::printf(
      "RK range cases=%d representable=360 nonrepresentable=12 mode=%s\n",
      cases, fidelity ? "fidelity" : "mathematical");
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false, fidelity);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity);
  }
  return 2;
}
