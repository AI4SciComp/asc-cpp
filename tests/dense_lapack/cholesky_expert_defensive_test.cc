#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_condition.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_equilibration.h"
#include "asc/dense/providers/lapack_cholesky_refinement.h"
#include "cholesky_expert_faults.h"
#include "cholesky_expert_test_support.h"
#include "cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_cholesky_expert_test::Calls;
using asc_cholesky_expert_test::CheckSuccess;
using asc_cholesky_expert_test::Fault;
using asc_cholesky_expert_test::Routine;
using asc_cholesky_expert_test::Scratch;
using asc_cholesky_expert_test::SetFault;
using asc_cholesky_expert_test::Vector;
using asc_cholesky_test::kLayouts;
using asc_cholesky_test::Layout;
using asc_cholesky_test::Matrix;
using asc_cholesky_test::Narrow;
using asc_cholesky_test::NotANumber;
using asc_cholesky_test::Take;
using asc_cholesky_test::TestContext;
using asc_cholesky_test::Triangle;
using asc_cholesky_test::WithoutAllocation;
using Equed = asc::LapackCholeskyEquilibration;
constexpr std::array kRoutines{Routine::kPocon, Routine::kPorfs,
                               Routine::kPosvx, Routine::kPoequ,
                               Routine::kPoequb};
std::size_t g_preflights = 0;
std::size_t g_defects = 0;

struct Case {
  Matrix<double> a{1, 1, Layout::kRowMajor};
  Matrix<double> af{1, 1, Layout::kRowMajor};
  Matrix<double> b{1, 1, Layout::kRowMajor};
  Matrix<double> x{1, 1, Layout::kRowMajor};
  std::array<double, 1> scales{11};
  std::array<double, 1> ferr{12};
  std::array<double, 1> berr{13};
  double rcond = 14;
  double scond = 15;
  double amax = 16;

  Case() {
    a(0, 0) = 4;
    af(0, 0) = 2;
    b(0, 0) = 6;
    x(0, 0) = 1;
  }

  asc::Result<asc::LapackWorkspacePlan> Query(
      const asc::ReferenceLapackProvider& provider, Routine routine) {
    switch (routine) {
      case Routine::kPocon:
        return asc::QueryPoconWorkspace(provider,
                                        asc_cholesky_test::kTriangles.front(),
                                        af.const_view(), 4., rcond);
      case Routine::kPorfs:
        return asc::QueryPorfsWorkspace(
            provider, Triangle::kLower, a.const_view(), af.const_view(),
            b.const_view(), x.view(), Vector(ferr, 1), Vector(berr, 1));
      case Routine::kPosvx:
        return asc::QueryPosvxWorkspace(
            provider, Triangle::kLower, a.const_view(), af.view(),
            b.const_view(), x.view(), Vector(ferr, 1), Vector(berr, 1), rcond);
      case Routine::kPoequ:
        return asc::QueryPoequWorkspace(provider, a.const_view(),
                                        Vector(scales, 1), scond, amax);
      case Routine::kPoequb:
        return asc::QueryPoequbWorkspace(provider, a.const_view(),
                                         Vector(scales, 1), scond, amax);
    }
    return asc::Status(asc::ErrorCode::kInvalidArgument);
  }

  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      Routine routine, const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
    switch (routine) {
      case Routine::kPocon:
        return asc::Pocon(provider, Triangle::kLower, af.const_view(), 4.,
                          rcond, plan, workspace, report);
      case Routine::kPorfs:
        return asc::Porfs(provider, Triangle::kLower, a.const_view(),
                          af.const_view(), b.const_view(), x.view(),
                          Vector(ferr, 1), Vector(berr, 1), plan, workspace,
                          report);
      case Routine::kPosvx:
        return asc::Posvx(provider, Triangle::kLower, a.const_view(), af.view(),
                          b.const_view(), x.view(), Vector(ferr, 1),
                          Vector(berr, 1), rcond, plan, workspace, report);
      case Routine::kPoequ:
        return asc::Poequ(provider, a.const_view(), Vector(scales, 1), scond,
                          amax, plan, workspace, report);
      case Routine::kPoequb:
        return asc::Poequb(provider, a.const_view(), Vector(scales, 1), scond,
                           amax, plan, workspace, report);
    }
    return asc::Status(asc::ErrorCode::kInvalidArgument);
  }

  void Unchanged(TestContext& test, const Case& before) const {
    a.CheckSame(test, before.a.bytes());
    af.CheckSame(test, before.af.bytes());
    b.CheckSame(test, before.b.bytes());
    x.CheckSame(test, before.x.bytes());
    ASC_DENSE_TEST_EQ(test, scales, before.scales);
    ASC_DENSE_TEST_EQ(test, ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, berr, before.berr);
    ASC_DENSE_TEST_EQ(test, rcond, before.rcond);
    ASC_DENSE_TEST_EQ(test, scond, before.scond);
    ASC_DENSE_TEST_EQ(test, amax, before.amax);
  }
};

void ExpectPreflight(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     Routine routine, Case& values,
                     const asc::LapackWorkspacePlan& plan,
                     const asc::LapackWorkspace& workspace) {
  const auto before = values;
  const auto calls = Calls();
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = -99;
  const auto status = WithoutAllocation(test, [&] {
    return values.Execute(provider, routine, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, Calls(), calls);
  values.Unchanged(test, before);
  ++g_preflights;
}

void Workspaces(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  for (const auto routine : kRoutines) {
    Case values;
    const auto plan = Take(values.Query(provider, routine));
    Scratch<double> scratch;
    auto workspace = scratch.view(plan);
    auto changed = plan;
    changed.regions[asc_cholesky_expert_test::kScalar].preferred_entries += 1;
    ExpectPreflight(test, provider, routine, values, changed, workspace);
    for (std::size_t kind = 0; kind < workspace.regions.size(); ++kind) {
      if (workspace.regions[kind].size() == 0) {
        continue;
      }
      auto short_workspace = workspace;
      const auto region = workspace.regions[kind];
      short_workspace.regions[kind] = {region.data(), region.size() - 1,
                                       asc::MemorySpace::kHost};
      ExpectPreflight(test, provider, routine, values, plan, short_workspace);
      auto inaccessible = workspace;
      inaccessible.regions[kind] = {region.data(), region.size(),
                                    asc::MemorySpace::kDevice};
      ExpectPreflight(test, provider, routine, values, plan, inaccessible);
      auto pinned = workspace;
      pinned.regions[kind] = {region.data(), region.size(),
                              asc::MemorySpace::kPinnedHost};
      ExpectPreflight(test, provider, routine, values, plan, pinned);
      auto alias = workspace;
      alias.regions[kind] = {values.a.view().data(), region.size(),
                             asc::MemorySpace::kHost};
      // POCON consumes AF, not A.
      if (routine == Routine::kPocon) {
        alias.regions[kind] = {values.af.view().data(), region.size(),
                               asc::MemorySpace::kHost};
      }
      ExpectPreflight(test, provider, routine, values, plan, alias);
      auto unaligned = workspace;
      unaligned.regions[kind] = {static_cast<std::byte*>(region.data()) + 1,
                                 region.size(), asc::MemorySpace::kHost};
      ExpectPreflight(test, provider, routine, values, plan, unaligned);
    }
    constexpr auto kUnused =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
    workspace.regions[kUnused] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return values.Execute(provider, routine, plan,
                                                       workspace, report);
                               }).ok());
  }
}

void MetadataAliases(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  for (const auto routine : kRoutines) {
    Case values;
    const auto before = values;
    const auto plan = Take(values.Query(provider, routine));
    Scratch<double> scratch;
    auto workspace = scratch.view(plan);
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = -97;
    for (auto& region : workspace.regions) {
      if (region.size() != 0) {
        ASC_DENSE_TEST_CHECK(test, region.size() <= sizeof(report));
        region = {&report, region.size(), asc::MemorySpace::kHost};
        break;
      }
    }
    const auto calls = Calls();
    const auto status = WithoutAllocation(test, [&] {
      return values.Execute(provider, routine, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, Calls(), calls);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), -97);
    values.Unchanged(test, before);
    ++g_preflights;
  }
}

void Malformed(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  Case values;
  Matrix<double> rectangular(1, 2, Layout::kRowMajor);
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryPoequWorkspace(provider, rectangular.const_view(),
                                      Vector(values.scales, 1), values.scond,
                                      values.amax)
                 .ok());
  ASC_DENSE_TEST_CHECK(test, !asc::QueryPoconWorkspace(
                                  provider, Triangle::kUpper,
                                  rectangular.const_view(), 1., values.rcond)
                                  .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryPorfsWorkspace(
                 provider, Triangle::kUpper, values.a.const_view(),
                 values.af.const_view(), values.b.const_view(), values.x.view(),
                 Vector(values.ferr, 1), Vector(values.ferr, 1))
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryPosvxWorkspace(
                 provider, Triangle::kUpper, values.a.const_view(),
                 values.a.view(), values.b.const_view(), values.x.view(),
                 Vector(values.ferr, 1), Vector(values.berr, 1), values.rcond)
                 .ok());
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kManaged,
        asc::MemorySpace::kDevice}) {
    const auto inaccessible =
        static_cast<asc::DenseBlasMatrixView<const double>>(
            values.a.view(space));
    ASC_DENSE_TEST_CHECK(
        test, !asc::QueryPoequbWorkspace(provider, inaccessible,
                                         Vector(values.scales, 1), values.scond,
                                         values.amax)
                   .ok());
  }
}

void ProviderDefects(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  for (const auto routine : kRoutines) {
    for (const auto fault :
         {Fault::kNegative, Fault::kMinimum, Fault::kExcess}) {
      Case values;
      const auto plan = Take(values.Query(provider, routine));
      Scratch<double> scratch;
      const auto workspace = scratch.view(plan);
      asc::LapackReport report;
      SetFault(routine, fault);
      const auto calls = Calls();
      const auto status = WithoutAllocation(test, [&] {
        return values.Execute(provider, routine, plan, workspace, report);
      });
      SetFault(routine, Fault::kNone);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, Calls(), calls + 1);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      if (fault == Fault::kNegative) {
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), -4);
        ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(-9999), 4);
      }
      if (fault == Fault::kMinimum) {
        ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
      }
      ++g_defects;
    }
  }
}

template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout : kLayouts) {
    Matrix<T> a(0, 0, layout);
    Matrix<T> af(0, 0, layout);
    Matrix<T> b(0, 2, layout);
    Matrix<T> x(0, 2, layout);
    std::array<Real, 2> s{};
    std::array<Real, 2> ferr{11, 12};
    std::array<Real, 2> berr{13, 14};
    Real rcond = 31;
    // Deliberate unread output sentinel in a fixed-underlying-type enum.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    Equed equed = static_cast<Equed>(255);
    asc::LapackReport report;
    const auto plan = Take(asc::QueryPorfsWorkspace(
        provider, Triangle::kUpper, a.const_view(), af.const_view(),
        b.const_view(), x.view(), Vector(ferr, 2), Vector(berr, 2)));
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Porfs(
                                     provider, Triangle::kUpper, a.const_view(),
                                     af.const_view(), b.const_view(), x.view(),
                                     Vector(ferr, 2), Vector(berr, 2), plan, {},
                                     report);
                               }).ok());
    CheckSuccess(test, provider, report, false);
    ASC_DENSE_TEST_EQ(test, ferr[0], Real{0});
    const auto new_plan = Take(asc::QueryPosvxWorkspace(
        provider, Triangle::kUpper, a.const_view(), af.view(), b.const_view(),
        x.view(), Vector(ferr, 2), Vector(berr, 2), rcond));
    ASC_DENSE_TEST_CHECK(
        test, asc::Posvx(provider, Triangle::kUpper, a.const_view(), af.view(),
                         b.const_view(), x.view(), Vector(ferr, 2),
                         Vector(berr, 2), rcond, new_plan, {}, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
    ASC_DENSE_TEST_EQ(test, rcond, Real{1});
    const auto eq_plan = Take(asc::QueryPosvxEquilibratedWorkspace(
        provider, Triangle::kUpper, a.view(), af.view(), equed, Vector(s, 0),
        b.view(), x.view(), Vector(ferr, 2), Vector(berr, 2), rcond));
    ASC_DENSE_TEST_CHECK(
        test, asc::PosvxEquilibrated(provider, Triangle::kUpper, a.view(),
                                     af.view(), equed, Vector(s, 0), b.view(),
                                     x.view(), Vector(ferr, 2), Vector(berr, 2),
                                     rcond, eq_plan, {}, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
    ASC_DENSE_TEST_EQ(test, equed, Equed::kNone);
    const auto f_plan = Take(asc::QueryPosvxFactoredWorkspace(
        provider, Triangle::kUpper, a.const_view(), af.const_view(),
        Equed::kDiagonal, Vector(s, 0), b.view(), x.view(), Vector(ferr, 2),
        Vector(berr, 2), rcond));
    ASC_DENSE_TEST_CHECK(
        test, asc::PosvxFactored(
                  provider, Triangle::kUpper, a.const_view(), af.const_view(),
                  Equed::kDiagonal, Vector(s, 0), b.view(), x.view(),
                  Vector(ferr, 2), Vector(berr, 2), rcond, f_plan, {}, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
  }
}

template <typename T>
void SuppliedPreflight(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(1, 1, Layout::kRowMajor);
  Matrix<T> af(1, 1, Layout::kColumnMajor);
  Matrix<T> b(1, 1, Layout::kRowMajor);
  Matrix<T> x(1, 1, Layout::kRowMajor);
  a(0, 0) = Narrow<T>({4, 0});
  af(0, 0) = Narrow<T>({2, 0});
  b(0, 0) = Narrow<T>({6, 0});
  std::array<Real, 1> scales{std::numeric_limits<Real>::quiet_NaN()};
  std::array<Real, 1> ferr{11};
  std::array<Real, 1> berr{12};
  Real rcond = 13;
  const auto before_b = b.bytes();
  const auto before_x = x.bytes();
  Scratch<T> scratch;
  asc::LapackReport report;
  for (const auto state : {Equed::kDiagonal, Equed::kNone}) {
    const auto plan = Take(asc::QueryPosvxFactoredWorkspace(
        provider, Triangle::kUpper, a.const_view(), af.const_view(), state,
        Vector(scales, 1), b.view(), x.view(), Vector(ferr, 1), Vector(berr, 1),
        rcond));
    const auto workspace = scratch.view(plan);
    const auto status = WithoutAllocation(test, [&] {
      return asc::PosvxFactored(
          provider, Triangle::kUpper, a.const_view(), af.const_view(), state,
          Vector(scales, 1), b.view(), x.view(), Vector(ferr, 1),
          Vector(berr, 1), rcond, plan, workspace, report);
    });
    if (state == Equed::kDiagonal) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_CHECK(test, !report.called_provider);
      b.CheckSame(test, before_b);
      x.CheckSame(test, before_x);
    } else {
      ASC_DENSE_TEST_CHECK(test, status.ok());
      CheckSuccess(test, provider, report, true);
      ASC_DENSE_TEST_EQ(test, x(0, 0), Narrow<T>({1.5L, 0}));
    }
  }
  af(0, 0) = T{};
  const auto plan = Take(asc::QueryPorfsWorkspace(
      provider, Triangle::kUpper, a.const_view(), af.const_view(),
      b.const_view(), x.view(), Vector(ferr, 1), Vector(berr, 1)));
  const auto workspace = scratch.view(plan);
  const auto status = WithoutAllocation(test, [&] {
    return asc::Porfs(provider, Triangle::kUpper, a.const_view(),
                      af.const_view(), b.const_view(), x.view(),
                      Vector(ferr, 1), Vector(berr, 1), plan, workspace,
                      report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-9999), 0);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
}

template <typename T>
void Warning(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout : kLayouts) {
    Matrix<T> a(2, 2, layout);
    Matrix<T> af(2, 2, layout);
    Matrix<T> b(2, 1, layout);
    Matrix<T> x(2, 1, layout);
    const Real tiny = std::numeric_limits<Real>::epsilon() / 8;
    a(0, 0) = Narrow<T>({1, 0});
    a(1, 1) = Narrow<T>({tiny, 0});
    a(0, 1) = T{};
    a(1, 0) = NotANumber<T>();
    b(0, 0) = Narrow<T>({1, 0});
    b(1, 0) = Narrow<T>({tiny, 0});
    std::array<Real, 1> ferr{11};
    std::array<Real, 1> berr{12};
    Real rcond = 13;
    const auto plan = Take(asc::QueryPosvxWorkspace(
        provider, Triangle::kUpper, a.const_view(), af.view(), b.const_view(),
        x.view(), Vector(ferr, 1), Vector(berr, 1), rcond));
    Scratch<T> scratch;
    const auto workspace = scratch.view(plan);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Posvx(provider, Triangle::kUpper, a.const_view(), af.view(),
                        b.const_view(), x.view(), Vector(ferr, 1),
                        Vector(berr, 1), rcond, plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), 3);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_NEAR(test, rcond, tiny, Real{0},
                        Real{16} * std::numeric_limits<Real>::epsilon());
    ASC_DENSE_TEST_CHECK(test, std::abs(x(0, 0) - T{1}) <=
                                   16 * std::numeric_limits<Real>::epsilon());
    ASC_DENSE_TEST_CHECK(test, std::abs(x(1, 0) - T{1}) <=
                                   16 * std::numeric_limits<Real>::epsilon());
    ASC_DENSE_TEST_CHECK(test, ferr[0] >= 0 && std::isfinite(ferr[0]));
    ASC_DENSE_TEST_CHECK(test, berr[0] >= 0 && std::isfinite(berr[0]));
  }
}

}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Workspaces(test, provider);
  MetadataAliases(test, provider);
  Malformed(test, provider);
  ProviderDefects(test, provider);
  Empty<float>(test, provider);
  Empty<double>(test, provider);
  Empty<std::complex<float>>(test, provider);
  Empty<std::complex<double>>(test, provider);
  SuppliedPreflight<float>(test, provider);
  SuppliedPreflight<double>(test, provider);
  SuppliedPreflight<std::complex<float>>(test, provider);
  SuppliedPreflight<std::complex<double>>(test, provider);
  Warning<float>(test, provider);
  Warning<double>(test, provider);
  Warning<std::complex<float>>(test, provider);
  Warning<std::complex<double>>(test, provider);
  std::cout << "Cholesky expert defensive: no-call preflights=" << g_preflights
            << " injected_provider_defects=" << g_defects << '\n';
  return test.Finish();
}
