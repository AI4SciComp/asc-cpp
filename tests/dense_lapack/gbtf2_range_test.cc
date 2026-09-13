#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_lu_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_test_support.h"

namespace {
namespace support = asc_lu_band_test;
using support::Take;
using support::TestContext;

template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kFactor = LAPACK_GLOBAL_SUFFIX(sgbtf2, SGBTF2);
};
template <>
struct Native<double> {
  static constexpr auto kFactor = LAPACK_GLOBAL_SUFFIX(dgbtf2, DGBTF2);
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kFactor = LAPACK_GLOBAL_SUFFIX(cgbtf2, CGBTF2);
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kFactor = LAPACK_GLOBAL_SUFFIX(zgbtf2, ZGBTF2);
};

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         bool mathematical) {
  using Real = asc::DenseBlasRealType<T>;
  for (const Real scale : {2 * std::numeric_limits<Real>::min(),
                           std::numeric_limits<Real>::min() / 1024,
                           2 * std::numeric_limits<Real>::denorm_min()}) {
    // A = scale*I has condition one. The exact multiplier below U is zero,
    // representable at every positive scale; no tolerance is needed for it.
    support::Band<T> band(2, 2, 1, 0);
    band.Put(0, 0, T{scale});
    band.Put(1, 0, T{});
    band.Put(1, 1, T{scale});
    const auto before = band.values;
    auto direct = before;
    support::Pivots pivots(2);
    const auto plan =
        Take(asc::QueryGbtf2Workspace(provider, band.View(), pivots.View()));
    support::Scratch<T> scratch(plan);
    asc::LapackReport report;
    const auto status = support::WithoutAllocation(test, [&] {
      return asc::Gbtf2(provider, band.View(), pivots.View(), plan,
                        scratch.View(), report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, pivots.values[1], 1);
    ASC_DENSE_TEST_EQ(test, pivots.values[2], 2);
    band.Padding(test, before);
    scratch.Check(test);
    pivots.Check(test);

    lapack_int n = 2;
    lapack_int kl = 1;
    lapack_int ku = 0;
    auto ld = static_cast<lapack_int>(band.ld);
    constexpr auto kGuard = std::numeric_limits<lapack_int>::max() - 19;
    std::array<lapack_int, 4> native_pivots{kGuard, kGuard, kGuard, kGuard};
    std::array<lapack_int, 3> info{kGuard, kGuard, kGuard};
    Native<T>::kFactor(&n, &n, &kl, &ku, direct.data() + 1, &ld,
                       native_pivots.data() + 1, info.data() + 1);
    ASC_DENSE_TEST_EQ(test, info[1], 0);
    ASC_DENSE_TEST_EQ(test, info.front(), kGuard);
    ASC_DENSE_TEST_EQ(test, info.back(), kGuard);
    ASC_DENSE_TEST_EQ(test, native_pivots.front(), kGuard);
    ASC_DENSE_TEST_EQ(test, native_pivots.back(), kGuard);
    ASC_DENSE_TEST_EQ(test, native_pivots[1], 1);
    ASC_DENSE_TEST_EQ(test, native_pivots[2], 2);
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(band.values, direct));

    const auto multiplier = support::ToWide(band.values[band.Index(1, 0)]);
    const bool finite =
        std::isfinite(multiplier.real()) && std::isfinite(multiplier.imag());
    std::printf("gbtf2 scale=%La multiplier=(%La,%La) INFO=0 finite=%d\n",
                static_cast<long double>(scale), multiplier.real(),
                multiplier.imag(), static_cast<int>(finite));
    if (mathematical) {
      ASC_DENSE_TEST_CHECK(test, finite);
      ASC_DENSE_TEST_EQ(test, multiplier, support::Wide{});
      band.Reconstruct(test, pivots);
    }
  }
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
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
  const bool mathematical = mode == "mathematical";
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider, mathematical);
  } else if (scalar == "d") {
    Run<double>(test, provider, mathematical);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, mathematical);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, mathematical);
  } else {
    return 2;
  }
  return test.Finish();
}
