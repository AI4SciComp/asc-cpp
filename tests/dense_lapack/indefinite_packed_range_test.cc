#include <array>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_numerical_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace packed = asc_packed_indefinite_test;
template <typename T>
void Fill(packed::Sample<T>& sample, asc::DenseBlasRealType<T> scale,
          int form) {
  for (int j = 0; j < sample.n; ++j) {
    for (int i = j; i < sample.n; ++i) {
      T value{};
      if (i == j) {
        value = packed::Value<T>(i % 2 == 0 ? 1 : -1,
                                 sample.hermitian ? 0 : 0.125L);
      } else if (form == 2) {
        value = packed::Value<T>(0.03125L, 0.015625L);
      }
      if (form == 1 && sample.n > 1 && i < 2 && j < 2) {
        value = i == j ? T{} : packed::Value<T>(1, 0.25L);
      }
      value *= scale;
      const auto wide = packed::ToWide(value);
      sample.full[i * sample.n + j] = wide;
      sample.full[j * sample.n + i] = packed::Adjoint(wide, sample.hermitian);
    }
  }
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        const auto wide = sample.full[i * sample.n + j];
        sample.a[sample.Offset(i, j)] =
            packed::Value<T>(wide.real(), wide.imag());
        if constexpr (asc::DenseBlasComplex<T>) {
          if (sample.hermitian && i == j) {
            sample.a[sample.Offset(i, j)].imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
      }
    }
  }
  sample.original = sample.a;
}
template <typename T>
int Run(bool he, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  packed::TestContext test;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider =
      packed::Take(asc::ReferenceLapackProvider::Create(context));
  const std::array scales{std::numeric_limits<Real>::min() / Real{8},
                          std::numeric_limits<Real>::min() / Real{2},
                          std::numeric_limits<Real>::min(),
                          std::numeric_limits<Real>::max() / Real{8},
                          std::numeric_limits<Real>::max() * Real{0.75}};
  int cases = 0;
  for (auto tri : {packed::kUpper, packed::kLower}) {
    for (auto layout : {packed::kColumn, packed::kRow}) {
      for (int n : {1, 2, 3, 8}) {
        for (int scale = 0; scale < 5; ++scale) {
          for (int form : {0, 1, 2}) {
            packed::Sample<T> sample(n, he, tri, layout, 0);
            Fill(sample, scales[scale], form);
            std::printf(
                "Packed range real_bytes=%zu complex=%d he=%d tri=%d layout=%d "
                "n=%d scale=%d form=%d\n",
                sizeof(Real), static_cast<int>(asc::DenseBlasComplex<T>),
                static_cast<int>(he), static_cast<int>(tri),
                static_cast<int>(layout), n, scale, form);
            std::fflush(stdout);
            packed::Case(test, provider, std::move(sample), fidelity);
            ++cases;
          }
        }
      }
    }
  }
  std::printf("Packed range cases=%d\n", cases);
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
