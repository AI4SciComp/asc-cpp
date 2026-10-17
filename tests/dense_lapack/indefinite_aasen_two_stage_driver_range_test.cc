#include <algorithm>
#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_driver_numerical_support.h"
#include "indefinite_aasen_two_stage_driver_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace aa = asc_aasen_two_stage_driver_test;
namespace base = asc_indefinite_rook_test;
template <typename T>
aa::Sample<T> Range(int kind, bool he, bool upper, bool row, bool rhs_row,
                    int band, int work, asc::DenseBlasRealType<T> scale) {
  const int n = std::array{1, 2, 7, 2}[kind];
  aa::Sample<T> sample(n, 2, he, upper, row, rhs_row, band, work);
  auto& original = sample.original;
  std::fill(original.full.begin(), original.full.end(), base::Wide{});
  if (kind == 3) {
    original.full[1] = base::ToWide(base::Value<T>(scale, scale));
    original.full[2] = base::Adjoint(original.full[1], he);
  } else {
    for (int i = 0; i < n; ++i) {
      original.full[i * n + i] = base::Wide{scale, 0};
    }
  }
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (original.Selected(i, j)) {
        const auto value = original.full[i * n + j];
        original.a[1 + j * original.lda + i] =
            base::Value<T>(value.real(), value.imag());
      }
    }
  }
  original.before = original.a;
  sample.a = original.a;
  // Columns of A give exactly representable RHS at every retained scale.
  // A complex unit phase tests conjugation without introducing rounded tiny
  // RHS products or unrepresentable huge sums into the known-X oracle.
  std::fill(sample.solution.begin(), sample.solution.end(), base::Wide{});
  for (int j = 0; j < 2; ++j) {
    const int column = j == 0 ? 0 : n - 1;
    base::Wide phase{1, 0};
    if constexpr (asc::DenseBlasComplex<T>) {
      if (j == 1) {
        phase = {0, 1};
      }
    }
    sample.solution[j * n + column] = phase;
    for (int i = 0; i < n; ++i) {
      const auto value = original.full[i * n + column] * phase;
      sample.b[sample.BOffset(i, j)] =
          base::Value<T>(value.real(), value.imag());
    }
  }
  sample.before_b = sample.b;
  return sample;
}
template <typename T>
int Run(bool he, bool fidelity) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  using Real = asc::DenseBlasRealType<T>;
  const auto small = std::numeric_limits<Real>::min();
  const auto large = std::numeric_limits<Real>::max();
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
          for (bool rhs_row : {false, true}) {
            for (int band : {0, 2}) {
              for (int work : {0, 2}) {
                std::printf(
                    "Two-stage driver range kind=%d scale=%La upper=%d row=%d "
                    "rhs_row=%d band=%d work=%d\n",
                    kind, static_cast<long double>(scale), upper, row, rhs_row,
                    band, work);
                // Retain the driver's factor reconstruction and independent
                // solution/residual requirements over the same finite inputs.
                aa::Case(
                    test, provider,
                    Range<T>(kind, he, upper, row, rhs_row, band, work, scale),
                    fidelity);
                ++cases;
              }
            }
          }
        }
      }
    }
  }
  std::printf("Two-stage Aasen driver range cases=%d\n", cases);
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
