#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_driver_numerical_support.h"
#include "indefinite_aasen_solve_fixture.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_solve_test;
template <typename T>
aa::Sample<T> Range(int kind, int nrhs, bool he, asc::DenseBlasTriangle tri,
                    asc::DenseBlasLayout layout,
                    asc::DenseBlasLayout rhs_layout,
                    asc::DenseBlasRealType<T> scale) {
  const std::array<int, 3> orders{1, 2, 3};
  const int n = orders[static_cast<std::size_t>(kind)];
  aa::Sample<T> sample(n, nrhs, he, tri, layout, rhs_layout, 0, false);
  sample.full.fill(base::Wide{});
  const auto t =
      base::ToWide(base::Value<T>(scale, he && kind != 1 ? 0 : scale));
  if (kind == 1) {
    // An exactly invertible two-by-two T with unit triangular multipliers.
    // Both full original entries and the exact Aasen factors are representable.
    sample.full[1] = t;
    sample.full[2] = base::Adjoint(t, he);
  } else {
    for (int i = 0; i < n; ++i) {
      sample.full[i * n + i] = t;
    }
  }
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (sample.Selected(i, j)) {
        const auto value = sample.full[i * n + j];
        sample.a[sample.Offset(i, j)] =
            base::Value<T>(value.real(), value.imag());
      }
    }
  }
  for (int j = 0; j < nrhs; ++j) {
    const base::Wide exact{static_cast<long double>(j + 1) / 4, 0};
    for (int i = 0; i < n; ++i) {
      sample.solution[j * n + i] = exact;
    }
    for (int i = 0; i < n; ++i) {
      base::Wide value{};
      for (int k = 0; k < n; ++k) {
        value += sample.full[i * n + k] * exact;
      }
      sample.b[sample.BOffset(i, j)] =
          base::Value<T>(value.real(), value.imag());
    }
  }
  sample.original = sample.a;
  sample.original_b = sample.b;
  return sample;
}
template <typename T>
int Run(bool he, bool fidelity) {
  base::TestContext test;
  using Real = asc::DenseBlasRealType<T>;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const Real minimum = std::numeric_limits<Real>::min();
  const Real maximum = std::numeric_limits<Real>::max();
  const std::array<Real, 6> scales{minimum / 8, minimum / 2,         minimum, 1,
                                   maximum / 4, maximum * Real{0.75}};
  int cases = 0;
  for (int kind = 0; kind < 3; ++kind) {
    for (Real scale : scales) {
      for (auto tri : {base::kUpper, base::kLower}) {
        for (auto layout : {base::kColumn, base::kRow}) {
          for (auto rhs_layout : {base::kColumn, base::kRow}) {
            for (int nrhs : {1, 2, 3}) {
              std::printf(
                  "Aasen driver range kind=%d scale=%Lg tri=%d layout=%d "
                  "rhs_layout=%d nrhs=%d\n",
                  kind, static_cast<long double>(scale), static_cast<int>(tri),
                  static_cast<int>(layout), static_cast<int>(rhs_layout), nrhs);
              asc_aasen_driver_test::Case(
                  test, provider,
                  Range<T>(kind, nrhs, he, tri, layout, rhs_layout, scale),
                  false, false, fidelity);
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf("Aasen driver range cases=%d failed=%d\n", cases, test.Finish());
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
