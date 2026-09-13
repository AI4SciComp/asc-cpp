#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_numerical_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace aa = asc_aasen_two_stage_test;
template <typename T>
aa::Sample<T> Range(int kind, bool he, bool upper,
                    asc::DenseBlasRealType<T> scale) {
  const int n = std::array{1, 2, 7, 2}[kind];
  aa::Sample<T> sample(n, he, upper);
  std::fill(sample.full.begin(), sample.full.end(), aa::base::Wide{});
  if (kind == 3) {
    // Nonsingular zero-diagonal 2-by-2 block. Its exact band pivots and
    // multipliers are representable at every scale, in both symmetries.
    sample.full[1] = aa::base::ToWide(aa::base::Value<T>(scale, scale));
    sample.full[2] = aa::base::Adjoint(sample.full[1], he);
  } else {
    for (int i = 0; i < n; ++i) {
      sample.full[i * n + i] = aa::base::Wide{scale, 0};
    }
  }
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (sample.Selected(i, j)) {
        const auto value = sample.full[i * n + j];
        sample.a[1 + j * sample.lda + i] =
            aa::base::Value<T>(value.real(), value.imag());
      }
    }
  }
  sample.before = sample.a;
  return sample;
}
template <typename T>
int Run(bool he, bool fidelity) {
  aa::base::TestContext test;
  const auto provider = aa::base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  using Real = asc::DenseBlasRealType<T>;
  const auto small = std::numeric_limits<Real>::min();
  const auto large = std::numeric_limits<Real>::max();
  // The first two points retain the already-required GBTRF dependency
  // failures. Remaining points cover the existing Aasen scaling controls.
  const std::array<Real, 8> scales{2 * std::numeric_limits<Real>::denorm_min(),
                                   small / 1024,
                                   small / 8,
                                   small,
                                   2 * small,
                                   1,
                                   large / 4,
                                   large * Real{.75}};
  int cases = 0;
  for (int kind = 0; kind < 4; ++kind) {
    for (auto scale : scales) {
      for (bool upper : {false, true}) {
        for (bool row : {false, true}) {
          for (int band : {0, 2}) {
            for (int work : {0, 2}) {
              std::printf(
                  "Two-stage range kind=%d scale=%La upper=%d row=%d band=%d "
                  "work=%d\n",
                  kind, static_cast<long double>(scale), upper, row, band,
                  work);
              aa::Case(test, provider, Range<T>(kind, he, upper, scale), row,
                       band, work, false, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf("Two-stage range cases=%d\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
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
