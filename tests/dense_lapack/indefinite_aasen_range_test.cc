#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_aasen_numerical_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_test;
using base::TestContext;
template <typename T>
aa::Sample<T> RangeSample(int kind, bool he, asc::DenseBlasTriangle tri,
                          asc::DenseBlasLayout layout,
                          asc::DenseBlasRealType<T> scale) {
  const std::array<int, 5> orders{1, 3, 67, 3, 67};
  const int n = orders[static_cast<std::size_t>(kind)];
  aa::Sample<T> sample(n, he, tri, layout, 0, false);
  sample.full.fill(base::Wide{});
  for (int i = 0; i < n; ++i) {
    sample.full[i * n + i] = base::Wide{scale, 0};
  }
  if (kind >= 3) {
    // Exact Aasen T has T(0,1)=t, all three leading diagonal entries zero,
    // T(1,2)=0, and the sole shifted multiplier L(2,1)=1/2. Every exact
    // factor is representable at each tested scale, including subnormals.
    for (int i = 0; i < 3; ++i) {
      sample.full[i * n + i] = base::Wide{};
    }
    const auto t = base::ToWide(base::Value<T>(scale, scale));
    sample.full[1] = t;
    sample.full[n] = base::Adjoint(t, he);
    sample.full[2] = t / 2.0L;
    sample.full[2 * n] = base::Adjoint(t / 2.0L, he);
  }
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (sample.Selected(i, j)) {
        const auto wide = sample.full[i * n + j];
        T value = base::Value<T>(wide.real(), wide.imag());
        if constexpr (asc::DenseBlasComplex<T>) {
          if (he && i == j) {
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
int Run(bool he, bool fidelity) {
  TestContext test;
  using Real = asc::DenseBlasRealType<T>;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const Real minimum = std::numeric_limits<Real>::min();
  const Real maximum = std::numeric_limits<Real>::max();
  const std::array<Real, 6> scales{minimum / 8, minimum / 2,         minimum, 1,
                                   maximum / 4, maximum * Real{0.75}};
  int cases = 0;
  for (int kind = 0; kind < 5; ++kind) {
    for (Real scale : scales) {
      for (const auto tri : {base::kUpper, base::kLower}) {
        for (const auto layout : {base::kColumn, base::kRow}) {
          for (int work : {0, 1, 2}) {
            std::printf(
                "Aasen range kind=%d scale=%Lg tri=%d layout=%d work=%d\n",
                kind, static_cast<long double>(scale), static_cast<int>(tri),
                static_cast<int>(layout), work);
            aa::Case(test, provider,
                     RangeSample<T>(kind, he, tri, layout, scale), work,
                     fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("Aasen range cases=%d mode=%s\n", cases,
              fidelity ? "fidelity" : "mathematical");
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
