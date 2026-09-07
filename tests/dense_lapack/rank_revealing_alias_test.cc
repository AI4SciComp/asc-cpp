#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rank_revealing.h"
#include "rank_revealing_faults.h"
#include "rank_revealing_test_support.h"

namespace {
using asc_rank_revealing_test::Execute;
using asc_rank_revealing_test::ForeignExecutions;
using asc_rank_revealing_test::kPacking;
using asc_rank_revealing_test::Layout;
using asc_rank_revealing_test::Matrix;
using asc_rank_revealing_test::Query;
using asc_rank_revealing_test::Routine;
using asc_rank_revealing_test::Scratch;
using asc_rank_revealing_test::Snapshot;
using asc_rank_revealing_test::Take;
using asc_rank_revealing_test::TestContext;
using asc_rank_revealing_test::Vector;
using asc_rank_revealing_test::WithoutAllocation;

template <typename T>
void MetadataAliases(TestContext& test, asc::ReferenceLapackProvider provider,
                     Routine routine) {
  Matrix<T> a(3, 2, Layout::kRowMajor);
  Matrix<T> b(3, 2, Layout::kColumnMajor);
  const auto before_a = Snapshot(a.bytes());
  const auto before_b = Snapshot(b.bytes());
  std::vector<asc::index_t> pivots{0, 0};
  std::vector<T> tau(2, T{-11});
  const auto av = a.view();
  const auto bv = b.view();
  const auto pv = Vector(pivots, 2);
  const auto tv = Vector(tau, 2);
  using Real = asc::DenseBlasRealType<T>;
  asc::LapackReport report;
  auto plan =
      Take(Query(provider, routine, av, bv, pv, tv, Real{0.125}, report));
  Scratch<T> scratch(plan, true);
  const auto original_workspace = scratch.view();
  const auto calls = ForeignExecutions();
  for (int kind = 0; kind < 4; ++kind) {
    auto workspace = original_workspace;
    std::array<asc::MutableMemoryView, 4> storage{
        asc::MutableMemoryView(&provider, sizeof(provider),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(&plan, sizeof(plan), asc::MemorySpace::kHost),
        asc::MutableMemoryView(&workspace, sizeof(workspace),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(&report, sizeof(report),
                               asc::MemorySpace::kHost)};
    workspace.regions[kPacking] = storage[static_cast<std::size_t>(kind)];
    const auto before_report = report;
    asc::index_t rank = -17;
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, av, bv, pv, tv, Real{0.125}, rank, plan,
                     workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, before_report.native_info);
    ASC_DENSE_TEST_EQ(test, report.called_provider,
                      before_report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.routine, before_report.routine);
    ASC_DENSE_TEST_EQ(test, ForeignExecutions(), calls);
    ASC_DENSE_TEST_EQ(test, rank, -17);
    a.CheckSame(test, before_a);
    b.CheckSame(test, before_b);
  }
  if (routine == Routine::kGelsy) {
    report.native_info = 53;
    // This is an actual live ASC64 object inside the report, not a fabricated
    // typed backing span. Pre-reset validation must protect its lifetime.
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return asc::Gelsy(provider, av, bv, pv,
                                                    Real{0.125},
                                                    *report.native_info, plan,
                                                    original_workspace, report);
                                }).ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, 53);
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return asc::Gelsy(provider, av, bv, pv,
                                                    Real{0.125}, pivots.front(),
                                                    plan, original_workspace,
                                                    report);
                                }).ok());
    ASC_DENSE_TEST_CHECK(test, pivots == std::vector<asc::index_t>({0, 0}));
  }
}

template <typename T>
void SingletonStride(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     Routine routine, bool row_major) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 1> a{T{2}};
  std::array<T, 1> b{T{6}};
  const auto limit =
      provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
          ? static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max())
          : std::numeric_limits<asc::extent_t>::max();
  const auto leading =
      row_major ? std::numeric_limits<asc::extent_t>::max() : limit;
  const auto layout = row_major ? Layout::kRowMajor : Layout::kColumnMajor;
  const auto av = Take(asc::DenseBlasMatrixView<T>::Create(
      a.data(), 1, 1, layout, leading,
      {a.data(), sizeof(a), asc::MemorySpace::kHost}));
  const auto bv = Take(asc::DenseBlasMatrixView<T>::Create(
      b.data(), 1, 1, Layout::kRowMajor,
      std::numeric_limits<asc::extent_t>::max(),
      {b.data(), sizeof(b), asc::MemorySpace::kHost}));
  std::vector<asc::index_t> pivots{std::numeric_limits<asc::index_t>::min()};
  std::vector<T> tau(1, T{-9});
  const auto pv = Vector(pivots, 1);
  const auto tv = Vector(tau, 1);
  asc::LapackReport report;
  const auto query = WithoutAllocation(test, [&] {
    return Query(provider, routine, av, bv, pv, tv, Real{0.25}, report);
  });
  ASC_DENSE_TEST_CHECK(test, query.ok());
  if (!query.ok()) {
    return;
  }
  Scratch<T> scratch(*query, false);
  const auto workspace = scratch.view();
  asc::index_t rank = -17;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Execute(provider, routine, av, bv, pv, tv,
                                              Real{0.25}, rank, *query,
                                              workspace, report);
                             }).ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, pivots.front(), 1);
  if (routine == Routine::kGelsy) {
    ASC_DENSE_TEST_EQ(test, rank, 1);
    ASC_DENSE_TEST_EQ(test, b.front(), T{3});
  } else {
    ASC_DENSE_TEST_EQ(test, tau.front(), T{});
    const auto const_a = Take(asc::DenseBlasMatrixView<const T>::Create(
        a.data(), 1, 1, layout, leading,
        {a.data(), sizeof(a), asc::MemorySpace::kHost}));
    const auto const_tau = Take(asc::DenseBlasVectorView<const T>::Create(
        tau.data(), 1, 1, {tau.data(), sizeof(T), asc::MemorySpace::kHost}));
    ASC_DENSE_TEST_CHECK(test, !asc::LapackHouseholderQrFactorView<T>::Create(
                                    const_a, const_tau, report)
                                    .ok());
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto routine : {Routine::kGeqp3, Routine::kGelsy}) {
    MetadataAliases<T>(test, provider, routine);
    SingletonStride<T>(test, provider, routine, false);
    SingletonStride<T>(test, provider, routine, true);
  }
}
}  // namespace

int main() {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  return test.Finish();
}
