#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
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
  static constexpr auto kFactor = LAPACK_sgbtrf;
  static constexpr auto kSolve = LAPACK_sgbtrs_base;
};
template <>
struct Native<double> {
  static constexpr auto kFactor = LAPACK_dgbtrf;
  static constexpr auto kSolve = LAPACK_dgbtrs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kFactor = LAPACK_cgbtrf;
  static constexpr auto kSolve = LAPACK_cgbtrs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kFactor = LAPACK_zgbtrf;
  static constexpr auto kSolve = LAPACK_zgbtrs_base;
};

template <typename T>
auto Scales() {
  using Real = asc::DenseBlasRealType<T>;
  return std::array<Real, 3>{2 * std::numeric_limits<Real>::min(),
                             std::numeric_limits<Real>::min() / 1024,
                             2 * std::numeric_limits<Real>::denorm_min()};
}

template <typename T>
void FactorCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasRealType<T> scale, bool blocked,
                bool mathematical) {
  support::Band<T> band(2, 2, blocked ? 32 : 1, blocked ? 65 : 0);
  for (asc::extent_t j = 0; j < 2; ++j) {
    for (asc::extent_t i = std::max<asc::extent_t>(0, j - band.ku); i < 2;
         ++i) {
      band.Put(i, j, i == j ? T{scale} : T{});
    }
  }
  const auto before = band.values;
  auto direct = before;
  support::Pivots pivots(2);
  const auto plan =
      Take(asc::QueryGbtrfWorkspace(provider, band.View(), pivots.View()));
  support::Scratch<T> scratch(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbtrf(provider, band.View(), pivots.View(), plan,
                      scratch.View(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, pivots.values[1], 1);
  ASC_DENSE_TEST_EQ(test, pivots.values[2], 2);
  band.Padding(test, before);
  pivots.Check(test);
  scratch.Check(test);

  const lapack_int n = 2;
  const auto kl = static_cast<lapack_int>(band.kl);
  const auto ku = static_cast<lapack_int>(band.ku);
  const auto ld = static_cast<lapack_int>(band.ld);
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
  std::printf("gbtrf blocked=%d scale=%La multiplier=(%La,%La) finite=%d\n",
              static_cast<int>(blocked), static_cast<long double>(scale),
              multiplier.real(), multiplier.imag(), static_cast<int>(finite));
  if (mathematical) {
    ASC_DENSE_TEST_CHECK(test, finite);
    ASC_DENSE_TEST_EQ(test, multiplier, support::Wide{});
    band.Reconstruct(test, pivots);
  }
}

template <typename T>
void SolveMode(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const support::Band<T>& band, const support::Pivots& pivots,
               asc::ReferenceLuBandFactorView<T> factor,
               asc::DenseBlasTranspose operation, asc::DenseBlasLayout layout) {
  const auto factored = band.values;
  const auto swaps = pivots.values;
  T coefficient = band.values[band.Index(0, 0)];
  if constexpr (asc::DenseBlasComplex<T>) {
    if (operation == asc::DenseBlasTranspose::kConjugateTranspose) {
      coefficient = std::conj(coefficient);
    }
  }
  support::Rhs<T> rhs(band, 2, layout, operation);
  for (asc::extent_t j = 0; j < 2; ++j) {
    rhs.values[rhs.Index(0, j)] = coefficient;
    rhs.original[static_cast<std::size_t>(j)] = support::ToWide(coefficient);
    rhs.expected[static_cast<std::size_t>(j)] = support::Wide{1};
  }
  const auto before = rhs.values;
  const auto plan =
      Take(asc::QueryGbtrsWorkspace(provider, operation, factor, rhs.View()));
  support::Scratch<T> scratch(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbtrs(provider, operation, factor, rhs.View(), plan,
                      scratch.View(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(factored, band.values));
  ASC_DENSE_TEST_EQ(test, swaps, pivots.values);
  rhs.Check(test, band, before);
  scratch.Check(test);
  pivots.Check(test);

  const lapack_int n = 1;
  const lapack_int k = 0;
  const lapack_int nrhs = 2;
  const auto ld = static_cast<lapack_int>(band.ld);
  const lapack_int native_pivot = 1;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  char trans = 'N';
  if (operation != asc::DenseBlasTranspose::kNone) {
    trans = operation == asc::DenseBlasTranspose::kTranspose ? 'T' : 'C';
  }
  std::array<T, 2> direct{coefficient, coefficient};
  Native<T>::kSolve(&trans, &n, &k, &k, &nrhs, band.values.data() + 1, &ld,
                    &native_pivot, direct.data(), &n, &info, FORTRAN_STRLEN{1});
  ASC_DENSE_TEST_EQ(test, info, 0);
  for (asc::extent_t j = 0; j < 2; ++j) {
    const auto value = rhs.values[rhs.Index(0, j)];
    ASC_DENSE_TEST_EQ(test, value, T{1});
    ASC_DENSE_TEST_CHECK(test, support::SameScalarBytes(
                                   value, direct[static_cast<std::size_t>(j)]));
  }
  std::printf("gbtrs scalar=(%La,%La) operation=%d layout=%d X=(%La,%La)\n",
              support::ToWide(coefficient).real(),
              support::ToWide(coefficient).imag(), static_cast<int>(operation),
              static_cast<int>(layout), support::ToWide(direct[0]).real(),
              support::ToWide(direct[0]).imag());
}

template <typename T>
void SolveCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               asc::DenseBlasRealType<T> scale) {
  support::Band<T> band(1, 1, 0, 0);
  band.Put(0, 0, support::Value<T>(scale, scale));
  support::Pivots pivots(1);
  const auto plan =
      Take(asc::QueryGbtrfWorkspace(provider, band.View(), pivots.View()));
  support::Scratch<T> scratch(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc::Gbtrf(provider, band.View(), pivots.View(),
                                        plan, scratch.View(), report)
                                 .ok());
  const auto factor = Take(asc::ReferenceLuBandFactorView<T>::Create(
      provider, band.ConstView(), pivots.ConstView(), report));
  for (const auto operation : support::kOperations) {
    for (const auto layout : {support::kColumn, support::kRow}) {
      SolveMode(test, provider, band, pivots, factor, operation, layout);
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         std::string_view mode) {
  for (const auto scale : Scales<T>()) {
    if (mode == "solve") {
      SolveCase<T>(test, provider, scale);
    } else {
      for (const bool blocked : {false, true}) {
        FactorCase<T>(test, provider, scale, blocked, mode == "mathematical");
      }
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
  if (mode != "mathematical" && mode != "fidelity" && mode != "solve") {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider, mode);
  } else if (scalar == "d") {
    Run<double>(test, provider, mode);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, mode);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
