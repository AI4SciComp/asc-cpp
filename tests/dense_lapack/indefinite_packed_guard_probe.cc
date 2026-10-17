#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/dense/blas.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_native.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace packed = asc_packed_indefinite_test;
template <typename T>
int Run(bool he) {
  using Real = asc::DenseBlasRealType<T>;
  packed::TestContext test;
  const lapack_int n = 3;
  const char tri = 'L';
  const Real scale = std::numeric_limits<Real>::min() / Real{8};
  const T guard = packed::Value<T>(-173, 19);
  std::array<T, 24> a;
  a.fill(guard);
  // Finite nonsingular matrix: isolated2x2 indefinite block and scalar -a.
  // Lower packed entries A11,A21,A31,A22,A32,A33; no ignored NaNs.
  a[8] = T{};
  a[9] = packed::Value<T>(1, 0.25L) * scale;
  a[10] = T{};
  a[11] = T{};
  a[12] = T{};
  a[13] = T{-scale};
  const auto original = a;
  constexpr lapack_int kPivotGuard =
      std::numeric_limits<lapack_int>::max() - 73;
  std::array<lapack_int, 24> p;
  p.fill(kPivotGuard);
  for (int i = 8; i < 11; ++i) {
    p[i] = std::numeric_limits<lapack_int>::min();
  }
  std::array<lapack_int, 3> info{
      kPivotGuard, std::numeric_limits<lapack_int>::min(), kPivotGuard};
  packed::Native(he, &tri, &n, a.data() + 8, p.data() + 8, &info[1]);
  ASC_DENSE_TEST_EQ(test, info.front(), kPivotGuard);
  ASC_DENSE_TEST_EQ(test, info.back(), kPivotGuard);
  ASC_DENSE_TEST_CHECK(test, info[1] >= 0 && info[1] <= n);
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (i < 8 || i >= 14) {
      if (!packed::EqualBytes(&a[i], &original[i], sizeof(T))) {
        std::printf("native AP relative_index=%lld real=%Lg imag=%Lg\n",
                    static_cast<long long>(i) - 8, packed::ToWide(a[i]).real(),
                    packed::ToWide(a[i]).imag());
      }
      ASC_DENSE_TEST_CHECK(test,
                           packed::EqualBytes(&a[i], &original[i], sizeof(T)));
    }
    if (i < 8 || i >= 11) {
      if (p[i] != kPivotGuard) {
        std::printf("native IPIV relative_index=%lld value=%lld\n",
                    static_cast<long long>(i) - 8,
                    static_cast<long long>(p[i]));
      }
      ASC_DENSE_TEST_EQ(test, p[i], kPivotGuard);
    }
  }
  std::printf(
      "Packed native span real_bytes=%zu complex=%d he=%d INFO=%lld "
      "pivots=%lld,%lld,%lld\n",
      sizeof(Real), static_cast<int>(asc::DenseBlasComplex<T>),
      static_cast<int>(he), static_cast<long long>(info[1]),
      static_cast<long long>(p[8]), static_cast<long long>(p[9]),
      static_cast<long long>(p[10]));
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
