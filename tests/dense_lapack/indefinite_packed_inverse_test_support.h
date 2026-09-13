#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_INVERSE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_INVERSE_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_inverse.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_test_support.h"
#include "indefinite_test_support.h"
#include "tests/dense/test_support.h"

namespace asc_packed_inverse_test {
namespace base = asc_indefinite_test;
namespace factor = asc_packed_indefinite_test;
using base::EqualBytes;
using base::kColumn;
using base::kHost;
using base::kLower;
using base::kRow;
using base::kUpper;
using base::Take;
using base::TestContext;
using base::ToWide;
using base::Value;
using base::Wide;
using factor::Sample;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasPackedMatrixView<T> a,
           asc::RawLapackPivotView pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHptriWorkspace(provider, triangle, a, pivots);
    }
  }
  return asc::QuerySptriWorkspace(provider, triangle, a, pivots);
}

template <typename T>
auto Inverse(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTriangle triangle, bool hermitian,
             asc::DenseBlasPackedMatrixView<T> a,
             asc::RawLapackPivotView pivots,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hptri(provider, triangle, a, pivots, plan, workspace, report);
    }
  }
  return asc::Sptri(provider, triangle, a, pivots, plan, workspace, report);
}

template <typename T>
auto Prepare(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Sample<T>& sample) {
  const auto pivots = base::Pivots(sample.pivots, sample.n);
  const auto plan = Take(factor::Query(
      provider, sample.triangle, sample.hermitian, sample.View(), pivots));
  base::Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status =
      factor::Factor(provider, sample.triangle, sample.hermitian, sample.View(),
                     pivots, plan, work, report);
  ASC_DENSE_TEST_CHECK(
      test, status.ok() || status.code() == asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.called_provider, sample.n != 0);
  ASC_DENSE_TEST_CHECK(
      test, report.output_validity == asc::LapackOutputValidity::kComplete ||
                report.output_validity ==
                    asc::LapackOutputValidity::kDocumentedPartial);
  sample.Reconstruction(test);
  sample.Guards(test);
  scratch.Guards(test, work);
  return report.native_info.value_or(0);
}

template <typename T>
void Mathematics(TestContext& test, const Sample<T>& sample) {
  std::array<Wide, 4489> inverse{};
  for (int j = 0; j < sample.n; ++j) {
    for (int i = 0; i < sample.n; ++i) {
      if (sample.Selected(i, j)) {
        const auto value = ToWide(sample.a[sample.Offset(i, j)]);
        inverse[i * sample.n + j] = value;
        inverse[j * sample.n + i] = factor::Adjoint(value, sample.hermitian);
      }
    }
  }
  long double norm_a = 0;
  long double norm_inverse = 0;
  long double residual = 0;
  for (int i = 0; i < sample.n; ++i) {
    long double row_a = 0;
    long double row_inverse = 0;
    long double row_residual = 0;
    for (int j = 0; j < sample.n; ++j) {
      row_a += std::abs(sample.full[i * sample.n + j]);
      row_inverse += std::abs(inverse[i * sample.n + j]);
      Wide left{};
      Wide right{};
      for (int k = 0; k < sample.n; ++k) {
        left += sample.full[i * sample.n + k] * inverse[k * sample.n + j];
        right += inverse[i * sample.n + k] * sample.full[k * sample.n + j];
      }
      const Wide identity{i == j ? 1.0L : 0.0L};
      const auto error =
          std::max(std::abs(left - identity), std::abs(right - identity));
      ASC_DENSE_TEST_CHECK(test, std::isfinite(error));
      row_residual += error;
    }
    norm_a = std::max(norm_a, row_a);
    norm_inverse = std::max(norm_inverse, row_inverse);
    residual = std::max(residual, row_residual);
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(norm_inverse));
  const auto bound = 256 * std::max(sample.n, 1) *
                     std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                     std::max(1.0L, norm_a * norm_inverse);
  ASC_DENSE_TEST_CHECK(test, residual <= bound);
}
}  // namespace asc_packed_inverse_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_INVERSE_TEST_SUPPORT_H_
