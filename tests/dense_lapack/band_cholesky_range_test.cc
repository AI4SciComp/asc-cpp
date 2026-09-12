#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "band_cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_band_test;
using support::Take;
using support::TestContext;

template <typename T>
auto Scales() {
  using Real = asc::DenseBlasRealType<T>;
  return std::array<Real, 4>{2 * std::numeric_limits<Real>::min(),
                             std::numeric_limits<Real>::min() / 1024,
                             2 * std::numeric_limits<Real>::denorm_min(),
                             std::numeric_limits<Real>::max()};
}

template <typename T>
void CheckIdentityFactor(TestContext& test, const support::BandData<T>& band,
                         asc::DenseBlasRealType<T> scale) {
  const long double tolerance =
      64 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      if (!band.Selected(i, j)) {
        continue;
      }
      const auto entry = support::ToWide(band.values[band.Index(i, j)]);
      ASC_DENSE_TEST_CHECK(
          test, std::isfinite(entry.real()) && std::isfinite(entry.imag()));
      if (i == j) {
        ASC_DENSE_TEST_CHECK(test, entry.real() > 0 && entry.imag() == 0);
        // A single rounded sqrt needs only a few epsilons on squaring; use
        // a conservative 64-epsilon relative bound at every exponent.
        ASC_DENSE_TEST_CHECK(
            test,
            std::abs(entry.real() * entry.real() - scale) / scale <= tolerance);
      } else {
        ASC_DENSE_TEST_EQ(test, entry, support::Wide{});
      }
    }
  }
}

template <typename T>
void SolveIdentity(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   const support::BandData<T>& band,
                   asc::DenseBlasRealType<T> scale,
                   asc::DenseBlasLayout layout) {
  constexpr asc::extent_t kCount = 2;
  const auto ld = (layout == support::kColumn ? band.n : kCount) + 2;
  const T guard = support::Value<T>(-163, 29);
  std::vector<T> values(
      static_cast<std::size_t>(
          (layout == support::kColumn ? kCount : band.n) * ld + 2),
      guard);
  const auto index = [&](asc::extent_t i, asc::extent_t j) {
    return static_cast<std::size_t>(
        1 + (layout == support::kColumn ? j * ld + i : i * ld + j));
  };
  std::vector<unsigned char> selected(values.size());
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < kCount; ++j) {
      values[index(i, j)] = T{scale};
      selected[index(i, j)] = 1;
    }
  }
  const auto before = values;
  const auto factored = band.values;
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      values.data() + 1, band.n, kCount, layout, ld,
      {values.data(), values.size() * sizeof(T), support::kHost}));
  const auto plan = support::WithoutAllocation(test, [&] {
    return Take(asc::QueryPbtrsWorkspace(provider, band.ConstView(), rhs));
  });
  support::Storage<T> scratch(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Pbtrs(provider, band.ConstView(), rhs, plan, scratch.View(),
                      report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(factored, band.values));
  const long double tolerance =
      64 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (selected[i] != 0) {
      const auto x = support::ToWide(values[i]);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(x)));
      ASC_DENSE_TEST_CHECK(test, std::abs(x - support::Wide{1}) <= tolerance);
      const auto a = static_cast<long double>(scale);
      // Independent original-system residual, evaluated without underflow.
      ASC_DENSE_TEST_CHECK(
          test, std::abs(a * x - support::Wide{a}) / (a * (std::abs(x) + 1)) <=
                    tolerance);
    } else {
      ASC_DENSE_TEST_CHECK(test,
                           support::SameScalarBytes(values[i], before[i]));
    }
  }
  scratch.Check(test);
}

template <typename T>
void Exercise(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::extent_t n, asc::extent_t kd,
              asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
              bool blocked, asc::DenseBlasRealType<T> scale) {
  support::BandData<T> band(n, kd, triangle, layout);
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (band.Selected(i, j)) {
        // Complex imaginary diagonals are explicitly ignored by the factor
        // contract. NaN there must not contaminate a finite Hermitian input.
        band.values[band.Index(i, j)] = support::Value<T>(
            i == j ? scale : 0,
            i == j ? std::numeric_limits<long double>::quiet_NaN() : 0);
      }
    }
  }
  const auto before = band.values;
  const auto plan = support::WithoutAllocation(test, [&] {
    return Take(blocked ? asc::QueryPbtrfWorkspace(provider, band.View())
                        : asc::QueryPbtf2Workspace(provider, band.View()));
  });
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(before, band.values));
  support::Storage<T> scratch(plan);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return blocked
               ? asc::Pbtrf(provider, band.View(), plan, scratch.View(), report)
               : asc::Pbtf2(provider, band.View(), plan, scratch.View(),
                            report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  CheckIdentityFactor(test, band, scale);
  band.CheckPadding(test, before);
  scratch.Check(test);
  for (const auto rhs_layout : {support::kColumn, support::kRow}) {
    SolveIdentity(test, provider, band, scale, rhs_layout);
  }
  std::printf(
      "pb range routine=%s n=%lld kd=%lld triangle=%d layout=%d scale=%La\n",
      blocked ? "pbtrf" : "pbtf2", static_cast<long long>(n),
      static_cast<long long>(kd), static_cast<int>(triangle),
      static_cast<int>(layout), static_cast<long double>(scale));
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto scale : Scales<T>()) {
    for (const auto triangle : {support::kUpper, support::kLower}) {
      for (const auto layout : {support::kColumn, support::kRow}) {
        for (const bool blocked : {false, true}) {
          for (const auto shape : std::array<std::array<asc::extent_t, 2>, 3>{
                   {{1, 0}, {3, 2}, {96, 65}}}) {
            Exercise<T>(test, provider, shape[0], shape[1], triangle, layout,
                        blocked, scale);
          }
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
