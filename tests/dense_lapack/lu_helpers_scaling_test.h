#ifndef ASC_TESTS_DENSE_LAPACK_LU_HELPERS_SCALING_TEST_H_
#define ASC_TESTS_DENSE_LAPACK_LU_HELPERS_SCALING_TEST_H_

#include <cmath>
#include <complex>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "asc/dense/providers/lapack_lu_helpers.h"
#include "lu_helpers_test_support.h"

namespace asc_helpers_test {
template <typename T>
void ComputeScales(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  const auto matrix = Take(asc::DenseBlasMatrixView<const T>::Create(
      sample.a.data() + 1, sample.m, sample.n, sample.layout, sample.Ld(),
      {sample.a.data(), sizeof(sample.a), kHost}));
  const auto rows = Take(asc::DenseBlasVectorView<Real>::Create(
      sample.rows.data() + 1, sample.m, 1,
      {sample.rows.data(), sizeof(sample.rows), kHost}));
  const auto columns = Take(asc::DenseBlasVectorView<Real>::Create(
      sample.columns.data() + 1, sample.n, 1,
      {sample.columns.data(), sizeof(sample.columns), kHost}));
  const auto before = sample;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryGeequWorkspace(provider, matrix, rows, columns,
                                    sample.statistics);
  }));
  Unchanged(test, sample, before);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Geequ(provider, matrix, rows, columns, sample.statistics, plan,
                      sample.Workspace(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info &&
                                 *report.native_info == 0);
  ASC_DENSE_TEST_CHECK(test, SameBits(sample.a, before.a));
  Padding(test, sample, before);
}

template <typename T>
void CheckTinyScaleWarning(TestContext& test, const Sample<T>& sample,
                           const asc::Status& status,
                           const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kAccuracyWarning);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  const auto value = sample.a[sample.Offset(0, 0)];
  ASC_DENSE_TEST_CHECK(test,
                       std::isinf(std::real(value)) && std::real(value) > 0);
  if constexpr (asc::DenseBlasComplex<T>) {
    ASC_DENSE_TEST_CHECK(test, std::isnan(value.imag()));
  }
  ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(0, 1)], T{0});
  ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(1, 0)], T{0});
  ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(1, 1)], T{1});
}

template <typename T>
void EquilibrationPipeline(TestContext& test,
                           const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (auto layout : {kColumn, kRow}) {
    for (bool tiny : {false, true}) {
      Sample<T> sample(2, 2, layout);
      sample.a[sample.Offset(0, 0)] =
          tiny ? T{std::numeric_limits<Real>::min() / 1024} : T{0x1p-20};
      sample.a[sample.Offset(0, 1)] = 0;
      sample.a[sample.Offset(1, 0)] = 0;
      sample.a[sample.Offset(1, 1)] = 1;
      ComputeScales(test, provider, sample);
      const auto before = sample;
      // Independent wide arithmetic proves that the intended scaling is I,
      // including the known upstream intermediate-overflow failure fixture.
      ASC_DENSE_TEST_EQ(test,
                        ToWide(before.a[before.Offset(0, 0)]) *
                            static_cast<long double>(before.rows[1]) *
                            static_cast<long double>(before.columns[1]),
                        std::complex<long double>(1, 0));
      const auto plan = Take(WithoutAllocation(test, [&] {
        return asc::QueryLaqgeWorkspace(provider, sample.Matrix(),
                                        sample.Rows(), sample.Columns(),
                                        sample.statistics, sample.applied);
      }));
      Unchanged(test, sample, before);
      for (int repeat = 0; repeat < 2; ++repeat) {
        sample.a = before.a;
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return asc::Laqge(provider, sample.Matrix(), sample.Rows(),
                            sample.Columns(), sample.statistics, sample.applied,
                            plan, sample.Workspace(), report);
        });
        ASC_DENSE_TEST_CHECK(test,
                             report.called_provider && !report.native_info);
        ASC_DENSE_TEST_EQ(test, sample.applied,
                          tiny ? asc::LapackEquilibration::kBoth
                               : asc::LapackEquilibration::kRows);
        if (tiny) {
          CheckTinyScaleWarning(test, sample, status, report);
        } else {
          ASC_DENSE_TEST_CHECK(test, status.ok());
          ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
          ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(0, 0)], T{1});
          ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(1, 1)], T{1});
        }
        ASC_DENSE_TEST_CHECK(test, SameBits(sample.rows, before.rows));
        ASC_DENSE_TEST_CHECK(test, SameBits(sample.columns, before.columns));
        Padding(test, sample, before);
      }
    }
  }
}

template <typename T>
void UnusedAndNonfiniteScales(TestContext& test,
                              const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (auto layout : {kColumn, kRow}) {
    for (bool scaling : {false, true}) {
      Sample<T> sample(2, 2, layout);
      sample.statistics = {scaling ? Real{0.01} : Real{1}, 1, 1};
      sample.a[sample.Offset(0, 0)] =
          Value<T>(std::numeric_limits<Real>::quiet_NaN());
      // Unused arrays can have genuinely empty borrowed storage. The serial
      // context rejects pinned host even on that empty descriptor.
      const auto pinned_rows =
          Take(asc::DenseBlasVectorView<const Real>::Create(
              scaling ? sample.rows.data() + 1 : nullptr, scaling ? 2 : 0, 1,
              {scaling ? sample.rows.data() : nullptr,
               scaling ? sizeof(sample.rows) : 0,
               asc::MemorySpace::kPinnedHost}));
      const auto columns = Take(asc::DenseBlasVectorView<const Real>::Create(
          nullptr, 0, 1, {nullptr, 0, kHost}));
      const auto before = sample;
      const auto rejected = WithoutAllocation(test, [&] {
        return asc::QueryLaqgeWorkspace(provider, sample.Matrix(), pinned_rows,
                                        columns, sample.statistics,
                                        sample.applied);
      });
      ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                        asc::ErrorCode::kMemoryAccess);
      Unchanged(test, sample, before);
      const auto rows = Take(asc::DenseBlasVectorView<const Real>::Create(
          scaling ? sample.rows.data() + 1 : nullptr, scaling ? 2 : 0, 1,
          {scaling ? sample.rows.data() : nullptr,
           scaling ? sizeof(sample.rows) : 0, kHost}));
      const auto plan = Take(WithoutAllocation(test, [&] {
        return asc::QueryLaqgeWorkspace(provider, sample.Matrix(), rows,
                                        columns, sample.statistics,
                                        sample.applied);
      }));
      Unchanged(test, sample, before);
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return asc::Laqge(provider, sample.Matrix(), rows, columns,
                          sample.statistics, sample.applied, plan,
                          sample.Workspace(), report);
      });
      ASC_DENSE_TEST_CHECK(test, report.called_provider && !report.native_info);
      ASC_DENSE_TEST_EQ(
          test, status.code(),
          scaling ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
      ASC_DENSE_TEST_EQ(test, report.outcome,
                        scaling ? asc::LapackOutcome::kAccuracyWarning
                                : asc::LapackOutcome::kSuccess);
      ASC_DENSE_TEST_CHECK(
          test, std::isnan(std::real(sample.a[sample.Offset(0, 0)])));
      if (!scaling) {
        ASC_DENSE_TEST_CHECK(test, SameBits(sample.a, before.a));
        ASC_DENSE_TEST_CHECK(test, SameBits(sample.packed, before.packed));
      }
      ASC_DENSE_TEST_CHECK(test, SameBits(sample.rows, before.rows));
      ASC_DENSE_TEST_CHECK(test, SameBits(sample.columns, before.columns));
      Padding(test, sample, before);
    }
  }
}
}  // namespace asc_helpers_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_HELPERS_SCALING_TEST_H_
