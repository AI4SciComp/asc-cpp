#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <thread>

#include "../../src/dense/lapack/internal_precision_conversion_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "precision_conversion_test_support.h"
namespace {
using asc_conversion_test::kColumn;
using asc_conversion_test::kHost;
using asc_conversion_test::kLayout;
using asc_conversion_test::kRow;
using asc_conversion_test::kScratch;
using asc_conversion_test::Output;
using asc_conversion_test::Problem;
using asc_conversion_test::Scratch;
using asc_conversion_test::Take;
using asc_conversion_test::TestContext;
using asc_conversion_test::ToWide;
using asc_conversion_test::Value;
using asc_conversion_test::WithoutAllocation;
template <typename Input>
Input Fixture(asc::extent_t i, asc::extent_t j) {
  // Values have independently known single-precision representations, including
  // signs of zero, subnormal endpoints and both finite range endpoints.
  constexpr auto kTiny = std::numeric_limits<float>::denorm_min();
  constexpr auto kSmall = std::numeric_limits<float>::min();
  constexpr auto kLarge = std::numeric_limits<float>::max();
  const std::array<long double, 12> values{
      0,      -0.0L,   0.125L, -0.5L,   kTiny,     -kTiny,
      kSmall, -kSmall, kLarge, -kLarge, 2 * kTiny, 0.5L * kTiny};
  const auto k = static_cast<std::size_t>(i + 3 * j);
  return Value<Input>(values[k % values.size()],
                      values[(k + 5) % values.size()]);
}
template <typename Input>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::extent_t m, asc::extent_t n, asc::DenseBlasLayout il,
              asc::DenseBlasLayout ol, bool audit = true) {
  Problem<Input> p(m, n, il, ol);
  for (asc::extent_t i = 0; i < m; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      p.input.At(i, j) = Fixture<Input>(i, j);
    }
  }
  const auto before = p;
  const auto plan = Take(p.Query(provider));
  Scratch<Input> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto call = [&] { return p.Run(provider, plan, workspace, report); };
  const auto status = audit ? WithoutAllocation(test, call) : call();
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, m > 0 && n > 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), m > 0 && n > 0);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  for (asc::extent_t i = 0; i < m; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      const auto original = ToWide(before.input.At(i, j));
      const auto expected =
          Value<Output<Input>>(original.real(), original.imag());
      ASC_DENSE_TEST_EQ(test, p.output.At(i, j), expected);
      const auto actual = ToWide(p.output.At(i, j));
      const auto wanted = ToWide(expected);
      ASC_DENSE_TEST_EQ(test, std::signbit(actual.real()),
                        std::signbit(wanted.real()));
      if constexpr (asc::DenseBlasComplex<Input>) {
        ASC_DENSE_TEST_EQ(test, std::signbit(actual.imag()),
                          std::signbit(wanted.imag()));
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
  p.output.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename Input>
void Exceptional(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasLayout il, asc::DenseBlasLayout ol) {
  using Real = asc::DenseBlasRealType<Input>;
  for (int fixture = 0; fixture < 4; ++fixture) {
    Problem<Input> p(3, 2, il, ol);
    for (asc::extent_t i = 0; i < 3; ++i) {
      for (asc::extent_t j = 0; j < 2; ++j) {
        p.input.At(i, j) = Value<Input>(1, 2);
      }
    }
    const auto infinity = std::numeric_limits<Real>::infinity();
    const auto nan = std::numeric_limits<Real>::quiet_NaN();
    Real special = fixture == 0 ? nan : infinity;
    if (fixture == 2) {
      special = -infinity;
    }
    if (fixture == 3) {
      if constexpr (sizeof(Real) > sizeof(float)) {
        special = Real{2} * std::numeric_limits<float>::max();
      }
    }
    p.input.At(2, 1) = Value<Input>(special, 0);
    if constexpr (asc::DenseBlasComplex<Input>) {
      if (fixture == 2) {
        p.input.At(2, 1) = Value<Input>(0, special);
      }
    }
    const auto before = p;
    const auto plan = Take(p.Query(provider));
    Scratch<Input> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status = WithoutAllocation(
        test, [&] { return p.Run(provider, plan, workspace, report); });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    constexpr bool kNarrow = sizeof(Input) > sizeof(Output<Input>);
    const bool range = kNarrow && fixture != 0;
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), range ? 1 : 0);
    if (range) {
      ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnchanged);
    } else {
      const auto value = ToWide(p.output.At(2, 1));
      ASC_DENSE_TEST_CHECK(
          test, !std::isfinite(value.real()) || !std::isfinite(value.imag()));
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
    }
    // Compare input object bytes, including NaN payloads and signed zeros.
    // This is mutation observation, not numerical equality of floating values.
    const auto bytes = std::as_bytes(std::span(p.input.data));
    const auto saved = std::as_bytes(std::span(before.input.data));
    ASC_DENSE_TEST_CHECK(test,
                         std::equal(bytes.begin(), bytes.end(), saved.begin()));
    p.output.Guards(test);
    scratch.Guards(test, workspace);
  }
}
template <typename Input>
void Rejections(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout il, asc::DenseBlasLayout ol) {
  Problem<Input> p(3, 2, il, ol);
  const auto before = p;
  const auto plan = Take(p.Query(provider));
  Scratch<Input> scratch;
  const auto workspace = scratch.Workspace(plan);
  for (std::size_t role : {kLayout, kScratch}) {
    if (workspace.regions[role].size() == 0) {
      continue;
    }
    auto short_work = workspace;
    short_work.regions[role] = {workspace.regions[role].data(),
                                workspace.regions[role].size() - 1, kHost};
    asc::LapackReport report;
    const auto status = WithoutAllocation(
        test, [&] { return p.Run(provider, plan, short_work, report); });
    ASC_DENSE_TEST_CHECK(test, !status.ok() && !report.called_provider);
    ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
    ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
  }
  auto alias = workspace;
  alias.regions[kScratch] = {p.output.View().data(),
                             p.output.View().reachable_storage().size(), kHost};
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, !p.Run(provider, plan, alias, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  const auto other = Take(
      Problem<Input>(3, 2, il, ol == kRow ? kColumn : kRow).Query(provider));
  ASC_DENSE_TEST_CHECK(test, !p.Run(provider, other, workspace, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
  ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
}
void Counts(TestContext& test) {
  using asc::internal_precision_conversion_counts::Count;
  for (const auto limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    ASC_DENSE_TEST_CHECK(test, Count(0, limit, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Count(limit, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Count(limit - 1, 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !Count(limit, 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !Count(1, limit, 1, limit).ok());
  }
  ASC_DENSE_TEST_CHECK(test, !Count(-1, 2, 1, 100).ok());
  ASC_DENSE_TEST_CHECK(test, !Count(2, -1, 2, 100).ok());
  ASC_DENSE_TEST_CHECK(test, !Count(3, 2, 101, 100).ok());
}
template <typename Input>
void Run(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto il : {kColumn, kRow}) {
    for (auto ol : {kColumn, kRow}) {
      for (asc::extent_t m : {0, 1, 2, 5}) {
        for (asc::extent_t n : {0, 1, 3, 4}) {
          Ordinary<Input>(test, provider, m, n, il, ol);
        }
      }
      Exceptional<Input>(test, provider, il, ol);
      Rejections<Input>(test, provider, il, ol);
    }
  }
  Counts(test);
  std::array<int, 4> failures{};
  std::array<std::thread, 4> threads;
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      TestContext local;
      const auto independent = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      Ordinary<Input>(local, independent, 5, 4, kRow, kColumn, false);
      failures[i] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (int count : failures) {
    ASC_DENSE_TEST_EQ(test, count, 0);
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
