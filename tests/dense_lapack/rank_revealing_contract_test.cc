#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rank_revealing.h"
#include "installed_lu/normal_return_guard.h"
#include "rank_revealing_faults.h"
#include "rank_revealing_test_support.h"

namespace {
using asc::extent_t;
using asc_rank_revealing_test::Execute;
using asc_rank_revealing_test::Fault;
using asc_rank_revealing_test::ForeignExecutions;
using asc_rank_revealing_test::ForeignQueries;
using asc_rank_revealing_test::kPacking;
using asc_rank_revealing_test::kScalar;
using asc_rank_revealing_test::Layout;
using asc_rank_revealing_test::Matrix;
using asc_rank_revealing_test::Narrow;
using asc_rank_revealing_test::Query;
using asc_rank_revealing_test::Routine;
using asc_rank_revealing_test::Scratch;
using asc_rank_revealing_test::SetFault;
using asc_rank_revealing_test::Snapshot;
using asc_rank_revealing_test::Take;
using asc_rank_revealing_test::TestContext;
using asc_rank_revealing_test::Vector;
using asc_rank_revealing_test::WithoutAllocation;

template <typename T>
void Reset(Matrix<T>& a, Matrix<T>& b) {
  for (extent_t i = 0; i < 3; ++i) {
    for (extent_t j = 0; j < 2; ++j) {
      a(i, j) = Narrow<T>({i == j ? 2.0L : 1.0L, i == j ? 1.0L : 0.0L});
      b(i, j) = Narrow<T>({static_cast<long double>(i + j + 1), 1});
    }
  }
}

template <typename T, typename CheckFailure>
void BadWorkspaces(const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::DenseBlasMatrixView<T> av, CheckFailure check_failure) {
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    if (plan.regions[i].minimum_entries == 0) {
      continue;
    }
    auto insufficient = workspace;
    insufficient.regions[i] = {
        workspace.regions[i].data(),
        static_cast<std::size_t>(plan.regions[i].minimum_entries) *
                plan.regions[i].entry_bytes -
            1,
        asc::MemorySpace::kHost};
    check_failure(plan, insufficient);
    auto pinned = workspace;
    pinned.regions[i] = {workspace.regions[i].data(),
                         workspace.regions[i].size(),
                         asc::MemorySpace::kPinnedHost};
    check_failure(plan, pinned);
    auto alias = workspace;
    alias.regions[i] = {av.data(), av.reachable_storage().size(),
                        asc::MemorySpace::kHost};
    check_failure(plan, alias);
    auto misaligned = workspace;
    misaligned.regions[i] = {
        static_cast<std::byte*>(workspace.regions[i].data()) + 1,
        workspace.regions[i].size() - 1, asc::MemorySpace::kHost};
    check_failure(plan, misaligned);
  }
  auto stale = plan;
  ++stale.regions[kScalar].preferred_entries;
  check_failure(stale, workspace);
  stale = plan;
  --stale.total_byte_limit;
  check_failure(stale, workspace);
  auto overlap = workspace;
  overlap.regions[kPacking] = workspace.regions[kScalar];
  check_failure(plan, overlap);
}

template <typename T>
void BadQueries(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Routine routine, Matrix<T>& a, asc::DenseBlasMatrixView<T> av,
                asc::DenseBlasMatrixView<T> bv,
                std::vector<asc::index_t>& pivots, std::vector<T>& tau,
                asc::DenseBlasRealType<T> rcond, asc::index_t& rank,
                const asc::LapackWorkspacePlan& plan,
                const asc::LapackWorkspace& workspace,
                asc::LapackReport& report) {
  using Real = asc::DenseBlasRealType<T>;
  const auto pv = Vector(pivots, 2);
  const auto tv = Vector(tau, 2);
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    const auto bad_a = a.view(space);
    const auto q = WithoutAllocation(test, [&] {
      return Query(provider, routine, bad_a, bv, pv, tv, rcond, report);
    });
    ASC_DENSE_TEST_CHECK(test, !q.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
  const auto bad_pv = Vector(pivots, 1);
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return Query(provider, routine, av, bv, bad_pv,
                                             tv, rcond, report);
                              }).ok());
  const auto reversed_pv = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, 2, -1,
      {pivots.data(), pivots.size() * sizeof(pivots[0]),
       asc::MemorySpace::kHost}));
  ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                return Query(provider, routine, av, bv,
                                             reversed_pv, tv, rcond, report);
                              }).ok());
  if (routine == Routine::kGelsy) {
    for (const auto bad_rcond : {std::numeric_limits<Real>::infinity(),
                                 std::numeric_limits<Real>::quiet_NaN()}) {
      ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                    return Query(provider, routine, av, bv, pv,
                                                 tv, bad_rcond, report);
                                  }).ok());
    }
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return Execute(provider, routine, av, bv, pv,
                                                 tv, Real{0}, rank, plan,
                                                 workspace, report);
                                }).ok());
    const auto zero_plan =
        Take(Query(provider, routine, av, bv, pv, tv, Real{0}, report));
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return Execute(provider, routine, av, bv, pv,
                                                 tv, -Real{0}, rank, zero_plan,
                                                 workspace, report);
                                }).ok());
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return Query(provider, routine, av, av, pv,
                                               tv, rcond, report);
                                }).ok());
  } else {
    const auto bad_tv = Vector(tau, 1);
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return Query(provider, routine, av, bv, pv,
                                               bad_tv, rcond, report);
                                }).ok());
  }
}

template <typename T>
void Structural(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Routine routine, Layout al, Layout bl) {
  Matrix<T> a(3, 2, al);
  Matrix<T> b(3, 2, bl);
  Reset(a, b);
  const auto before_a = Snapshot(a.bytes());
  const auto before_b = Snapshot(b.bytes());
  std::vector<asc::index_t> pivots{0, std::numeric_limits<asc::index_t>::min()};
  const auto before_pivots = pivots;
  std::vector<T> tau(2, Narrow<T>({-21, 3}));
  const auto before_tau = tau;
  const auto av = a.view();
  const auto bv = b.view();
  const auto pv = Vector(pivots, 2);
  const auto tv = Vector(tau, 2);
  using Real = asc::DenseBlasRealType<T>;
  const Real rcond = Real{0.125};
  asc::LapackReport report;
  const auto plan =
      Take(Query(provider, routine, av, bv, pv, tv, rcond, report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  const auto calls = ForeignExecutions();
  asc::index_t rank = -41;
  const auto check_failure = [&](const auto& candidate_plan,
                                 const auto& candidate_workspace) {
    const auto result = WithoutAllocation(test, [&] {
      return Execute(provider, routine, av, bv, pv, tv, rcond, rank,
                     candidate_plan, candidate_workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, ForeignExecutions(), calls);
    a.CheckSame(test, before_a);
    b.CheckSame(test, before_b);
    ASC_DENSE_TEST_CHECK(test, pivots == before_pivots && tau == before_tau);
    ASC_DENSE_TEST_EQ(test, rank, -41);
  };
  BadWorkspaces(plan, workspace, av, check_failure);
  BadQueries(test, provider, routine, a, av, bv, pivots, tau, rcond, rank, plan,
             workspace, report);
  a.CheckSame(test, before_a);
  b.CheckSame(test, before_b);
  ASC_DENSE_TEST_CHECK(test, pivots == before_pivots && tau == before_tau);
  ASC_DENSE_TEST_EQ(test, rank, -41);
}

template <typename T>
void ProviderFailures(TestContext& test,
                      const asc::ReferenceLapackProvider& provider,
                      Routine routine, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 2, layout);
  Matrix<T> b(3, 2, Layout::kRowMajor);
  Reset(a, b);
  std::vector<asc::index_t> pivots{0, 1};
  std::vector<T> tau(2, Narrow<T>({-11, 1}));
  const auto av = a.view();
  const auto bv = b.view();
  const auto pv = Vector(pivots, 2);
  const auto tv = Vector(tau, 2);
  asc::LapackReport report;
  const auto plan =
      Take(Query(provider, routine, av, bv, pv, tv, Real{0.125}, report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  for (const auto fault :
       {Fault::kNegativeInfo, Fault::kMinimumInfo, Fault::kPositiveInfo,
        Fault::kQueryNan, Fault::kQueryNegative, Fault::kQueryImaginary,
        Fault::kQueryOverflow}) {
    const auto before = Snapshot(a.bytes());
    const auto rhs_before = Snapshot(b.bytes());
    const auto before_p = Snapshot(pivots);
    const auto before_t = Snapshot(tau);
    SetFault(routine, fault);
    const auto q = WithoutAllocation(test, [&] {
      return Query(provider, routine, av, bv, pv, tv, Real{0.125}, report);
    });
    SetFault(routine, Fault::kNone);
    ASC_DENSE_TEST_CHECK(test, !q.ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info);
    a.CheckSame(test, before);
    b.CheckSame(test, rhs_before);
    ASC_DENSE_TEST_CHECK(test, pivots == before_p && tau == before_t);
  }
  for (const auto fault :
       {Fault::kNegativeInfo, Fault::kMinimumInfo, Fault::kPositiveInfo,
        Fault::kPivotZero, Fault::kPivotLarge, Fault::kPivotDuplicate,
        Fault::kRankNegative, Fault::kRankLarge}) {
    if (routine == Routine::kGeqp3 &&
        (fault == Fault::kRankNegative || fault == Fault::kRankLarge)) {
      continue;
    }
    Reset(a, b);
    pivots = {0, 1};
    tau.assign(2, Narrow<T>({-11, 1}));
    const auto before = Snapshot(a.bytes());
    const auto rhs_before = Snapshot(b.bytes());
    const auto before_p = Snapshot(pivots);
    const auto before_t = Snapshot(tau);
    asc::index_t rank = -41;
    SetFault(routine, fault);
    const auto result = WithoutAllocation(test, [&] {
      return Execute(provider, routine, av, bv, pv, tv, Real{0.125}, rank, plan,
                     workspace, report);
    });
    SetFault(routine, Fault::kNone);
    ASC_DENSE_TEST_CHECK(test, !result.ok());
    ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    if (layout == Layout::kRowMajor) {
      a.CheckSame(test, before);
    }
    b.CheckSame(test, rhs_before);
    ASC_DENSE_TEST_CHECK(test, pivots == before_p && tau == before_t);
    ASC_DENSE_TEST_EQ(test, rank, -41);
    if (fault == Fault::kNegativeInfo) {
      ASC_DENSE_TEST_EQ(test, report.native_info, -3);
      ASC_DENSE_TEST_EQ(test, report.native_argument, 3);
    } else if (fault == Fault::kMinimumInfo) {
      ASC_DENSE_TEST_CHECK(test, !report.native_argument);
    } else if (fault == Fault::kPositiveInfo) {
      ASC_DENSE_TEST_EQ(test, report.native_info, 1);
    } else {
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    }
  }
}

template <typename T>
void EmptyCases(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto shape :
       {std::array<extent_t, 3>{0, 3, 2}, {3, 0, 2}, {3, 2, 0}, {0, 0, 0}}) {
    for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
      const auto m = shape[0];
      const auto n = shape[1];
      const auto nrhs = shape[2];
      Matrix<T> a(m, n, layout);
      Matrix<T> b(std::max(m, n), nrhs, layout);
      const auto original = Snapshot(a.bytes());
      const auto input = Snapshot(b.bytes());
      std::vector<asc::index_t> pivots(static_cast<std::size_t>(n), -912);
      const auto flags = pivots;
      const auto av = a.view();
      const auto bv = b.view();
      const auto pv = Vector(pivots, n);
      asc::LapackReport report;
      const auto queries = ForeignQueries();
      const auto calls = ForeignExecutions();
      const auto query = WithoutAllocation(test, [&] {
        return asc::QueryGelsyWorkspace(provider, av, bv, pv, Real{-1}, report);
      });
      ASC_DENSE_TEST_CHECK(test, query.ok());
      if (!query.ok()) {
        continue;
      }
      ASC_DENSE_TEST_EQ(test, ForeignQueries(), queries + 1);
      Scratch<T> scratch(*query, false);
      const auto workspace = scratch.view();
      asc::index_t rank = -41;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Gelsy(provider, av, bv, pv,
                                                     Real{-1}, rank, *query,
                                                     workspace, report);
                                 }).ok());
      ASC_DENSE_TEST_EQ(test, rank, 0);
      ASC_DENSE_TEST_EQ(test, ForeignExecutions(), calls);
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
      a.CheckSame(test, original);
      b.CheckSame(test, input);
      ASC_DENSE_TEST_CHECK(test, pivots == flags);
    }
  }
}

template <typename T>
void CutoffCases(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto cutoff : {Real{-1}, -Real{0}, Real{0}, Real{0.25}, Real{1},
                            Real{2}, std::numeric_limits<Real>::max()}) {
    Matrix<T> a(2, 2, Layout::kRowMajor);
    Matrix<T> b(2, 1, Layout::kColumnMajor);
    a(0, 0) = T{1};
    a(0, 1) = T{};
    a(1, 0) = T{};
    a(1, 1) = T{1};
    b(0, 0) = T{1};
    b(1, 0) = T{1};
    std::vector<asc::index_t> pivots{0, 0};
    const auto av = a.view();
    const auto bv = b.view();
    const auto pv = Vector(pivots, 2);
    asc::LapackReport report;
    const auto query = WithoutAllocation(test, [&] {
      return asc::QueryGelsyWorkspace(provider, av, bv, pv, cutoff, report);
    });
    ASC_DENSE_TEST_CHECK(test, query.ok());
    if (!query.ok()) {
      continue;
    }
    Scratch<T> scratch(*query, false);
    const auto workspace = scratch.view();
    asc::index_t rank = -41;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Gelsy(provider, av, bv, pv, cutoff,
                                                   rank, *query, workspace,
                                                   report);
                               }).ok());
    ASC_DENSE_TEST_EQ(test, rank, cutoff > 1 ? 1 : 2);
    // For cutoff>1 this is fidelity to a deliberately truncated rank decision,
    // not an original-system least-squares optimum claim.
    if (cutoff <= 1) {
      ASC_DENSE_TEST_CHECK(test, std::abs(b(0, 0) - T{1}) < Real{0.00001});
      ASC_DENSE_TEST_CHECK(test, std::abs(b(1, 0) - T{1}) < Real{0.00001});
    }
  }
}

template <typename T>
void RankDecisionCases(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const bool fixed_zero : {false, true}) {
    for (const Real cutoff : {Real{0.03125}, Real{0.125}}) {
      Matrix<T> a(2, 2, Layout::kRowMajor);
      Matrix<T> b(2, 1, Layout::kColumnMajor);
      a(0, 0) = fixed_zero ? T{} : T{1};
      a(0, 1) = fixed_zero ? T{1} : T{};
      a(1, 0) = T{};
      a(1, 1) = fixed_zero ? T{} : T{0.0625};
      b(0, 0) = T{1};
      b(1, 0) = fixed_zero ? T{} : T{0.0625};
      std::vector<asc::index_t> pivots{fixed_zero ? -91 : 0, 0};
      const auto av = a.view();
      const auto bv = b.view();
      const auto pv = Vector(pivots, 2);
      asc::LapackReport report;
      const auto query = WithoutAllocation(test, [&] {
        return asc::QueryGelsyWorkspace(provider, av, bv, pv, cutoff, report);
      });
      ASC_DENSE_TEST_CHECK(test, query.ok());
      if (!query.ok()) {
        continue;
      }
      Scratch<T> scratch(*query, false);
      const auto workspace = scratch.view();
      asc::index_t rank = -41;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Gelsy(provider, av, bv, pv,
                                                     cutoff, rank, *query,
                                                     workspace, report);
                                 }).ok());
      const asc::index_t threshold_rank = cutoff < Real{0.0625} ? 2 : 1;
      ASC_DENSE_TEST_EQ(test, rank, fixed_zero ? 0 : threshold_rank);
      ASC_DENSE_TEST_CHECK(test, report.native_info == 0);
      ASC_DENSE_TEST_CHECK(test, pivots == std::vector<asc::index_t>({1, 2}));
      ASC_DENSE_TEST_CHECK(
          test, std::abs(b(0, 0) - (fixed_zero ? T{} : T{1})) < Real{0.00001});
      const T expected_second = rank == 2 ? T{1} : T{};
      ASC_DENSE_TEST_CHECK(test,
                           std::abs(b(1, 0) - expected_second) < Real{0.00001});
      // These are exact rank-decision/fidelity checks. In particular, the
      // fixed-zero system's actual returned X=0 is NOT its optimum X=[0,1].
      // That mathematical gate remains a separately recorded failing probe.
    }
  }
}

template <typename T>
void SingularNonpositiveCutoffs(TestContext& test,
                                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const Real cutoff : {Real{-1}, Real{0}}) {
    Matrix<T> a(2, 2, Layout::kRowMajor);
    Matrix<T> b(2, 1, Layout::kColumnMajor);
    a(0, 0) = T{1};
    a(0, 1) = T{};
    a(1, 0) = T{};
    a(1, 1) = T{};
    b(0, 0) = T{1};
    b(1, 0) = T{1};
    std::vector<asc::index_t> pivots{0, 0};
    const auto av = a.view();
    const auto bv = b.view();
    const auto pv = Vector(pivots, 2);
    asc::LapackReport report;
    const auto plan = WithoutAllocation(test, [&] {
      return asc::QueryGelsyWorkspace(provider, av, bv, pv, cutoff, report);
    });
    ASC_DENSE_TEST_CHECK(test, plan.ok());
    if (!plan.ok()) {
      continue;
    }
    Scratch<T> scratch(*plan, false);
    const auto workspace = scratch.view();
    asc::index_t rank = -41;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Gelsy(provider, av, bv, pv, cutoff,
                                                   rank, *plan, workspace,
                                                   report);
                               }).ok());
    ASC_DENSE_TEST_EQ(test, rank, 2);
    ASC_DENSE_TEST_CHECK(test, report.native_info == 0);
    ASC_DENSE_TEST_CHECK(test, !std::isfinite(std::real(b(1, 0))));
    // A nonpositive cutoff admits the zero pivot and its actual nonfinite
    // division. This is raw source-result fidelity, not an optimum claim.
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto routine : {Routine::kGeqp3, Routine::kGelsy}) {
    for (const auto al : {Layout::kColumnMajor, Layout::kRowMajor}) {
      for (const auto bl : {Layout::kColumnMajor, Layout::kRowMajor}) {
        Structural<T>(test, provider, routine, al, bl);
      }
      ProviderFailures<T>(test, provider, routine, al);
    }
  }
  EmptyCases<T>(test, provider);
  CutoffCases<T>(test, provider);
  RankDecisionCases<T>(test, provider);
  SingularNonpositiveCutoffs<T>(test, provider);
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  ASC_DENSE_TEST_EQ(test, asc_rank_revealing_test::NonNormalizedFlags(), 0U);
  return test.Finish();
}
