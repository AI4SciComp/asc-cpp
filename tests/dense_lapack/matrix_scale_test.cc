#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_scale.h"
#include "installed_lu/normal_return_guard.h"
#include "internal_matrix_scale_counts.h"
#include "matrix_scale_test_support.h"
#include "tridiagonal_test_support.h"

namespace {
namespace support = asc_tridiagonal_test;
using support::Take;
using support::TestContext;
using Part = asc::LapackMatrixScalePart;
using Extent = asc::extent_t;

using asc_scale_test::Problem;
using asc_scale_test::SameBytes;

template <typename T>
void Numerical(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Problem<T>& problem, asc::DenseBlasRealType<T> from,
               asc::DenseBlasRealType<T> to, bool audit = true) {
  using Real = asc::DenseBlasRealType<T>;
  // This independently evaluated ratio needs range, not an assumption that the
  // operation's working-precision CTO/CFROM is representable. No test skip.
  static_assert(std::numeric_limits<long double>::max_exponent >=
                3 * std::numeric_limits<Real>::max_exponent);
  std::array<bool, 512> touched{};
  for (Extent i = 0; i < problem.rows; ++i) {
    for (Extent j = 0; j < problem.columns; ++j) {
      if (problem.Selected(i, j)) {
        const auto at = problem.Offset(i, j);
        touched[at] = true;
        const Real coefficient = ((i + j) % 2 == 0) ? Real{1} : Real{-0.5};
        problem.data[at] =
            support::Value<T>(from * coefficient, from * Real{0.25});
      }
    }
  }
  const auto original = problem.data;
  const auto plan = Take(problem.Query(provider));
  const auto workspace = problem.Workspace(plan);
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem.data = original;
    const Real numerator = reuse == 0 ? to : -to;
    asc::LapackReport report;
    const auto call = [&] {
      return problem.Run(provider, from, numerator, plan, workspace, report);
    };
    const auto status = audit ? support::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_CHECK(test, status.ok());
    const bool entered =
        problem.rows > 0 && problem.columns > 0 && from != numerator;
    ASC_DENSE_TEST_EQ(test, report.called_provider, entered);
    ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), entered);
    if (entered) {
      ASC_DENSE_TEST_EQ(test, *report.native_info, 0);
    }
    for (std::size_t i = 0; i < problem.data.size(); ++i) {
      if (!touched[i]) {
        ASC_DENSE_TEST_CHECK(test, SameBytes(problem.data[i], original[i]));
        continue;
      }
      const auto expected =
          support::ToWide(original[i]) * (static_cast<long double>(numerator) /
                                          static_cast<long double>(from));
      const auto observed = support::ToWide(problem.data[i]);
      for (const auto component : {false, true}) {
        const auto wanted = component ? expected.imag() : expected.real();
        const auto actual = component ? observed.imag() : observed.real();
        const auto tolerance =
            8 * static_cast<long double>(std::numeric_limits<Real>::epsilon()) *
                std::abs(wanted) +
            static_cast<long double>(std::numeric_limits<Real>::denorm_min());
        const bool correct =
            std::isfinite(actual) && std::abs(actual - wanted) <= tolerance;
        if (!correct) {
          std::fprintf(
              stderr,
              "LASCL %c m=%lld n=%lld layout=%d slot=%zu component=%d from=%La "
              "to=%La expected=%La actual=%La tolerance=%La\n",
              problem.type, static_cast<long long>(problem.rows),
              static_cast<long long>(problem.columns),
              static_cast<int>(problem.layout), i, static_cast<int>(component),
              static_cast<long double>(from),
              static_cast<long double>(numerator), wanted, actual, tolerance);
        }
        ASC_DENSE_TEST_CHECK(test, correct);
      }
    }
    const auto end = 1 + workspace.regions[support::kLayout].size() / sizeof(T);
    for (std::size_t i = 0; i < problem.packed.size(); ++i) {
      if (i == 0 || i >= end) {
        ASC_DENSE_TEST_EQ(test, problem.packed[i], support::Value<T>(-41, 23));
      }
    }
  }
}

template <typename T>
void Validation(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Problem<T>& p) {
  using Real = asc::DenseBlasRealType<T>;
  if (p.type == 'G') {
    // Deliberate invalid API option, representable in the fixed uint8_t base.
    // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
    ASC_DENSE_TEST_CHECK(test, !asc::QueryLasclWorkspace(
                                    provider, static_cast<Part>(99), p.Full())
                                    .ok());
    // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
  }
  const auto plan = Take(p.Query(provider));
  auto workspace = p.Workspace(plan);
  const auto before = p.data;
  const auto scratch = p.packed;
  asc::LapackReport report;
  for (Real from :
       {Real{0}, -Real{0}, std::numeric_limits<Real>::quiet_NaN()}) {
    ASC_DENSE_TEST_CHECK(
        test, !p.Run(provider, from, Real{2}, plan, workspace, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
  ASC_DENSE_TEST_CHECK(
      test, !p.Run(provider, Real{1}, std::numeric_limits<Real>::quiet_NaN(),
                   plan, workspace, report)
                 .ok());
  auto stale = plan;
  stale.regions[support::kLayout].minimum_entries += 1;
  ASC_DENSE_TEST_CHECK(
      test, !p.Run(provider, Real{1}, Real{2}, stale, workspace, report).ok());
  if (workspace.regions[support::kLayout].size() > 0) {
    auto short_workspace = workspace;
    auto& region = short_workspace.regions[support::kLayout];
    region = {region.data(), region.size() - 1, support::kHost};
    ASC_DENSE_TEST_CHECK(
        test,
        !p.Run(provider, Real{1}, Real{2}, plan, short_workspace, report).ok());
    auto alias = workspace;
    alias.regions[support::kLayout] = {
        p.data.data() + 1, workspace.regions[support::kLayout].size(),
        support::kHost};
    ASC_DENSE_TEST_CHECK(
        test, !p.Run(provider, Real{1}, Real{2}, plan, alias, report).ok());
  }
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(test, SameBytes(p.data, before));
  ASC_DENSE_TEST_CHECK(test, SameBytes(p.packed, scratch));
}

void CountChecks(TestContext& test) {
  namespace counts = asc::internal_matrix_scale_counts;
  for (const auto limit :
       {std::int64_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<std::int64_t>::max()}) {
    ASC_DENSE_TEST_CHECK(
        test,
        counts::Count('G', limit - 1, 1, 0, 0, limit - 1, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, !counts::Count('H', 1, limit, 0, 0, 1, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, counts::Count('Z', limit - 3, 1, 1, 0, 3, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, !counts::Count('Z', limit - 1, 1, 1, 0, 3, false, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         !counts::Count('B', 2, 3, 1, 1, 2, false, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         !counts::Count('Q', 2, 2, 2, 2, 3, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, counts::Count('G', 0, limit, 0, 0, 1, true, limit).ok());
  }
}

template <typename T>
int Run() {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  CountChecks(test);
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::array scales{
      std::array<Real, 2>{1, 2},
      std::array<Real, 2>{-3, 1},
      std::array<Real, 2>{1, 0},
      std::array<Real, 2>{1, 1},
      std::array<Real, 2>{std::numeric_limits<Real>::min(),
                          std::numeric_limits<Real>::max()},
      std::array<Real, 2>{std::numeric_limits<Real>::max(),
                          std::numeric_limits<Real>::min()},
      std::array<Real, 2>{std::numeric_limits<Real>::denorm_min(), 1},
      std::array<Real, 2>{1, std::numeric_limits<Real>::denorm_min()}};
  for (const auto layout : {support::kColumn, support::kRow}) {
    for (const auto type : {'G', 'L', 'U', 'H', 'B', 'Q', 'Z'}) {
      if (type == 'Z' && layout == support::kRow) {
        continue;  // The existing GBTRF descriptor is explicitly column-only.
      }
      for (Extent m : {0, 1, 2, 5}) {
        for (Extent n : {0, 1, 3, 5}) {
          if ((type == 'B' || type == 'Q') && m != n) {
            continue;
          }
          const Extent kl = std::min<Extent>(2, std::max<Extent>(0, m - 1));
          const Extent ku = std::min<Extent>(2, std::max<Extent>(0, n - 1));
          Problem<T> p(type, m, n, kl, ku, layout);
          Validation(test, provider, p);
          for (const auto& scale : scales) {
            Numerical(test, provider, p, scale[0], scale[1]);
          }
        }
      }
    }
  }
  std::array<int, 4> failures{};
  std::array<std::thread, 4> threads;
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      TestContext local;
      const auto independent = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      Problem<T> p(i % 2 == 0 ? 'H' : 'Q', 5, 5, 2, 2,
                   i < 2 ? support::kColumn : support::kRow);
      for (int repeat = 0; repeat < 8; ++repeat) {
        Numerical(local, independent, p, Real{1}, Real{2}, false);
      }
      failures[i] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (auto failure : failures) {
    ASC_DENSE_TEST_EQ(test, failure, 0);
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
