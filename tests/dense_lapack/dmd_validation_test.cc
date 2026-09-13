#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "dmd_entry.h"
#include "dmd_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace support = asc_dmd_test;
namespace fault = asc_dmd_entry;
using support::Take;
using support::TestContext;
using Mode = fault::Mode;
constexpr std::array kLayouts{support::kRow, support::kColumn,
                              support::kRow, support::kColumn,
                              support::kRow, support::kColumn};
asc::LapackDmdOptions Options(asc::LapackDmdSvd svd) {
  return {asc::LapackDmdScaling::kNone,
          asc::LapackDmdVectors::kExplicit,
          asc::LapackDmdExtra::kRefinement,
          svd,
          true,
          -1};
}
template <typename T>
void Fixture(support::Problem<T>& p) {
  for (asc::extent_t j = 0; j < p.x.columns; ++j) {
    for (asc::extent_t i = 0; i < p.x.rows; ++i) {
      p.x.At(i, j) = i == j ? T{1} : T{};
      p.y.At(i, j) = i == j ? support::Value<T>(j + 2) : T{};
    }
  }
}
template <typename T>
void Unchanged(TestContext& test, const support::Problem<T>& p,
               const support::Problem<T>& before) {
  const auto equal = [&](const auto& first, const auto& second) {
    ASC_DENSE_TEST_CHECK(
        test, asc_tridiagonal_test::EqualBytes(&first, &second, sizeof(first)));
  };
  equal(p.x.data, before.x.data);
  equal(p.y.data, before.y.data);
  equal(p.z.data, before.z.data);
  equal(p.b.data, before.b.data);
  equal(p.w.data, before.w.data);
  equal(p.s.data, before.s.data);
  equal(p.eigen, before.eigen);
  equal(p.singular, before.singular);
  equal(p.residual, before.residual);
  ASC_DENSE_TEST_EQ(test, p.rank, before.rank);
}
template <typename T>
void FaultCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               asc::LapackDmdSvd svd, Mode mode) {
  support::Problem<T> p(3, 2, kLayouts);
  Fixture(p);
  const auto before = p;
  const auto options = Options(svd);
  fault::Reset(mode);
  const auto plan = Take(asc::QueryGedmdWorkspace(
      provider, options, p.Buffers(), support::Real<T>{0}));
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  support::Scratch<T> scratch(plan, false);
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gedmd(provider, options, p.Buffers(), support::Real<T>{0},
                      p.rank, plan, scratch.workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  if (mode == Mode::kWarning) {
    ASC_DENSE_TEST_EQ(test, report.native_info, 4);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, p.rank, 2);
  } else {
    Unchanged(test, p, before);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    if (mode == Mode::kSvdFailure || mode == Mode::kEigenFailure) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_EQ(test, report.native_info,
                        mode == Mode::kSvdFailure ? 2 : 3);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        asc::LapackOutcome::kNonconvergence);
    } else {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    }
    if (mode == Mode::kOmitInfo) {
      ASC_DENSE_TEST_CHECK(test, !report.native_info);
    }
    if (mode == Mode::kNegativeInfo) {
      ASC_DENSE_TEST_EQ(test, report.native_info, -8);
      ASC_DENSE_TEST_EQ(test, report.native_argument, 8);
    }
    if (mode == Mode::kOmitRank || mode == Mode::kNarrowRank) {
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    }
  }
  scratch.Guards(test);
  fault::Reset();
}
template <typename T>
void Structural(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::LapackDmdSvd svd) {
  support::Problem<T> p(3, 2, kLayouts);
  Fixture(p);
  const auto before = p;
  auto options = Options(svd);
  fault::Reset();
  const auto plan = Take(asc::QueryGedmdWorkspace(
      provider, options, p.Buffers(), support::Real<T>{0}));
  support::Scratch<T> scratch(plan, false);
  asc::LapackReport report;
  const auto run = [&](const asc::LapackWorkspacePlan& selected,
                       const asc::LapackWorkspace& work) {
    return asc::Gedmd(provider, options, p.Buffers(), support::Real<T>{0},
                      p.rank, selected, work, report);
  };
  for (std::size_t role = 0; role < plan.regions.size(); ++role) {
    if (plan.regions[role].minimum_entries == 0) {
      continue;
    }
    auto short_work = scratch.workspace;
    short_work.regions[role] = {short_work.regions[role].data(),
                                short_work.regions[role].size() - 1,
                                support::kHost};
    ASC_DENSE_TEST_CHECK(test, !run(plan, short_work).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    Unchanged(test, p, before);
  }
  auto stale = plan;
  --stale.regions[support::kScratch].minimum_entries;
  ASC_DENSE_TEST_CHECK(test, !run(stale, scratch.workspace).ok());
  options.rank_selection = 1;
  ASC_DENSE_TEST_CHECK(test, !run(plan, scratch.workspace).ok());
  options = Options(svd);
  auto alias = scratch.workspace;
  alias.regions[support::kLayout] = {
      p.x.data.data() + 1, scratch.workspace.regions[support::kLayout].size(),
      support::kHost};
  ASC_DENSE_TEST_CHECK(test, !run(plan, alias).ok());
  std::array<std::byte, sizeof(report)> old_report{};
  std::memcpy(old_report.data(), &report, sizeof(report));
  alias = scratch.workspace;
  alias.regions[support::kScratch] = {&report, sizeof(report), support::kHost};
  ASC_DENSE_TEST_CHECK(test, !run(plan, alias).ok());
  ASC_DENSE_TEST_CHECK(test, asc_tridiagonal_test::EqualBytes(
                                 &report, old_report.data(), sizeof(report)));
  Unchanged(test, p, before);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  for (const auto value :
       {support::Real<T>{0},
        std::numeric_limits<support::Real<T>>::quiet_NaN()}) {
    for (asc::extent_t j = 0; j < p.x.columns; ++j) {
      for (asc::extent_t i = 0; i < p.x.rows; ++i) {
        p.x.At(i, j) = T{value};
      }
    }
    const auto numerical_before = p;
    ASC_DENSE_TEST_EQ(test, run(plan, scratch.workspace).code(),
                      asc::ErrorCode::kNumerical);
    Unchanged(test, p, numerical_before);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
}
template <typename T>
void RealWarning(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 asc::LapackDmdSvd svd, asc::LapackDmdScaling scaling) {
  support::Problem<T> p(3, 3, kLayouts);
  // The first two pairs identify diag(2,3) on span(e0,e1). The third pair
  // is deliberately inconsistent, so all requested scaling modes warn.
  for (asc::extent_t j = 0; j < 3; ++j) {
    for (asc::extent_t i = 0; i < 3; ++i) {
      p.x.At(i, j) = i < 2 && (i == j || j == 2) ? T{1} : T{};
      p.y.At(i, j) = support::Value<T>(i + 2) * p.x.At(i, j);
      if (j == 2) {
        if (scaling == asc::LapackDmdScaling::kSuccessors) {
          p.y.At(i, j) = T{};
        } else {
          p.x.At(i, j) = T{};
        }
      }
    }
  }
  auto options = Options(svd);
  options.scaling = scaling;
  options.residuals = false;
  fault::Reset();
  const auto tolerance =
      support::Real<T>{32} * std::numeric_limits<support::Real<T>>::epsilon();
  const auto plan =
      Take(asc::QueryGedmdWorkspace(provider, options, p.Buffers(), tolerance));
  support::Scratch<T> scratch(plan, false);
  asc::LapackReport report;
  const auto status = asc::Gedmd(provider, options, p.Buffers(), tolerance,
                                 p.rank, plan, scratch.workspace, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info, 4);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kAccuracyWarning);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, p.rank, 2);
  if (p.rank != 2) {
    return;
  }
  const auto first = support::ToWide(p.eigen[1]);
  const auto second = support::ToWide(p.eigen[2]);
  // With successor scaling the normalized X has Gram[[5/4,1],[1,10/9]],
  // and Y*X^H=diag(1/2,1/3). Its least-squares map therefore has trace5/2
  // and determinant3/7. Snapshot scaling leaves the identified diag(2,3).
  const bool successor = scaling == asc::LapackDmdScaling::kSuccessors;
  const long double expected_trace = successor ? 2.5L : 5.L;
  const long double expected_determinant = successor ? 3.L / 7 : 6.L;
  const long double bound =
      256 * std::numeric_limits<support::Real<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(test,
                       std::abs(first + second - expected_trace) <= bound);
  ASC_DENSE_TEST_CHECK(
      test, std::abs(first * second - expected_determinant) <= bound);
  for (asc::extent_t i = 0; i < 3; ++i) {
    const auto expected = scaling == asc::LapackDmdScaling::kSnapshots && i < 2
                              ? support::Value<T>(i + 2)
                              : T{};
    ASC_DENSE_TEST_EQ(test, p.y.At(i, 2), expected);
  }
  scratch.Guards(test);
}
template <typename T>
void RankOne(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::LapackDmdSvd svd) {
  support::Problem<T> p(3, 2, kLayouts);
  Fixture(p);
  auto options = Options(svd);
  options.rank_selection = 1;
  fault::Reset();
  const auto plan = Take(asc::QueryGedmdWorkspace(
      provider, options, p.Buffers(), support::Real<T>{0}));
  support::Scratch<T> scratch(plan, false);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Gedmd(provider, options, p.Buffers(), support::Real<T>{0},
                       p.rank, plan, scratch.workspace, report)
                .ok());
  ASC_DENSE_TEST_EQ(test, p.rank, 1);
  long double projected = 0;
  for (asc::extent_t i = 0; i < 3; ++i) {
    projected += (i + 2) * std::norm(support::ToWide(p.x.At(i, 0)));
  }
  const long double bound =
      256 * std::numeric_limits<support::Real<T>>::epsilon();
  ASC_DENSE_TEST_CHECK(
      test, std::abs(support::ToWide(p.eigen[1]) - projected) <= bound);
  ASC_DENSE_TEST_EQ(test, p.eigen[2], std::complex<support::Real<T>>(-41, 3));
  scratch.Guards(test);
}
template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  support::Problem<T> p(4, 0, kLayouts);
  auto before = p;
  const auto options = Options(asc::LapackDmdSvd::kBidiagonalQr);
  fault::Reset();
  const auto plan = Take(asc::QueryGedmdWorkspace(
      provider, options, p.Buffers(), support::Real<T>{0}));
  const asc::LapackWorkspace workspace;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, asc::Gedmd(provider, options, p.Buffers(), support::Real<T>{0},
                       p.rank, plan, workspace, report)
                .ok());
  ASC_DENSE_TEST_EQ(test, p.rank, 0);
  before.rank = 0;
  Unchanged(test, p, before);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.native_info && !report.called_provider);
}
template <typename T>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto svd :
       {asc::LapackDmdSvd::kBidiagonalQr, asc::LapackDmdSvd::kDivideAndConquer,
        asc::LapackDmdSvd::kQrPreconditioned, asc::LapackDmdSvd::kJacobi}) {
    Structural<T>(test, provider, svd);
    RankOne<T>(test, provider, svd);
    for (const auto scaling : {asc::LapackDmdScaling::kSnapshots,
                               asc::LapackDmdScaling::kConsistentSnapshots,
                               asc::LapackDmdScaling::kSuccessors}) {
      RealWarning<T>(test, provider, svd, scaling);
    }
    for (const auto mode :
         {Mode::kOmitInfo, Mode::kOmitRank, Mode::kSvdFailure,
          Mode::kEigenFailure, Mode::kNegativeInfo, Mode::kImpossibleInfo,
          Mode::kWarning, Mode::kNarrowInfo, Mode::kNarrowRank}) {
      FaultCase<T>(test, provider, svd, mode);
    }
  }
  Empty<T>(test, provider);
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
