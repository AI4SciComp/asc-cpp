#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_mixed_general.h"
#include "installed_lu/normal_return_guard.h"
#include "mixed_general_test_support.h"

namespace {
using asc_mixed_test::kColumn;
using asc_mixed_test::kHost;
using asc_mixed_test::kRow;
using asc_mixed_test::Narrow;
using asc_mixed_test::Problem;
using asc_mixed_test::Rhs;
using asc_mixed_test::Scratch;
using asc_mixed_test::Take;
using asc_mixed_test::TestContext;
using asc_mixed_test::ToWide;
using asc_mixed_test::Value;
using asc_mixed_test::Wide;
using asc_mixed_test::WithoutAllocation;
enum class Fixture : std::uint8_t {
  kExact,
  kOrdinary,
  kRange,
  kCollapsed,
  kSingular,
  kHilbert
};

template <typename T>
Wide Exact(asc::extent_t i, asc::extent_t j) {
  return ToWide(Value<T>(1 + i / 8.0L - j / 16.0L, 0.25L + j / 8.0L));
}
template <typename T>
void Initialize(Problem<T>& p, Fixture fixture) {
  for (asc::extent_t i = 0; i < p.a.rows; ++i) {
    for (asc::extent_t j = 0; j < p.a.rows; ++j) {
      T value{};
      switch (fixture) {
        case Fixture::kExact:
          value = Value<T>(i == j ? 2 : 0);
          break;
        case Fixture::kOrdinary:
        case Fixture::kRange:
          value = Value<T>(i < j ? 0.125L : -0.25L, i < j ? -0.125L : 0.0625L);
          if (i == j) {
            value = Value<T>(4 + i, 0.25L);
          }
          if (fixture == Fixture::kRange) {
            value *= 0x1p150;
          }
          break;
        case Fixture::kCollapsed:
          value = Value<T>(1 + (i == 1 && j == 1 ? 0x1p-30 : 0));
          break;
        case Fixture::kSingular:
          value = Value<T>(1);
          break;
        case Fixture::kHilbert:
          value = Value<T>(1.0L / (1 + i + j));
          break;
      }
      p.a.At(i, j) = value;
    }
  }
  for (asc::extent_t j = 0; j < p.b.columns; ++j) {
    for (asc::extent_t i = 0; i < p.b.rows; ++i) {
      Wide sum{};
      for (asc::extent_t k = 0; k < p.a.rows; ++k) {
        sum += ToWide(p.a.At(i, k)) * Exact<T>(k, j);
      }
      p.b.At(i, j) = Narrow<T>(sum);
    }
  }
}

template <typename T>
void CheckSolution(TestContext& test, const Problem<T>& original,
                   const Rhs<T>& computed, bool forward) {
  constexpr long double kTolerance =
      128 * std::numeric_limits<double>::epsilon();
  // Wider accumulation is an oracle for rounded input B. This bounded set
  // needs no exponent range beyond double: scale <= 2^150 and n <= 12.
  for (asc::extent_t j = 0; j < computed.columns; ++j) {
    for (asc::extent_t i = 0; i < computed.rows; ++i) {
      const auto x = ToWide(computed.At(i, j));
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(x.real()) && std::isfinite(x.imag()));
      if (forward) {
        ASC_DENSE_TEST_CHECK(test, std::abs(x - Exact<T>(i, j)) <=
                                       kTolerance * std::abs(Exact<T>(i, j)));
      }
      Wide residual = -ToWide(original.b.At(i, j));
      long double scale = std::abs(residual);
      for (asc::extent_t k = 0; k < computed.rows; ++k) {
        const auto term =
            ToWide(original.a.At(i, k)) * ToWide(computed.At(k, j));
        residual += term;
        scale += std::abs(term);
      }
      ASC_DENSE_TEST_CHECK(test, std::abs(residual) <= kTolerance * scale);
    }
  }
}

template <typename T>
void Solve(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout al,
           asc::DenseBlasLayout bl, asc::DenseBlasLayout xl, Fixture fixture,
           bool audit = true) {
  Problem<T> p(n, nrhs, al, bl, xl);
  Initialize(p, fixture);
  const auto original = p;
  const auto plan = Take(p.Query(provider));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackMixedSolveStatistics statistics;
  asc::LapackReport report;
  const auto call = [&] {
    return p.Run(provider, plan, workspace, statistics, report);
  };
  const auto status = audit ? WithoutAllocation(test, call) : call();
  const bool singular = fixture == Fixture::kSingular && n > 0;
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_EQ(test, report.called_provider, n != 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n != 0);
  ASC_DENSE_TEST_EQ(test, statistics.native_iteration.has_value(), n != 0);
  if (n > 0) {
    const auto iter = statistics.native_iteration.value_or(-999);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999),
                      singular ? 2 : 0);
    if (fixture == Fixture::kExact) {
      ASC_DENSE_TEST_EQ(test, iter, 0);
    } else if (fixture == Fixture::kOrdinary) {
      ASC_DENSE_TEST_CHECK(test, iter >= 0 && iter <= 30);
      // N=9 needs refinement; the smaller dyadic systems may solve
      // exactly in the initial low-precision pass (ITER=0 is valid).
      if (n == 9 && nrhs > 0) {
        ASC_DENSE_TEST_CHECK(test, iter > 0);
      }
    } else if (fixture == Fixture::kRange) {
      ASC_DENSE_TEST_EQ(test, iter, -2);
      ASC_DENSE_TEST_EQ(test, statistics.fallback,
                        asc::LapackMixedFallback::kConversionRange);
    } else if (fixture == Fixture::kCollapsed || singular) {
      ASC_DENSE_TEST_EQ(test, iter, -3);
      ASC_DENSE_TEST_EQ(test, statistics.fallback,
                        asc::LapackMixedFallback::kLowFactorization);
    } else if (fixture == Fixture::kHilbert) {
      ASC_DENSE_TEST_EQ(test, iter, -31);
      ASC_DENSE_TEST_EQ(test, statistics.fallback,
                        asc::LapackMixedFallback::kIterationLimit);
    }
    if (iter >= 0) {
      ASC_DENSE_TEST_EQ(test, p.a.data, original.a.data);
      ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto pivot = p.pivots[static_cast<std::size_t>(i + 1)];
      ASC_DENSE_TEST_CHECK(test, pivot >= i + 1 && pivot <= n);
    }
    std::printf(
        "fixture=%d n=%lld nrhs=%lld layouts=%d%d%d ITER=%lld INFO=%lld\n",
        static_cast<int>(fixture), static_cast<long long>(n),
        static_cast<long long>(nrhs), static_cast<int>(al),
        static_cast<int>(bl), static_cast<int>(xl),
        static_cast<long long>(iter),
        static_cast<long long>(report.native_info.value_or(-999)));
  }
  if (singular) {
    ASC_DENSE_TEST_EQ(test, p.x.data, original.x.data);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  } else {
    CheckSolution(test, original, p.x, fixture != Fixture::kHilbert);
  }
  ASC_DENSE_TEST_EQ(test, p.b.data, original.b.data);
  ASC_DENSE_TEST_EQ(test, p.pivots.front(), -73);
  ASC_DENSE_TEST_EQ(test, p.pivots[static_cast<std::size_t>(n + 1)], -73);
  p.a.Guards(test);
  p.x.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  for (const auto format : {kColumn, kRow}) {
    Problem<T> p(3, 2, format, format, format);
    Initialize(p, Fixture::kOrdinary);
    const auto original = p;
    const auto plan =
        Take(WithoutAllocation(test, [&] { return p.Query(provider); }));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackMixedSolveStatistics statistics{
        27, asc::LapackMixedFallback::kNone, asc::LapackScalarKind::kF32};
    for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
      const auto& region = workspace.regions[role];
      if (region.size() == 0) {
        continue;
      }
      auto short_workspace = workspace;
      short_workspace.regions[role] = {region.data(), region.size() - 1, kHost};
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(
          test,
          !p.Run(provider, plan, short_workspace, statistics, report).ok());
      ASC_DENSE_TEST_CHECK(test, !report.called_provider);
      ASC_DENSE_TEST_EQ(test, statistics.native_iteration.value_or(0), 27);
      ASC_DENSE_TEST_EQ(test, p.a.data, original.a.data);
      ASC_DENSE_TEST_EQ(test, p.b.data, original.b.data);
      ASC_DENSE_TEST_EQ(test, p.x.data, original.x.data);
      ASC_DENSE_TEST_EQ(test, p.pivots, original.pivots);
      const Scratch<T> pristine;
      ASC_DENSE_TEST_EQ(test, scratch.scalar, pristine.scalar);
      ASC_DENSE_TEST_EQ(test, scratch.lower, pristine.lower);
      ASC_DENSE_TEST_EQ(test, scratch.layout, pristine.layout);
      ASC_DENSE_TEST_EQ(test, scratch.real, pristine.real);
      ASC_DENSE_TEST_EQ(test, scratch.integer, pristine.integer);
    }
    asc::LapackReport report;
    ++p.x.ld;
    ASC_DENSE_TEST_CHECK(
        test, !p.Run(provider, plan, workspace, statistics, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_EQ(test, p.x.data, original.x.data);
  }
}

template <typename T>
void Aliases(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Problem<T> p(3, 3, kRow, kColumn, kRow);
  Initialize(p, Fixture::kOrdinary);
  const auto before = p;
  const auto plan = Take(p.Query(provider));
  const auto av = p.a.View();
  const asc::DenseBlasMatrixView<const T> bv = p.b.View();
  const auto xv = p.x.View();
  const auto pv = asc_mixed_test::Vector(p.pivots, 3);
  const auto query = [&](auto a, asc::DenseBlasMatrixView<const T> b, auto x) {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::QueryZcgesvWorkspace(provider, a, pv, b, x);
    } else {
      return asc::QueryDsgesvWorkspace(provider, a, pv, b, x);
    }
  };
  ASC_DENSE_TEST_CHECK(test, !query(av, av, xv).ok());
  ASC_DENSE_TEST_CHECK(test, !query(av, bv, av).ok());
  ASC_DENSE_TEST_CHECK(test, !query(av, bv, p.b.View()).ok());
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackMixedSolveStatistics statistics;
  asc::LapackReport report;
  auto overlap = workspace;
  const auto role = asc_mixed_test::kScalar;
  overlap.regions[role] = {p.a.data.data() + 1, workspace.regions[role].size(),
                           kHost};
  ASC_DENSE_TEST_CHECK(
      test, !p.Run(provider, plan, overlap, statistics, report).ok());
  auto misaligned = workspace;
  auto* bytes = reinterpret_cast<std::byte*>(scratch.scalar.data());
  misaligned.regions[role] = {bytes + 1, workspace.regions[role].size(), kHost};
  ASC_DENSE_TEST_CHECK(
      test, !p.Run(provider, plan, misaligned, statistics, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_EQ(test, p.a.data, before.a.data);
  ASC_DENSE_TEST_EQ(test, p.b.data, before.b.data);
  ASC_DENSE_TEST_EQ(test, p.x.data, before.x.data);
  ASC_DENSE_TEST_EQ(test, p.pivots, before.pivots);
}

template <typename T>
void RequiredMath(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  for (const double a : {std::numeric_limits<double>::denorm_min(),
                         2 * std::numeric_limits<double>::denorm_min(),
                         std::numeric_limits<double>::min(),
                         std::numeric_limits<double>::max()}) {
    for (const auto format : {kColumn, kRow}) {
      Problem<T> p(1, 1, format, format, format);
      p.a.At(0, 0) = Value<T>(a);
      p.b.At(0, 0) = Value<T>(a);
      const auto plan = Take(p.Query(provider));
      Scratch<T> scratch;
      asc::LapackMixedSolveStatistics statistics;
      asc::LapackReport report;
      const auto status =
          p.Run(provider, plan, scratch.Workspace(plan), statistics, report);
      const auto x = ToWide(p.x.At(0, 0));
      std::printf(
          "scalar_case A=B=%a X=(%La,%La) ITER=%lld INFO=%lld status=%d\n", a,
          x.real(), x.imag(),
          static_cast<long long>(statistics.native_iteration.value_or(-999)),
          static_cast<long long>(report.native_info.value_or(-999)),
          static_cast<int>(status.code()));
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(x - Wide{1, 0}) <=
                               128 * std::numeric_limits<double>::epsilon());
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), 0);
    }
  }
}
template <typename T>
int Run(bool extreme) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (extreme) {
    RequiredMath<T>(test, provider);
    return test.Finish();
  }
  for (const auto al : {kColumn, kRow}) {
    for (const auto bl : {kColumn, kRow}) {
      for (const auto xl : {kColumn, kRow}) {
        for (const auto n : {0, 1, 3, 9}) {
          for (const auto nrhs : {0, 1, 3}) {
            Solve<T>(test, provider, n, nrhs, al, bl, xl, Fixture::kExact);
            Solve<T>(test, provider, n, nrhs, al, bl, xl, Fixture::kOrdinary);
            Solve<T>(test, provider, n, nrhs, al, bl, xl, Fixture::kRange);
          }
        }
        Solve<T>(test, provider, 2, 2, al, bl, xl, Fixture::kCollapsed);
        Solve<T>(test, provider, 2, 2, al, bl, xl, Fixture::kSingular);
        Solve<T>(test, provider, 8, 2, al, bl, xl, Fixture::kHilbert);
      }
    }
  }
  Preflight<T>(test, provider);
  Aliases<T>(test, provider);
  std::array<int, 4> results{};
  std::array<std::thread, 4> threads;
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      TestContext local;
      const auto context = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      Solve<T>(local, context, 3, 2, kRow, kColumn, kRow,
               i % 2 == 0 ? Fixture::kOrdinary : Fixture::kRange, false);
      results[i] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  asc_lapack_test::NormalReturnGuard guard;
  if (argc < 2 || argc > 3) {
    return 2;
  }
  const std::string_view kind(argv[1]);
  if (kind == "d") {
    return Run<double>(argc == 3);
  }
  if (kind == "z") {
    return Run<std::complex<double>>(argc == 3);
  }
  return 2;
}
