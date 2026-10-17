#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_condition.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
#include "asc/dense/providers/lapack_indefinite_refinement.h"
#include "indefinite_expert_faults.h"
#include "indefinite_expert_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc_indefinite_expert_test::Fault;
using asc_indefinite_expert_test::ObservedCalls;
using asc_indefinite_expert_test::Offset;
using asc_indefinite_expert_test::Route;
using asc_indefinite_expert_test::Scratch;
using asc_indefinite_expert_test::SetFault;
using asc_indefinite_expert_test::Vector;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kLower;
using asc_indefinite_test::kRow;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Matrix;
using asc_indefinite_test::Pivots;
using asc_indefinite_test::Raw;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::Value;
using asc_indefinite_test::WithoutAllocation;

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  bool hermitian;
  bool factored = false;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 12> a{};
  std::array<T, 12> af{};
  std::array<T, 12> b{};
  std::array<T, 12> x{};
  std::array<asc::index_t, 4> pivots{};
  std::array<Real, 3> ferr{};
  std::array<Real, 3> berr{};
  Real condition = -101;

  Sample(bool he, asc::DenseBlasTriangle tri, asc::DenseBlasLayout storage)
      : hermitian(he), triangle(tri), layout(storage) {
    a.fill(Value<T>(-31));
    af.fill(Value<T>(-33));
    b.fill(Value<T>(-35));
    x.fill(Value<T>(-37));
    ferr.fill(-41);
    berr.fill(-43);
    pivots = {-47, triangle == kUpper ? -1 : -2, triangle == kUpper ? -1 : -2,
              -47};
    const T off = Value<T>(2, 1);
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        a[Offset(i, j, layout, 3)] = i == j ? T{} : off;
      }
      b[Offset(i, 0, layout, 3)] = off * Real{3};
      x[Offset(i, 0, layout, 3)] = T{1};
    }
    if constexpr (asc::DenseBlasComplex<T>) {
      if (hermitian) {
        a[Offset(1, 0, layout, 3)] = std::conj(off);
        b[Offset(1, 0, layout, 3)] = std::conj(off) * Real{3};
      }
    }
    af = a;
  }
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, Sample<T>& s,
           Route route) {
  const auto a = Matrix(std::as_const(s.a), 2, 2, s.layout, 3);
  const auto af = Matrix(std::as_const(s.af), 2, 2, s.layout, 3);
  const auto b = Matrix(std::as_const(s.b), 2, 1, s.layout, 3);
  auto x = Matrix(s.x, 2, 1, s.layout, 3);
  const auto pivots = Raw(s.pivots, 2);
  auto ferr = Vector(s.ferr, 1);
  auto berr = Vector(s.berr, 1);
  if (route == Route::kCon) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::QueryHeconWorkspace(provider, s.triangle, af, pivots,
                                        asc::DenseBlasRealType<T>{3},
                                        s.condition);
      }
    }
    return asc::QuerySyconWorkspace(provider, s.triangle, af, pivots,
                                    asc::DenseBlasRealType<T>{3}, s.condition);
  }
  if (route == Route::kRfs) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::QueryHerfsWorkspace(provider, s.triangle, a, af, pivots, b,
                                        x, ferr, berr);
      }
    }
    return asc::QuerySyrfsWorkspace(provider, s.triangle, a, af, pivots, b, x,
                                    ferr, berr);
  }
  if (route == Route::kSv) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::QueryHesvWorkspace(
            provider, s.triangle, Matrix(s.a, 2, 2, s.layout, 3),
            Pivots(s.pivots, 2), Matrix(s.b, 2, 1, s.layout, 3));
      }
    }
    return asc::QuerySysvWorkspace(
        provider, s.triangle, Matrix(s.a, 2, 2, s.layout, 3),
        Pivots(s.pivots, 2), Matrix(s.b, 2, 1, s.layout, 3));
  }
  if (s.factored) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::QueryHesvxFactoredWorkspace(
            provider, s.triangle, a, af, pivots, b, x, s.condition, ferr, berr);
      }
    }
    return asc::QuerySysvxFactoredWorkspace(provider, s.triangle, a, af, pivots,
                                            b, x, s.condition, ferr, berr);
  }
  {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::QueryHesvxWorkspace(
            provider, s.triangle, a, Matrix(s.af, 2, 2, s.layout, 3),
            Pivots(s.pivots, 2), b, x, s.condition, ferr, berr);
      }
    }
    return asc::QuerySysvxWorkspace(
        provider, s.triangle, a, Matrix(s.af, 2, 2, s.layout, 3),
        Pivots(s.pivots, 2), b, x, s.condition, ferr, berr);
  }
}

template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider, Sample<T>& s,
                    Route route, const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  const auto a = Matrix(std::as_const(s.a), 2, 2, s.layout, 3);
  const auto af = Matrix(std::as_const(s.af), 2, 2, s.layout, 3);
  const auto b = Matrix(std::as_const(s.b), 2, 1, s.layout, 3);
  auto x = Matrix(s.x, 2, 1, s.layout, 3);
  const auto pivots = Raw(s.pivots, 2);
  auto ferr = Vector(s.ferr, 1);
  auto berr = Vector(s.berr, 1);
  if (route == Route::kCon) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::Hecon(provider, s.triangle, af, pivots,
                          asc::DenseBlasRealType<T>{3}, s.condition, plan,
                          workspace, report);
      }
    }
    return asc::Sycon(provider, s.triangle, af, pivots,
                      asc::DenseBlasRealType<T>{3}, s.condition, plan,
                      workspace, report);
  }
  if (route == Route::kRfs) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::Herfs(provider, s.triangle, a, af, pivots, b, x, ferr, berr,
                          plan, workspace, report);
      }
    }
    return asc::Syrfs(provider, s.triangle, a, af, pivots, b, x, ferr, berr,
                      plan, workspace, report);
  }
  if (route == Route::kSv) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::Hesv(provider, s.triangle, Matrix(s.a, 2, 2, s.layout, 3),
                         Pivots(s.pivots, 2), Matrix(s.b, 2, 1, s.layout, 3),
                         plan, workspace, report);
      }
    }
    return asc::Sysv(provider, s.triangle, Matrix(s.a, 2, 2, s.layout, 3),
                     Pivots(s.pivots, 2), Matrix(s.b, 2, 1, s.layout, 3), plan,
                     workspace, report);
  }
  if (s.factored) {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::HesvxFactored(provider, s.triangle, a, af, pivots, b, x,
                                  s.condition, ferr, berr, plan, workspace,
                                  report);
      }
    }
    return asc::SysvxFactored(provider, s.triangle, a, af, pivots, b, x,
                              s.condition, ferr, berr, plan, workspace, report);
  }
  {
    if constexpr (asc::DenseBlasComplex<T>) {
      if (s.hermitian) {
        return asc::Hesvx(provider, s.triangle, a,
                          Matrix(s.af, 2, 2, s.layout, 3), Pivots(s.pivots, 2),
                          b, x, s.condition, ferr, berr, plan, workspace,
                          report);
      }
    }
    return asc::Sysvx(provider, s.triangle, a, Matrix(s.af, 2, 2, s.layout, 3),
                      Pivots(s.pivots, 2), b, x, s.condition, ferr, berr, plan,
                      workspace, report);
  }
}

template <typename T>
void OneFault(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Sample<T>& sample, Route route, Fault fault) {
  SetFault(route, Fault::kNone);
  const auto plan = Take(
      WithoutAllocation(test, [&] { return Query(provider, sample, route); }));
  ASC_DENSE_TEST_EQ(test, ObservedCalls(), 0U);
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto old_a = sample.a;
  const auto old_af = sample.af;
  const auto old_b = sample.b;
  const auto old_x = sample.x;
  const auto old_pivots = sample.pivots;
  asc::LapackReport report;
  SetFault(route, fault);
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, sample, route, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, ObservedCalls(), 1U);
  SetFault(Route::kCon, Fault::kNone);
  const bool warning = fault == Fault::kNanDiagnostic;
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      warning ? asc::ErrorCode::kNumerical : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  asc::index_t expected_info = 0;
  if (fault == Fault::kNegativeInfo) {
    expected_info = -4;
  } else if (fault == Fault::kExcessInfo) {
    expected_info = 4;
  } else if (fault == Fault::kUnexpectedFactorInfo) {
    expected_info = 2;
  }
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), expected_info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    warning ? asc::LapackOutputValidity::kDocumentedPartial
                            : asc::LapackOutputValidity::kUnusable);
  if (route == Route::kCon || route == Route::kRfs || sample.factored) {
    ASC_DENSE_TEST_EQ(test, sample.pivots, old_pivots);
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(sample.a.data(), old_a.data(), sizeof(old_a)));
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(sample.af.data(), old_af.data(), sizeof(old_af)));
  }
  if (!warning) {
    ASC_DENSE_TEST_EQ(test, sample.pivots, old_pivots);
    const bool packed_a = sample.layout == kRow || sample.hermitian;
    if (route == Route::kSv && packed_a) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(sample.a.data(), old_a.data(), sizeof(old_a)));
    }
    if (route == Route::kVx && sample.layout == kRow) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(sample.af.data(), old_af.data(), sizeof(old_af)));
    }
    if (sample.layout == kRow || fault == Fault::kNegativeInfo) {
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(sample.x.data(), old_x.data(), sizeof(old_x)));
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(sample.b.data(), old_b.data(), sizeof(old_b)));
    }
  }
  scratch.Guards(test, workspace);
}

template <typename T>
void Preflight(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian, Route route, bool factored = false) {
  Sample<T> sample(hermitian, kUpper, kRow);
  sample.factored = factored;
  const auto plan = Take(Query(provider, sample, route));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto a_before = sample.a;
  const auto af_before = sample.af;
  const auto b_before = sample.b;
  const auto x_before = sample.x;
  const auto pivots_before = sample.pivots;
  const auto ferr_before = sample.ferr;
  const auto berr_before = sample.berr;
  auto reject = [&](const asc::LapackWorkspacePlan& p,
                    const asc::LapackWorkspace& w, asc::ErrorCode code) {
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = -99;
    SetFault(route, Fault::kNone);
    const auto status = WithoutAllocation(
        test, [&] { return Execute(provider, sample, route, p, w, report); });
    ASC_DENSE_TEST_EQ(test, ObservedCalls(), 0U);
    ASC_DENSE_TEST_EQ(test, status.code(), code);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, sample.a, a_before);
    ASC_DENSE_TEST_EQ(test, sample.af, af_before);
    ASC_DENSE_TEST_EQ(test, sample.b, b_before);
    ASC_DENSE_TEST_EQ(test, sample.x, x_before);
    ASC_DENSE_TEST_EQ(test, sample.pivots, pivots_before);
    ASC_DENSE_TEST_EQ(test, sample.ferr, ferr_before);
    ASC_DENSE_TEST_EQ(test, sample.berr, berr_before);
    ASC_DENSE_TEST_EQ(test, sample.condition, -101);
  };
  auto stale = plan;
  constexpr auto kScalar =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
  constexpr auto kInteger =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
  ++stale.regions[kScalar].preferred_entries;
  reject(stale, workspace, asc::ErrorCode::kInvalidState);
  auto short_work = workspace;
  auto& integer = short_work.regions[kInteger];
  integer = {integer.data(), integer.size() - 1, asc::MemorySpace::kHost};
  reject(plan, short_work, asc::ErrorCode::kInvalidArgument);
  auto inaccessible = workspace;
  const auto scalar = workspace.regions[kScalar];
  inaccessible.regions[kScalar] = {scalar.data(), scalar.size(),
                                   asc::MemorySpace::kPinnedHost};
  reject(plan, inaccessible, asc::ErrorCode::kMemoryAccess);
  auto aliases = workspace;
  aliases.regions[kScalar] = {
      route == Route::kCon ? sample.af.data() : sample.a.data(),
      sizeof(sample.a), asc::MemorySpace::kHost};
  reject(plan, aliases, asc::ErrorCode::kInvalidArgument);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = -99;
  aliases.regions[kScalar] = {&report, sizeof(report), asc::MemorySpace::kHost};
  SetFault(route, Fault::kNone);
  const auto result = WithoutAllocation(test, [&] {
    return Execute(provider, sample, route, plan, aliases, report);
  });
  ASC_DENSE_TEST_EQ(test, result.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, ObservedCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), -99);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const auto route :
           {Route::kCon, Route::kRfs, Route::kSv, Route::kVx}) {
        for (const auto fault :
             {Fault::kNegativeInfo, Fault::kExcessInfo, Fault::kPivotMinimum,
              Fault::kWorkNan, Fault::kNegativeDiagnostic,
              Fault::kNanDiagnostic}) {
          const bool factors = route == Route::kSv || route == Route::kVx;
          if ((!factors &&
               (fault == Fault::kPivotMinimum || fault == Fault::kWorkNan)) ||
              (route == Route::kSv && (fault == Fault::kNegativeDiagnostic ||
                                       fault == Fault::kNanDiagnostic))) {
            continue;
          }
          Sample<T> sample(hermitian, triangle, layout);
          OneFault(test, provider, sample, route, fault);
          ++cases;
        }
      }
    }
  }
  for (const auto route : {Route::kCon, Route::kRfs, Route::kSv, Route::kVx}) {
    Preflight<T>(test, provider, hermitian, route);
  }
  // FACT=F has a distinct input-mutation contract and rejects INFO in [1,n].
  // Execute every applicable return fault through the real factored route;
  // successful provider work still precedes the injected output faults.
  int factored_cases = 0;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const auto fault :
           {Fault::kNegativeInfo, Fault::kExcessInfo, Fault::kPivotMinimum,
            Fault::kWorkNan, Fault::kNegativeDiagnostic, Fault::kNanDiagnostic,
            Fault::kUnexpectedFactorInfo}) {
        Sample<T> sample(hermitian, triangle, layout);
        sample.factored = true;
        OneFault(test, provider, sample, Route::kVx, fault);
        ++factored_cases;
      }
    }
  }
  Preflight<T>(test, provider, hermitian, Route::kVx, true);
  std::printf(
      "expert synthetic fault cases=%d structural cases=20; "
      "factored return cases=%d structural cases=5\n",
      cases, factored_cases);
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
