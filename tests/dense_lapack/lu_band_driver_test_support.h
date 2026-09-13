#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_DRIVER_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_DRIVER_TEST_SUPPORT_H_
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "asc/dense/providers/lapack_lu_band_expert.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace asc_lu_band_driver_test {
namespace support = asc_lu_band_expert_test;
namespace base = asc_lu_band_test;
using support::Take;
using support::TestContext;
inline bool Rows(asc::LapackEquilibration eq) {
  return eq == asc::LapackEquilibration::kRows ||
         eq == asc::LapackEquilibration::kBoth;
}
inline bool Columns(asc::LapackEquilibration eq) {
  return eq == asc::LapackEquilibration::kColumns ||
         eq == asc::LapackEquilibration::kBoth;
}
template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  base::Band<T> original;
  support::Compact<T> a;
  base::Band<T> af;
  base::Pivots pivots;
  base::Rhs<T> b;
  base::Rhs<T> x;
  support::Vector<Real> rows;
  support::Vector<Real> columns;
  support::Vector<Real> ferr;
  support::Vector<Real> berr;
  asc::LapackEquilibration equed = asc::LapackEquilibration::kNone;
  asc::LapackSolveStatistics<Real> stats{Real{-271}, Real{-277}};
  Sample(base::Band<T> source, asc::extent_t nrhs,
         asc::DenseBlasTranspose trans, asc::DenseBlasLayout b_layout,
         asc::DenseBlasLayout x_layout)
      : original(std::move(source)),
        a(original),
        af(original),
        pivots(original.n),
        b(original, nrhs, b_layout, trans),
        x(original, nrhs, x_layout, trans),
        rows(original.n),
        columns(original.n),
        ferr(nrhs),
        berr(nrhs) {
    const auto nan = std::numeric_limits<Real>::quiet_NaN();
    for (asc::extent_t i = 0; i < original.n; ++i) {
      for (asc::extent_t j = 0; j < nrhs; ++j) {
        x.values[x.Index(i, j)] = support::Value<T>(nan, nan);
      }
    }
  }
  [[nodiscard]] auto ConstB() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        b.values.data() + 1, b.n, b.count, b.layout, b.ld,
        {b.values.data(), b.values.size() * sizeof(T), base::kHost}));
  }
  void Supply(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::LapackEquilibration selected) {
    equed = selected;
    for (asc::extent_t i = 0; i < original.n; ++i) {
      rows.values[static_cast<std::size_t>(i + 1)] =
          Rows(equed) ? std::ldexp(Real{1}, static_cast<int>(i % 3) - 1)
                      : std::numeric_limits<Real>::quiet_NaN();
      columns.values[static_cast<std::size_t>(i + 1)] =
          Columns(equed) ? std::ldexp(Real{1}, 1 - static_cast<int>(i % 3))
                         : std::numeric_limits<Real>::quiet_NaN();
    }
    for (asc::extent_t j = 0; j < original.n; ++j) {
      for (asc::extent_t i = std::max<asc::extent_t>(0, j - original.ku);
           i < std::min(original.n, j + original.kl + 1); ++i) {
        T value = original.values[original.Index(i, j)];
        if (Rows(equed)) {
          value *= rows.values[static_cast<std::size_t>(i + 1)];
        }
        if (Columns(equed)) {
          value *= columns.values[static_cast<std::size_t>(j + 1)];
        }
        a.At(i, j) = value;
        af.Put(i, j, value);
      }
    }
    const auto matrix = af.View();
    const auto swaps = pivots.View();
    const auto plan = Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
    support::Scratch<T> scratch(plan);
    const auto workspace = scratch.View();
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test,
        asc::Gbtrf(provider, matrix, swaps, plan, workspace, report).ok());
    af.Reconstruct(test, pivots);
    scratch.Check(test);
    pivots.Check(test);
  }
  auto Query(const asc::ReferenceLapackProvider& provider, int mode) {
    if (mode == 0) {
      return asc::QueryGbsvxWorkspace(
          provider, b.operation, a.ConstView(), af.View(), pivots.View(),
          ConstB(), x.View(), ferr.View(), berr.View(), stats);
    }
    if (mode == 1) {
      return asc::QueryGbsvxEquilibratedWorkspace(
          provider, b.operation, a.View(), af.View(), pivots.View(), equed,
          rows.View(), columns.View(), b.View(), x.View(), ferr.View(),
          berr.View(), stats);
    }
    return asc::QueryGbsvxFactoredWorkspace(
        provider, b.operation, a.ConstView(), af.ConstView(),
        Take(asc::ReferenceLuBandPivotView::Create(pivots.ConstView())), equed,
        rows.ConstView(), columns.ConstView(), b.View(), x.View(), ferr.View(),
        berr.View(), stats);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider, int mode,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    if (mode == 0) {
      return asc::Gbsvx(provider, b.operation, a.ConstView(), af.View(),
                        pivots.View(), ConstB(), x.View(), ferr.View(),
                        berr.View(), stats, plan, workspace, report);
    }
    if (mode == 1) {
      return asc::GbsvxEquilibrated(
          provider, b.operation, a.View(), af.View(), pivots.View(), equed,
          rows.View(), columns.View(), b.View(), x.View(), ferr.View(),
          berr.View(), stats, plan, workspace, report);
    }
    return asc::GbsvxFactored(
        provider, b.operation, a.ConstView(), af.ConstView(),
        Take(asc::ReferenceLuBandPivotView::Create(pivots.ConstView())), equed,
        rows.ConstView(), columns.ConstView(), b.View(), x.View(), ferr.View(),
        berr.View(), stats, plan, workspace, report);
  }
};
template <typename T>
void Near(TestContext& test, T actual, support::Wide expected) {
  const auto error = std::abs(support::ToWide(actual) - expected);
  const auto scale =
      std::max(std::abs(expected), std::abs(support::ToWide(actual)));
  ASC_DENSE_TEST_CHECK(
      test,
      std::isfinite(error) &&
          error <=
              64 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                  scale);
}
template <typename T>
void Scaling(TestContext& test, const Sample<T>& sample,
             const Sample<T>& before, int mode) {
  if (mode != 1) {
    ASC_DENSE_TEST_CHECK(test,
                         support::SameBytes(sample.a.values, before.a.values));
  }
  for (asc::extent_t j = 0; j < sample.original.n; ++j) {
    for (asc::extent_t i = std::max<asc::extent_t>(0, j - sample.original.ku);
         i < std::min(sample.original.n, j + sample.original.kl + 1); ++i) {
      auto expected =
          support::ToWide(sample.original.values[sample.original.Index(i, j)]);
      if (mode != 0 && Rows(sample.equed)) {
        expected *= sample.rows.values[static_cast<std::size_t>(i + 1)];
      }
      if (mode != 0 && Columns(sample.equed)) {
        expected *= sample.columns.values[static_cast<std::size_t>(j + 1)];
      }
      Near(test, sample.a.At(i, j), expected);
    }
  }
  for (asc::extent_t i = 0; i < sample.original.n; ++i) {
    for (asc::extent_t j = 0; j < sample.b.count; ++j) {
      auto expected = support::ToWide(before.b.values[before.b.Index(i, j)]);
      if (mode != 0 && sample.b.operation == asc::DenseBlasTranspose::kNone &&
          Rows(sample.equed)) {
        expected *= sample.rows.values[static_cast<std::size_t>(i + 1)];
      }
      if (mode != 0 && sample.b.operation != asc::DenseBlasTranspose::kNone &&
          Columns(sample.equed)) {
        expected *= sample.columns.values[static_cast<std::size_t>(i + 1)];
      }
      Near(test, sample.b.values[sample.b.Index(i, j)], expected);
    }
  }
  if (mode == 0) {
    ASC_DENSE_TEST_CHECK(test,
                         support::SameBytes(sample.b.values, before.b.values));
  }
  if (mode == 2) {
    ASC_DENSE_TEST_CHECK(
        test, support::SameBytes(sample.af.values, before.af.values));
    ASC_DENSE_TEST_EQ(test, sample.pivots.values, before.pivots.values);
    ASC_DENSE_TEST_CHECK(
        test, support::SameBytes(sample.rows.values, before.rows.values));
    ASC_DENSE_TEST_CHECK(
        test, support::SameBytes(sample.columns.values, before.columns.values));
  }
}
}  // namespace asc_lu_band_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_DRIVER_TEST_SUPPORT_H_
