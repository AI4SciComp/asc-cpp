#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rank_revealing.h"
#include "lapack_build_config.h"
#include "rank_revealing_faults.h"
#include "rank_revealing_test_support.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

namespace {
using asc_rank_revealing_test::ForeignExecutions;
using asc_rank_revealing_test::ForeignQueries;
using asc_rank_revealing_test::kInteger;
using asc_rank_revealing_test::kPacking;
using asc_rank_revealing_test::kPermutation;
using asc_rank_revealing_test::kReal;
using asc_rank_revealing_test::kScalar;
using asc_rank_revealing_test::Layout;
using asc_rank_revealing_test::Take;
using asc_rank_revealing_test::TestContext;
using asc_rank_revealing_test::WithoutAllocation;
constexpr asc::extent_t kColumns = 6;

template <typename T>
void SourceCall(T* a, lapack_int* pivots, T* tau, T* work, lapack_int lwork,
                lapack_int& info) {
  const lapack_int m = 0;
  const lapack_int n = kColumns;
  const lapack_int lda = 1;
  if constexpr (std::same_as<T, float>) {
    LAPACK_sgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, &info);
  } else if constexpr (std::same_as<T, double>) {
    LAPACK_dgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, &info);
  } else {
    asc::DenseBlasRealType<T> unused_real{};
    if constexpr (std::same_as<T, std::complex<float>>) {
      LAPACK_cgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, &unused_real,
                    &info);
    } else {
      LAPACK_zgeqp3(&m, &n, a, &lda, pivots, tau, work, &lwork, &unused_real,
                    &info);
    }
  }
}

template <typename T>
std::array<lapack_int, kColumns> DirectPermutation(
    TestContext& test, const std::array<asc::index_t, kColumns + 2>& flags) {
  std::array<T, kColumns> backing{};
  std::array<T, kColumns> work{};
  std::array<lapack_int, kColumns> pivots{};
  T tau{};
  for (std::size_t j = 0; j < pivots.size(); ++j) {
    pivots[j] = flags[j + 1] == 0 ? 0 : 1;
  }
  T query{};
  lapack_int info = -99;
  SourceCall(backing.data(), pivots.data(), &tau, &query, -1, info);
  ASC_DENSE_TEST_CHECK(test, info == 0 && query == T{1});
  SourceCall(backing.data(), pivots.data(), &tau, work.data(), kColumns, info);
  ASC_DENSE_TEST_CHECK(test, info == 0 && work[0] == T{1});
  return pivots;
}

template <typename T>
struct Storage {
  std::array<T, kColumns + 2> a;
  std::array<T, 3> tau;
  std::array<T, kColumns + 2> work;
  std::array<T, kColumns + 2> packing;
  std::array<lapack_int, kColumns + 2> integers;
  std::array<asc::index_t, kColumns + 2> conversion;
  std::array<asc::index_t, kColumns + 2> pivots;

  explicit Storage(int flags) {
    a.fill(T{-73});
    tau.fill(T{-79});
    work.fill(T{-83});
    packing.fill(T{-89});
    integers.fill(-97);
    conversion.fill(-101);
    pivots.fill(-103);
    for (std::size_t j = 1; j <= kColumns; ++j) {
      const bool fixed = flags == 2 || (flags == 1 && j % 2 == 0);
      pivots[j] = fixed ? std::numeric_limits<asc::index_t>::min() : 0;
      if (fixed && j % 3 == 0) {
        pivots[j] = std::numeric_limits<asc::index_t>::max();
      }
    }
  }

  asc::LapackWorkspace workspace() {
    asc::LapackWorkspace result;
    result.regions[kScalar] = {work.data() + 1, kColumns * sizeof(T),
                               asc::MemorySpace::kHost};
    result.regions[kPacking] = {packing.data() + 1, kColumns * sizeof(T),
                                asc::MemorySpace::kHost};
    result.regions[kInteger] = {integers.data() + 1,
                                kColumns * sizeof(lapack_int),
                                asc::MemorySpace::kHost};
    result.regions[kPermutation] = {conversion.data() + 1,
                                    kColumns * sizeof(asc::index_t),
                                    asc::MemorySpace::kHost};
    return result;
  }
};

template <typename T>
void CheckUnchangedPadding(TestContext& test, const Storage<T>& storage,
                           bool computed) {
  for (const auto value : storage.a) {
    ASC_DENSE_TEST_EQ(test, value, T{-73});
  }
  for (const auto value : storage.tau) {
    ASC_DENSE_TEST_EQ(test, value, T{-79});
  }
  for (const auto value : storage.packing) {
    ASC_DENSE_TEST_EQ(test, value, T{-89});
  }
  for (std::size_t j = 0; j < storage.work.size(); ++j) {
    ASC_DENSE_TEST_EQ(test, storage.work[j],
                      computed && j == 1 ? T{1} : T{-83});
  }
  ASC_DENSE_TEST_EQ(test, storage.integers.front(), -97);
  ASC_DENSE_TEST_EQ(test, storage.integers.back(), -97);
  ASC_DENSE_TEST_EQ(test, storage.conversion.front(), -101);
  ASC_DENSE_TEST_EQ(test, storage.conversion.back(), -101);
  ASC_DENSE_TEST_EQ(test, storage.pivots.front(), -103);
  ASC_DENSE_TEST_EQ(test, storage.pivots.back(), -103);
}

template <typename T>
void CheckFailures(TestContext& test, Storage<T>& storage,
                   const asc::LapackWorkspace& workspace, auto invoke) {
  const auto before = storage.pivots;
  const auto executions = ForeignExecutions();
  for (int mode = 0; mode != 3; ++mode) {
    auto candidate = workspace;
    if (mode == 0) {
      candidate.regions[kScalar] = {storage.work.data() + 1,
                                    (kColumns - 1) * sizeof(T),
                                    asc::MemorySpace::kHost};
    } else if (mode == 1) {
      candidate.regions[kPacking] = {storage.packing.data() + 1,
                                     kColumns * sizeof(T),
                                     asc::MemorySpace::kPinnedHost};
    } else {
      candidate.regions[kPacking] = workspace.regions[kInteger];
    }
    ASC_DENSE_TEST_CHECK(
        test, !WithoutAllocation(test, [&] { return invoke(candidate); }).ok());
    ASC_DENSE_TEST_EQ(test, ForeignExecutions(), executions);
    ASC_DENSE_TEST_CHECK(test, storage.pivots == before);
    CheckUnchangedPadding(test, storage, false);
    for (const auto value : storage.integers) {
      ASC_DENSE_TEST_EQ(test, value, -97);
    }
    for (const auto value : storage.conversion) {
      ASC_DENSE_TEST_EQ(test, value, -101);
    }
  }
}

template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Layout layout, int flags) {
  Storage<T> storage(flags);
  const auto input = storage.pivots;
  const auto av = Take(asc::DenseBlasMatrixView<T>::Create(
      storage.a.data() + 1, 0, kColumns, layout,
      std::numeric_limits<asc::extent_t>::max(),
      {storage.a.data(), sizeof(storage.a), asc::MemorySpace::kHost}));
  const auto pv = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      storage.pivots.data() + 1, kColumns, 1,
      {storage.pivots.data(), sizeof(storage.pivots),
       asc::MemorySpace::kHost}));
  const auto tv = Take(asc::DenseBlasVectorView<T>::Create(
      storage.tau.data() + 1, 0, 1,
      {storage.tau.data(), sizeof(storage.tau), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto queries = ForeignQueries();
  const auto executions = ForeignExecutions();
  const auto plan = WithoutAllocation(test, [&] {
    return asc::QueryGeqp3Workspace(provider, av, pv, tv, report);
  });
  ASC_DENSE_TEST_CHECK(test, plan.ok());
  if (!plan.ok()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, ForeignQueries(), queries + 1);
  ASC_DENSE_TEST_EQ(test, plan->regions[kScalar].minimum_entries, kColumns);
  ASC_DENSE_TEST_EQ(test, plan->regions[kScalar].preferred_entries, kColumns);
  ASC_DENSE_TEST_EQ(test, plan->regions[kPacking].minimum_entries, kColumns);
  ASC_DENSE_TEST_EQ(test, plan->regions[kInteger].minimum_entries, kColumns);
  ASC_DENSE_TEST_EQ(test, plan->regions[kReal].minimum_entries, 0);
  ASC_DENSE_TEST_CHECK(test, storage.pivots == input);
  const auto workspace = storage.workspace();
  CheckFailures(test, storage, workspace, [&](const auto& candidate) {
    return asc::Geqp3(provider, av, pv, tv, *plan, candidate, report);
  });
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Geqp3(provider, av, pv, tv, *plan,
                                                 workspace, report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, ForeignExecutions(), executions + 1);
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  CheckUnchangedPadding(test, storage, true);
  const auto direct = WithoutAllocation(
      test, [&] { return DirectPermutation<T>(test, input); });
  for (std::size_t j = 0; j < direct.size(); ++j) {
    ASC_DENSE_TEST_EQ(test, storage.pivots[j + 1], direct[j]);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    for (int flags = 0; flags != 3; ++flags) {
      Check<T>(test, provider, layout, flags);
    }
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
