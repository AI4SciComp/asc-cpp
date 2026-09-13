#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_dmd_divide_query_counts.h"
#include "../../src/dense/lapack/internal_dmd_eigen_query_counts.h"
#include "../../src/dense/lapack/internal_dmd_execution_counts.h"
#include "../../src/dense/lapack/internal_dmd_query_counts.h"
#include "../../src/dense/lapack/internal_dmd_svd_query_counts.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using asc::extent_t;
using asc::internal_dmd_counts::Minimum;
using asc_dense_test::TestContext;

void ScalarDefaults(TestContext& test, extent_t limit) {
  // Independently evaluated scalar cases, also compared with the real provider
  // in dmd_counts_native_test. These are resource counts, not numerical
  // oracles.
  const std::array<extent_t, 4> real_scalar{6, 13, 9, 10};
  const std::array<extent_t, 4> real_integer{1, 8, 1, 4};
  const std::array<extent_t, 4> complex_scalar{3, 5, 4, 5};
  const std::array<extent_t, 4> complex_real{6, 13, 6, 8};
  for (int svd = 1; svd <= 4; ++svd) {
    const auto index = static_cast<std::size_t>(svd - 1);
    const auto real = Minimum<double>(1, 1, svd, false, false, limit);
    const auto complex = Minimum<float>(1, 1, svd, true, true, limit);
    ASC_DENSE_TEST_CHECK(test, real.ok() && complex.ok());
    if (!real.ok() || !complex.ok()) {
      continue;
    }
    ASC_DENSE_TEST_EQ(test, real->scalar, real_scalar[index]);
    ASC_DENSE_TEST_EQ(test, real->integer, real_integer[index]);
    ASC_DENSE_TEST_EQ(test, complex->scalar, complex_scalar[index]);
    ASC_DENSE_TEST_EQ(test, complex->real, complex_real[index]);
  }
}

void Boundaries(TestContext& test, extent_t limit) {
  for (int svd = 1; svd <= 4; ++svd) {
    const auto empty = Minimum<float>(limit, 0, svd, false, true, limit);
    ASC_DENSE_TEST_CHECK(test, empty.ok());
    if (empty.ok()) {
      ASC_DENSE_TEST_EQ(test, empty->scalar, 2);
      ASC_DENSE_TEST_EQ(test, empty->real, 1);
      ASC_DENSE_TEST_EQ(test, empty->integer, 1);
    }
    ASC_DENSE_TEST_CHECK(
        test, !Minimum<double>(-1, 0, svd, false, false, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         !Minimum<double>(1, 2, svd, false, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, !Minimum<double>(limit, limit, svd, false, false, limit).ok());
  }
  ASC_DENSE_TEST_CHECK(test,
                       !Minimum<double>(1, 1, 0, false, false, limit).ok());
  ASC_DENSE_TEST_CHECK(test,
                       !Minimum<double>(1, 1, 5, false, false, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !Minimum<double>(1, 1, 1, false, false, 17).ok());
}

void UnusedQueryBound(TestContext& test) {
  const extent_t limit = std::numeric_limits<std::int32_t>::max();
  // Even with n=1 and O,S, GESVD queries the unused m-by-m ORGQR workspace.
  const auto over = asc::internal_dmd_counts::SvdQuery<double>(
      limit / 32 + 1, 1, false, true, limit);
  const auto within = asc::internal_dmd_counts::SvdQuery<double>(
      limit / 32, 1, false, true, limit);
  ASC_DENSE_TEST_CHECK(test, !over.ok());
  ASC_DENSE_TEST_CHECK(test, within.ok());
  ASC_DENSE_TEST_CHECK(test, !asc::internal_dmd_counts::SvdQuery<double>(
                                  3, 2, false, false, limit)
                                  .ok());
}

void ShiftTransitions(TestContext& test) {
  namespace counts = asc::internal_dmd_counts;
  ASC_DENSE_TEST_EQ(test, counts::ShiftCount(180), 24);
  ASC_DENSE_TEST_EQ(test, counts::ShiftCount(182), 22);
  ASC_DENSE_TEST_EQ(test, counts::ShiftCount(590), 64);
  ASC_DENSE_TEST_EQ(test, counts::ShiftCount(3000), 128);
  ASC_DENSE_TEST_EQ(test, counts::ShiftCount(6000), 256);
  ASC_DENSE_TEST_EQ(test, counts::HessenbergQuery(16), 4259);
  ASC_DENSE_TEST_EQ(test, counts::HessenbergQuery(6000), 16865);
  const auto overflow = counts::EigenQuery<double>(
      std::numeric_limits<std::int64_t>::max(), false, true,
      std::numeric_limits<std::int64_t>::max());
  ASC_DENSE_TEST_CHECK(test, !overflow.ok());
}

void ExecutionBoundaries(TestContext& test) {
  namespace counts = asc::internal_dmd_counts;
  const extent_t narrow = std::numeric_limits<std::int32_t>::max();
  const extent_t wide = std::numeric_limits<std::int64_t>::max();
  // A near-square GESVD does not evaluate tall padded-workspace thresholds.
  ASC_DENSE_TEST_CHECK(test, counts::ExecutionBounds<double>(
                                 40000, 40000, 1, true, false, narrow)
                                 .ok());
  // The native tall GESVD threshold adds two full m-by-n matrices before
  // comparing LWORK, even when each individual matrix and query count fits.
  ASC_DENSE_TEST_CHECK(
      test, counts::Query<double>(30000000, 40, 1, false, false, narrow).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::ExecutionBounds<double>(
                                  30000000, 40, 1, false, false, narrow)
                                  .ok());
  ASC_DENSE_TEST_CHECK(
      test, counts::ExecutionBounds<double>(30000000, 40, 1, false, false, wide)
                .ok());
  // LAQPS stores linked column indices in real arithmetic. Both S/C
  // preconditioned routes reject an inexact binary32 column index.
  for (int svd : {3, 4}) {
    ASC_DENSE_TEST_CHECK(test, !counts::ExecutionBounds<float>(
                                    16777217, 16777217, svd, true, true, wide)
                                    .ok());
  }
  for (int svd = 1; svd <= 4; ++svd) {
    ASC_DENSE_TEST_CHECK(
        test,
        counts::ExecutionBounds<float>(16, 3, svd, true, true, narrow).ok());
  }
}

void RoundedMinimum(TestContext& test) {
  const extent_t limit = std::numeric_limits<std::int64_t>::max();
  // 4*(2^24+1) rounds down in binary32. The native encoded minimum cannot
  // replace sufficient scalar capacity. No large containing array is claimed.
  const auto counts = Minimum<float>(16777217, 16777217, 3, true, true, limit);
  ASC_DENSE_TEST_CHECK(test, counts.ok());
  if (!counts.ok()) {
    return;
  }
  ASC_DENSE_TEST_EQ(test, counts->scalar, 67108868);
  ASC_DENSE_TEST_EQ(test, counts->source_scalar, 67108864);
  ASC_DENSE_TEST_EQ(test, counts->query_scalar, 67108864);
  ASC_DENSE_TEST_EQ(test, counts->real, 100663305);
  ASC_DENSE_TEST_EQ(test, counts->source_real, 100663305);
  ASC_DENSE_TEST_EQ(test, counts->query_real, 100663304);
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  TestContext test;
  for (const extent_t limit :
       {extent_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<std::int64_t>::max()}) {
    ScalarDefaults(test, limit);
    Boundaries(test, limit);
  }
  RoundedMinimum(test);
  UnusedQueryBound(test);
  ShiftTransitions(test);
  ExecutionBoundaries(test);
  // The D/Z divide-and-conquer query uses DROUNDUP, unlike GESVD/GEEV.
  // Exercise its pure count model above binary64 exactness without arrays.
  constexpr extent_t kWideLimit = std::numeric_limits<std::int64_t>::max();
  const auto divided = asc::internal_dmd_counts::DivideQuery<double>(
      50000003, 50000003, false, kWideLimit);
  ASC_DENSE_TEST_CHECK(test, divided.ok());
  if (divided.ok()) {
    ASC_DENSE_TEST_EQ(test, divided->raw_preferred, 10000001550000057LL);
    ASC_DENSE_TEST_EQ(test, divided->returned_preferred, 10000001550000058LL);
  }
  for (int svd = 1; svd <= 4; ++svd) {
    ASC_DENSE_TEST_CHECK(
        test, !asc::internal_dmd_counts::Query<double>(
                   kWideLimit, kWideLimit, svd, true, true, kWideLimit)
                   .ok());
  }
  return test.Finish();
}
