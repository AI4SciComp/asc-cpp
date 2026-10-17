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
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "asc/dense/providers/lapack_dmd_qr.h"
#include "dmd_qr_entry.h"
#include "dmd_qr_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_dmd_qr_test::kColumn;
using asc_dmd_qr_test::kHost;
using asc_dmd_qr_test::kRow;
using asc_dmd_qr_test::Problem;
using asc_dmd_qr_test::Real;
using asc_dmd_qr_test::Scratch;
using asc_dmd_qr_test::Take;
using asc_dmd_qr_test::TestContext;
using asc_dmd_qr_test::WithoutAllocation;
namespace entry = asc_dmd_qr_entry;
constexpr std::array kLayouts{kRow, kColumn, kRow, kColumn,
                              kRow, kColumn, kRow};
asc::LapackDmdQrOptions Options(asc::LapackDmdSvd svd) {
  return {asc::LapackDmdScaling::kNone,
          asc::LapackDmdQrVectors::kExplicit,
          asc::LapackDmdExtra::kRefinement,
          svd,
          true,
          true,
          true,
          2};
}
template <typename T>
void Same(TestContext& test, const Problem<T>& a, const Problem<T>& b) {
  ASC_DENSE_TEST_CHECK(test, a.f.data == b.f.data);
  ASC_DENSE_TEST_CHECK(test, a.x.data == b.x.data);
  ASC_DENSE_TEST_CHECK(test, a.y.data == b.y.data);
  ASC_DENSE_TEST_CHECK(test, a.z.data == b.z.data);
  ASC_DENSE_TEST_CHECK(test, a.b.data == b.b.data);
  ASC_DENSE_TEST_CHECK(test, a.v.data == b.v.data);
  ASC_DENSE_TEST_CHECK(test, a.s.data == b.s.data);
  ASC_DENSE_TEST_CHECK(test, a.eigen == b.eigen);
  ASC_DENSE_TEST_CHECK(test, a.singular == b.singular);
  ASC_DENSE_TEST_CHECK(test, a.residual == b.residual);
  ASC_DENSE_TEST_CHECK(test, a.tau == b.tau);
  ASC_DENSE_TEST_EQ(test, a.rank, b.rank);
}
template <typename T>
void Rejections(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::LapackDmdSvd svd) {
  Problem<T> p(5, 4, kLayouts);
  p.Initialize();
  const auto options = Options(svd);
  const auto plan = Take(
      asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), Real<T>{0}));
  Scratch<T> storage(plan, false);
  const auto before = p;
  for (std::size_t role = 0; role < plan.regions.size(); ++role) {
    const auto region = storage.workspace.regions[role];
    if (region.size() == 0) {
      continue;
    }
    auto short_work = storage.workspace;
    short_work.regions[role] = {region.data(), region.size() - 1, kHost};
    entry::Reset();
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0}, p.rank,
                         plan, short_work, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, entry::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    Same(test, p, before);
  }
  auto changed = options;
  changed.orthogonal = false;
  entry::Reset();
  asc::LapackReport report;
  const auto stale = asc::Gedmdq(provider, changed, p.Buffers(), Real<T>{0},
                                 p.rank, plan, storage.workspace, report);
  ASC_DENSE_TEST_CHECK(test, !stale.ok());
  ASC_DENSE_TEST_EQ(test, entry::Calls(), 0U);
  Same(test, p, before);
  auto alias = storage.workspace;
  alias.regions[asc_dmd_test::kScalar] = {&report, sizeof(report), kHost};
  report.called_provider = true;
  report.native_info = 19;
  std::array<std::byte, sizeof(report)> report_before{};
  std::memcpy(report_before.data(), &report, sizeof(report));
  const auto aliased = asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0},
                                   p.rank, plan, alias, report);
  ASC_DENSE_TEST_CHECK(test, !aliased.ok());
  std::array<std::byte, sizeof(report)> report_after{};
  std::memcpy(report_after.data(), &report, sizeof(report));
  ASC_DENSE_TEST_CHECK(test, report_before == report_after);
  Same(test, p, before);
  storage.Guards(test);
}
template <typename T>
void NumericPreflight(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      asc::LapackDmdSvd svd) {
  for (bool nonfinite : {false, true}) {
    Problem<T> p(5, 4, kLayouts);
    p.Initialize();
    for (asc::extent_t j = 0; j < 3; ++j) {
      for (asc::extent_t i = 0; i < 5; ++i) {
        p.f.At(i, j) = T{};
      }
    }
    if (nonfinite) {
      p.f.At(0, 0) = T{std::numeric_limits<Real<T>>::infinity()};
    }
    const auto before = p;
    const auto options = Options(svd);
    const auto plan = Take(
        asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), Real<T>{0}));
    Scratch<T> storage(plan, false);
    asc::LapackReport report;
    entry::Reset();
    const auto status = asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0},
                                    p.rank, plan, storage.workspace, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, entry::Calls(), 0U);
    Same(test, p, before);
  }
}
template <typename T>
void Faults(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::LapackDmdSvd svd) {
  for (auto mode : {entry::Mode::kOmitInfo, entry::Mode::kOmitRank,
                    entry::Mode::kSvdFailure, entry::Mode::kEigenFailure,
                    entry::Mode::kNegativeInfo, entry::Mode::kImpossibleInfo,
                    entry::Mode::kImpossibleRank, entry::Mode::kNarrowInfo,
                    entry::Mode::kNarrowRank, entry::Mode::kWarning}) {
    Problem<T> p(5, 4, kLayouts);
    p.Initialize();
    const auto before = p;
    const auto options = Options(svd);
    const auto plan = Take(
        asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), Real<T>{0}));
    Scratch<T> storage(plan, false);
    asc::LapackReport report;
    entry::Reset(mode);
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0}, p.rank,
                         plan, storage.workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok() && report.called_provider);
    ASC_DENSE_TEST_EQ(test, entry::Calls(), 1U);
    if (mode == entry::Mode::kWarning) {
      ASC_DENSE_TEST_EQ(test, report.native_info, 4);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        asc::LapackOutcome::kAccuracyWarning);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
      ASC_DENSE_TEST_EQ(test, p.rank, 2);
    } else {
      Same(test, p, before);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnchanged);
      if (mode == entry::Mode::kOmitInfo) {
        ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
      }
      if (mode == entry::Mode::kSvdFailure ||
          mode == entry::Mode::kEigenFailure) {
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
        ASC_DENSE_TEST_EQ(test, report.outcome,
                          asc::LapackOutcome::kNonconvergence);
      }
    }
    p.Guards(test);
    storage.Guards(test);
  }
}
template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (asc::extent_t n : {0, 1}) {
    Problem<T> p(5, n, kLayouts);
    auto before = p;
    before.rank = 0;
    auto options = Options(asc::LapackDmdSvd::kBidiagonalQr);
    options.rank_selection = -1;
    const auto plan = Take(
        asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), Real<T>{0}));
    asc::LapackWorkspace workspace;
    asc::LapackReport report;
    entry::Reset();
    ASC_DENSE_TEST_CHECK(
        test, asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0}, p.rank,
                          plan, workspace, report)
                  .ok());
    ASC_DENSE_TEST_EQ(test, entry::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    Same(test, p, before);
  }
}
template <typename T>
void NativeWarning(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc::LapackDmdSvd svd) {
  for (auto scaling : {asc::LapackDmdScaling::kSnapshots,
                       asc::LapackDmdScaling::kConsistentSnapshots,
                       asc::LapackDmdScaling::kSuccessors}) {
    Problem<T> p(3, 3, kLayouts);
    const bool successor = scaling == asc::LapackDmdScaling::kSuccessors;
    // X=[0,e0],Y=[e0,2e0] warns under S/C but still identifies lambda=2.
    // X=[e0,0],Y=[0,2e0] warns under Y and has projected lambda=0.
    for (asc::extent_t j = 0; j < 3; ++j) {
      for (asc::extent_t i = 0; i < 3; ++i) {
        p.f.At(i, j) = T{};
      }
    }
    p.f.At(0, successor ? 0 : 1) = T{1};
    p.f.At(0, 2) = T{2};
    auto options = Options(svd);
    options.scaling = scaling;
    options.rank_selection = -1;
    const auto tolerance =
        Real<T>{32} * std::numeric_limits<Real<T>>::epsilon();
    const auto plan = Take(
        asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), tolerance));
    Scratch<T> scratch(plan, false);
    asc::LapackReport report;
    entry::Reset();
    const auto status = asc::Gedmdq(provider, options, p.Buffers(), tolerance,
                                    p.rank, plan, scratch.workspace, report);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info, 4);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, entry::Calls(), 1U);
    ASC_DENSE_TEST_EQ(test, p.rank, 1);
    const std::complex<Real<T>> expected(successor ? 0 : 2, 0);
    ASC_DENSE_TEST_CHECK(
        test, std::abs(p.eigen[1] - expected) <
                  Real<T>{512} * std::numeric_limits<Real<T>>::epsilon());
    p.Guards(test);
    scratch.Guards(test);
  }
}
template <typename T>
void SinglePair(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::LapackDmdSvd svd) {
  Problem<T> p(3, 2, kLayouts);
  p.Initialize();
  auto options = Options(svd);
  options.rank_selection = 1;
  const auto plan = Take(
      asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), Real<T>{0}));
  Scratch<T> scratch(plan, false);
  asc::LapackReport report;
  entry::Reset();
  ASC_DENSE_TEST_CHECK(
      test, asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0}, p.rank,
                        plan, scratch.workspace, report)
                .ok());
  ASC_DENSE_TEST_EQ(test, p.rank, 1);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  const auto bound = Real<T>{512} * std::numeric_limits<Real<T>>::epsilon();
  // The single snapshot [1,1,0] spans neither individual eigenvector.
  // Its projected eigenvalue is the arithmetic mean 2.5, with a nonzero
  // full-space residual .5 (real) or sqrt(1.25) (complex).
  ASC_DENSE_TEST_CHECK(
      test, std::abs(p.eigen[1] - std::complex<Real<T>>(2.5, 0)) < bound);
  const auto expected =
      asc::DenseBlasComplex<T> ? std::sqrt(Real<T>{1.25}) : Real<T>{0.5};
  ASC_DENSE_TEST_CHECK(test, std::abs(p.residual[1] - expected) < bound);
  p.Guards(test);
  scratch.Guards(test);
}
template <typename T>
int Run() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto svd :
       {asc::LapackDmdSvd::kBidiagonalQr, asc::LapackDmdSvd::kDivideAndConquer,
        asc::LapackDmdSvd::kQrPreconditioned, asc::LapackDmdSvd::kJacobi}) {
    Rejections<T>(test, provider, svd);
    NumericPreflight<T>(test, provider, svd);
    Faults<T>(test, provider, svd);
    NativeWarning<T>(test, provider, svd);
    SinglePair<T>(test, provider, svd);
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
