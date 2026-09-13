#ifndef ASC_TESTS_DENSE_LAPACK_LU_HELPERS_FAILURE_TEST_H_
#define ASC_TESTS_DENSE_LAPACK_LU_HELPERS_FAILURE_TEST_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "asc/dense/providers/lapack_lu_helpers.h"
#include "lu_helpers_faults.h"
#include "lu_helpers_test_support.h"

namespace asc_helpers_test {
inline void FailedBeforeCall(TestContext& test,
                             const asc::LapackReport& report) {
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, asc_lapack_test::ReadHelperProbe().swap_calls, 0U);
  ASC_DENSE_TEST_EQ(test, asc_lapack_test::ReadHelperProbe().scale_calls, 0U);
}

template <typename T>
void DamageWorkspace(int defect, Sample<T>& sample,
                     asc::LapackWorkspacePlan& plan,
                     asc::LapackWorkspace& workspace) {
  auto& region = workspace.regions[kLayout];
  auto& required = plan.regions[kLayout];
  switch (defect) {
    case 0:
      region = {nullptr, 0, kHost};
      break;
    case 1:
      region = {sample.packed.data() + 1, sizeof(T), kHost};
      break;
    case 2:
      region = {reinterpret_cast<std::byte*>(sample.packed.data()) + 1,
                12 * sizeof(T), kHost};
      break;
    case 3:
      region = {sample.packed.data() + 1, 12 * sizeof(T),
                asc::MemorySpace::kDevice};
      break;
    case 4:
      region = {sample.a.data() + 1, 12 * sizeof(T), kHost};
      break;
    case 5:
      ++required.minimum_entries;
      break;
    case 6:
      ++required.preferred_entries;
      break;
    case 7:
      ++required.entry_bytes;
      break;
    case 8:
      required.alignment *= 2;
      break;
    case 9:
      --plan.total_byte_limit;
      break;
    case 12:
      region = {sample.packed.data() + 1, 12 * sizeof(T),
                asc::MemorySpace::kPinnedHost};
      break;
    default:
      break;
  }
}

template <typename T>
void WorkspaceFailures(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       bool scaling) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> sample(3, 4, kRow);
  sample.pivots[2] = 3;
  sample.pivots[4] = 1;
  sample.statistics = {static_cast<Real>(0.01L), static_cast<Real>(0.01L), 1};
  const auto swap = Take(asc::QueryLaswpWorkspace(provider, sample.Matrix(), 1,
                                                  2, sample.Pivots(), 2));
  const auto scale = Take(asc::QueryLaqgeWorkspace(
      provider, sample.Matrix(), sample.Rows(), sample.Columns(),
      sample.statistics, sample.applied));
  for (int defect = 0; defect < 13; ++defect) {
    sample.statistics = {static_cast<Real>(0.01L), static_cast<Real>(0.01L), 1};
    auto plan = scaling ? scale : swap;
    auto workspace = sample.Workspace();
    DamageWorkspace(defect, sample, plan, workspace);
    if (defect == 10) {
      plan.identity = scaling ? swap.identity : scale.identity;
    }
    if (defect == 11) {
      if (scaling) {
        sample.statistics.row_condition = static_cast<Real>(0.02L);
      } else {
        workspace.regions[kInteger] = {nullptr, 0, kHost};
      }
    }
    const auto before = sample;
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 23;
    asc_lapack_test::ResetHelperProbe();
    const auto status = WithoutAllocation(test, [&] {
      return scaling ? asc::Laqge(provider, sample.Matrix(), sample.Rows(),
                                  sample.Columns(), sample.statistics,
                                  sample.applied, plan, workspace, report)
                     : asc::Laswp(provider, sample.Matrix(), 1, 2,
                                  sample.Pivots(), 2, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    if (defect == 12) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
    }
    Unchanged(test, sample, before);
    FailedBeforeCall(test, report);
  }
}

template <typename T>
void IntegerWorkspaceFailures(TestContext& test,
                              const asc::ReferenceLapackProvider& provider) {
  for (auto layout : {kColumn, kRow}) {
    Sample<T> sample(3, 4, layout);
    sample.pivots[2] = 3;
    sample.pivots[4] = 1;
    const auto plan = Take(asc::QueryLaswpWorkspace(provider, sample.Matrix(),
                                                    1, 2, sample.Pivots(), 2));
    for (int defect = 0; defect < 5; ++defect) {
      auto workspace = sample.Workspace();
      auto& region = workspace.regions[kInteger];
      const auto bytes =
          static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
          plan.regions[kInteger].entry_bytes;
      if (defect == 0) {
        region = {sample.integer.data() + 8, bytes - 1, kHost};
      } else if (defect == 1) {
        region = {sample.integer.data() + 1, bytes, kHost};
      } else if (defect == 2) {
        region = {sample.pivots.data() + 1, bytes, kHost};
      } else if (defect == 3) {
        region = {sample.packed.data() + 1, bytes, kHost};
      } else {
        region = {sample.integer.data() + 8, bytes,
                  asc::MemorySpace::kPinnedHost};
      }
      const auto before = sample;
      asc::LapackReport report;
      asc_lapack_test::ResetHelperProbe();
      const auto status = WithoutAllocation(test, [&] {
        return asc::Laswp(provider, sample.Matrix(), 1, 2, sample.Pivots(), 2,
                          plan, workspace, report);
      });
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      Unchanged(test, sample, before);
      FailedBeforeCall(test, report);
    }
  }
}

template <typename T>
void EmptySwapWideStride(TestContext& test,
                         const asc::ReferenceLapackProvider& provider) {
  constexpr auto kWide =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const auto pivots = Take(asc::RawLapackPivotView::Create(
      nullptr, 0, asc::LapackFactorFamily::kLuPartialPivot,
      {nullptr, 0, kHost}));
  for (auto layout : {kColumn, kRow}) {
    for (auto shape : {std::array{0, 0}, std::array{0, 2}, std::array{3, 0}}) {
      const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
          nullptr, shape[0], shape[1], layout, kWide, {nullptr, 0, kHost}));
      const auto query = WithoutAllocation(test, [&] {
        return asc::QueryLaswpWorkspace(provider, matrix, 0, 0, pivots, 1);
      });
      if (layout == kColumn &&
          provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64) {
        ASC_DENSE_TEST_EQ(test, query.status().code(),
                          asc::ErrorCode::kOverflow);
        continue;
      }
      ASC_DENSE_TEST_CHECK(test, query.ok());
      if (!query.ok()) {
        continue;
      }
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return asc::Laswp(provider, matrix, 0, 0, pivots, 1, *query, {},
                          report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_CHECK(test,
                           !report.called_provider && !report.native_info);
      const auto stale = Take(asc::DenseBlasMatrixView<T>::Create(
          nullptr, shape[0], shape[1], layout, kWide + 1, {nullptr, 0, kHost}));
      ASC_DENSE_TEST_EQ(test,
                        WithoutAllocation(test,
                                          [&] {
                                            return asc::Laswp(
                                                provider, stale, 0, 0, pivots,
                                                1, *query, {}, report);
                                          })
                            .code(),
                        asc::ErrorCode::kInvalidState);
    }
  }
}

template <typename T>
void PivotFailures(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  for (int defect = 0; defect < 8; ++defect) {
    Sample<T> sample(3, 4, kRow);
    sample.pivots[2] = 3;
    sample.pivots[4] = 1;
    const auto plan = Take(asc::QueryLaswpWorkspace(provider, sample.Matrix(),
                                                    1, 2, sample.Pivots(), 2));
    auto pivots = sample.Pivots();
    auto matrix = sample.Matrix();
    asc::index_t first = 1;
    asc::extent_t count = 2;
    asc::index_t increment = 2;
    if (defect == 0) {
      pivots = sample.Pivots(3);
    }
    if (defect == 1) {
      pivots = sample.Pivots(64, asc::LapackFactorFamily::kRook);
    }
    if (defect == 2) {
      sample.pivots[2] = 0;
    }
    if (defect == 3) {
      sample.pivots[4] = 4;
    }
    if (defect == 4) {
      first = -1;
    }
    if (defect == 5) {
      count = 3;
    }
    if (defect == 6) {
      increment = std::numeric_limits<asc::index_t>::min();
    }
    if (defect == 7) {
      matrix = Take(asc::DenseBlasMatrixView<T>::Create(
          sample.a.data() + 1, 3, 4, kRow, sample.Ld(),
          {sample.a.data(), sizeof(sample.a), asc::MemorySpace::kDevice}));
    }
    const auto before = sample;
    asc::LapackReport report;
    asc_lapack_test::ResetHelperProbe();
    ASC_DENSE_TEST_CHECK(
        test, !WithoutAllocation(test, [&] {
                 return asc::Laswp(provider, matrix, first, count, pivots,
                                   increment, plan, sample.Workspace(), report);
               }).ok());
    Unchanged(test, sample, before);
    FailedBeforeCall(test, report);
  }
}

template <typename T>
void ScaleFailures(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (int defect = 0; defect < 12; ++defect) {
    Sample<T> sample(3, 4, kRow);
    sample.statistics = {static_cast<Real>(0.01L), static_cast<Real>(0.01L), 1};
    const auto plan = Take(asc::QueryLaqgeWorkspace(
        provider, sample.Matrix(), sample.Rows(), sample.Columns(),
        sample.statistics, sample.applied));
    auto rows = sample.Rows();
    auto columns = sample.Columns();
    if (defect == 0) {
      rows = Vector(sample.rows, 2);
    }
    if (defect == 1) {
      columns = Vector(sample.columns, 3);
    }
    if (defect == 2) {
      rows = Vector(sample.rows, 3, 2);
    }
    if (defect == 3) {
      rows = Take(asc::DenseBlasVectorView<const Real>::Create(
          sample.rows.data() + 1, 3, 1,
          {sample.rows.data(), sizeof(sample.rows),
           asc::MemorySpace::kDevice}));
    }
    if (defect == 4) {
      rows = Vector(sample.columns, 3);
    }
    if (defect == 5) {
      sample.statistics.row_condition = std::numeric_limits<Real>::quiet_NaN();
    }
    if (defect == 6) {
      sample.statistics.column_condition = -1;
    }
    if (defect == 7) {
      sample.statistics.row_condition = 2;
    }
    if (defect == 8) {
      sample.statistics.absolute_maximum = -1;
    }
    if (defect == 9) {
      sample.rows[1] = 0;
    }
    if (defect == 10) {
      sample.columns[2] = std::numeric_limits<Real>::infinity();
    }
    if (defect == 11) {
      sample.rows[3] = std::numeric_limits<Real>::quiet_NaN();
    }
    const auto before = sample;
    asc::LapackReport report;
    asc_lapack_test::ResetHelperProbe();
    ASC_DENSE_TEST_CHECK(test, !WithoutAllocation(test, [&] {
                                  return asc::Laqge(
                                      provider, sample.Matrix(), rows, columns,
                                      sample.statistics, sample.applied, plan,
                                      sample.Workspace(), report);
                                }).ok());
    Unchanged(test, sample, before);
    FailedBeforeCall(test, report);
  }
}

inline void EquedFailures(TestContext& test,
                          const asc::ReferenceLapackProvider& provider) {
  for (auto layout : {kColumn, kRow}) {
    for (char fault : {'?', 'N', 'R', 'C'}) {
      Sample<double> sample(3, 4, layout);
      sample.statistics = {0.01, 0.01, 1};
      sample.applied = asc::LapackEquilibration::kNone;
      const auto plan = Take(asc::QueryLaqgeWorkspace(
          provider, sample.Matrix(), sample.Rows(), sample.Columns(),
          sample.statistics, sample.applied));
      asc::LapackReport report;
      asc_lapack_test::ResetHelperProbe(fault);
      const auto status = WithoutAllocation(test, [&] {
        return asc::Laqge(provider, sample.Matrix(), sample.Rows(),
                          sample.Columns(), sample.statistics, sample.applied,
                          plan, sample.Workspace(), report);
      });
      ASC_DENSE_TEST_EQ(test, asc_lapack_test::ReadHelperProbe().scale_calls,
                        1U);
      ASC_DENSE_TEST_EQ(test, asc_lapack_test::ReadHelperProbe().equed_length,
                        1U);
      asc_lapack_test::ResetHelperProbe();
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_CHECK(test, report.called_provider && !report.native_info);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, sample.applied, asc::LapackEquilibration::kNone);
    }
  }
}

template <typename T>
void EmptyScaleCase(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    std::array<int, 2> shape, asc::DenseBlasLayout layout,
                    asc::extent_t stride) {
  using Real = asc::DenseBlasRealType<T>;
  const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, shape[0], shape[1], layout, stride, {nullptr, 0, kHost}));
  Sample<T> sample(shape[0], shape[1], layout);
  sample.statistics = {std::numeric_limits<Real>::quiet_NaN(), -1,
                       std::numeric_limits<Real>::infinity()};
  const auto before = sample;
  const auto query = WithoutAllocation(test, [&] {
    return asc::QueryLaqgeWorkspace(provider, matrix, sample.Rows(),
                                    sample.Columns(), sample.statistics,
                                    sample.applied);
  });
  const bool supported =
      layout == kRow ||
      provider.identity().integer_abi != asc::LapackIntegerAbi::kLp64 ||
      stride <= std::numeric_limits<std::int32_t>::max();
  ASC_DENSE_TEST_EQ(test, query.ok(), supported);
  Unchanged(test, sample, before);
  if (!query.ok()) {
    ASC_DENSE_TEST_EQ(test, query.status().code(), asc::ErrorCode::kOverflow);
    return;
  }
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Laqge(
                                   provider, matrix, sample.Rows(),
                                   sample.Columns(), sample.statistics,
                                   sample.applied, *query, {}, report);
                             }).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_EQ(test, sample.applied, asc::LapackEquilibration::kNone);
  sample.applied = before.applied;
  Unchanged(test, sample, before);
  const auto changed = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, shape[0], shape[1], layout, stride + 1, {nullptr, 0, kHost}));
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::Laqge(
                                            provider, changed, sample.Rows(),
                                            sample.Columns(), sample.statistics,
                                            sample.applied, *query, {}, report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidState);
  Unchanged(test, sample, before);
}

template <typename T>
void EmptyScale(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  const asc::extent_t wide =
      static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  for (auto layout : {kColumn, kRow}) {
    for (asc::extent_t stride : {asc::extent_t{4}, wide}) {
      for (auto shape :
           {std::array{0, 0}, std::array{0, 2}, std::array{3, 0}}) {
        EmptyScaleCase<T>(test, provider, shape, layout, stride);
      }
    }
  }
}
}  // namespace asc_helpers_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_HELPERS_FAILURE_TEST_H_
