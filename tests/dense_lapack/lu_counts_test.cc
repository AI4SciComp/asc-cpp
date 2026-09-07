#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "src/dense/lapack/internal_lu_counts.h"

namespace {
using asc::internal_lapack_lu::CheckFactor;
using asc::internal_lapack_lu::CheckSolve;
using asc::internal_lapack_lu::FactorRoute;
using asc::internal_lapack_lu::InversePreferred;
using asc_dense_test::TestContext;
constexpr asc::extent_t k32 = std::numeric_limits<std::int32_t>::max();
constexpr asc::extent_t k64 = std::numeric_limits<std::int64_t>::max();

void FactorCounts(TestContext& test) {
  for (const auto limit : {k32, k64}) {
    for (const auto route : {FactorRoute::kBlocked, FactorRoute::kRecursive,
                             FactorRoute::kUnblocked}) {
      ASC_DENSE_TEST_CHECK(test, CheckFactor(route, 0, limit, 1, limit).ok());
      ASC_DENSE_TEST_CHECK(test,
                           CheckFactor(route, limit, 0, limit, limit).ok());
      ASC_DENSE_TEST_CHECK(test,
                           CheckFactor(route, 1, limit, limit, limit).ok());
      ASC_DENSE_TEST_EQ(test, CheckFactor(route, limit, 1, limit, limit).code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_CHECK(test, CheckFactor(route, 67, 67, 70, limit).ok());
    }
    ASC_DENSE_TEST_CHECK(
        test,
        CheckFactor(FactorRoute::kRecursive, 2, limit, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, CheckFactor(FactorRoute::kBlocked, 2, limit, limit, limit).ok());
    ASC_DENSE_TEST_EQ(
        test, CheckFactor(FactorRoute::kUnblocked, 2, 1, limit, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test,
        CheckFactor(FactorRoute::kUnblocked, 2, 1, limit - 1, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, CheckFactor(FactorRoute::kUnblocked, 2, (limit - 1) / 2, 2, limit)
                  .ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckFactor(FactorRoute::kUnblocked, 2, (limit - 1) / 2 + 1, 2, limit)
            .code(),
        asc::ErrorCode::kOverflow);
    // Independent exact block endpoint: MAX is 63 modulo 64. The last
    // accepted pivot count has a final J=MAX-62; its successor wraps J.
    ASC_DENSE_TEST_CHECK(test, CheckFactor(FactorRoute::kBlocked, limit - 63,
                                           limit - 63, limit, limit)
                                   .ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckFactor(FactorRoute::kBlocked, limit - 62, limit - 62, limit, limit)
            .code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, CheckFactor(FactorRoute::kRecursive, limit - 1,
                                           limit - 1, limit, limit)
                                   .ok());
    ASC_DENSE_TEST_EQ(
        test, CheckFactor(FactorRoute::kRecursive, -1, 1, 1, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test,
                      CheckFactor(FactorRoute::kBlocked, 2, 2, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
  }
}

void SolveCounts(TestContext& test) {
  for (const auto limit : {k32, k64}) {
    ASC_DENSE_TEST_CHECK(test, CheckSolve(0, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, CheckSolve(limit, 0, limit).ok());
    ASC_DENSE_TEST_CHECK(test, CheckSolve(limit - 1, limit - 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, CheckSolve(1, limit, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, CheckSolve(limit, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, CheckSolve(-1, 0, limit).code(),
                      asc::ErrorCode::kOverflow);
  }
}

void InverseCounts(TestContext& test) {
  for (const auto limit : {k32, k64}) {
    ASC_DENSE_TEST_EQ(test, *InversePreferred<float>(0, limit), 1);
    ASC_DENSE_TEST_EQ(test, *InversePreferred<double>(67, limit), 4288);
    ASC_DENSE_TEST_EQ(
        test, InversePreferred<double>(limit / 64 + 1, limit).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, InversePreferred<float>(-1, limit).status().code(),
                      asc::ErrorCode::kOverflow);
  }
  // Exact raw products straddle binary32 rounding and both unsafe phases
  // of SROUNDUP_LWORK. No out-of-range floating-to-integer cast is executed.
  ASC_DENSE_TEST_EQ(test, *InversePreferred<float>(33554430, k32), 2147483520);
  ASC_DENSE_TEST_EQ(test,
                    InversePreferred<float>(33554431, k32).status().code(),
                    asc::ErrorCode::kOverflow);
  constexpr asc::extent_t kFloat64Order = 144115179485921280;
  ASC_DENSE_TEST_EQ(test, *InversePreferred<float>(kFloat64Order, k64),
                    9223371487098961920);
  ASC_DENSE_TEST_EQ(
      test, InversePreferred<float>(kFloat64Order + 1, k64).status().code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test,
                    InversePreferred<float>(k64 / 64, k64).status().code(),
                    asc::ErrorCode::kOverflow);
  // 64*(2^53+1) rounds down in binary64. Capacity must retain the exact
  // source integer, not just ceil of the rounded provider query value.
  ASC_DENSE_TEST_EQ(test, *InversePreferred<double>(9007199254740993, k64),
                    576460752303423552);
  ASC_DENSE_TEST_EQ(test, *InversePreferred<double>(9007199254740995, k64),
                    576460752303423744);
  ASC_DENSE_TEST_EQ(test,
                    InversePreferred<double>(k64 / 64, k64).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, *InversePreferred<double>(k32 / 64, k32), 2147483584);
  ASC_DENSE_TEST_EQ(test, InversePreferred<double>(1, 7).status().code(),
                    asc::ErrorCode::kInvalidArgument);
}
}  // namespace

int main() {
  TestContext test;
  std::size_t allocations = 0;
  {
    const asc_dense_test::AllocationProbe probe;
    FactorCounts(test);
    SolveCounts(test);
    InverseCounts(test);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  if (test.Finish() == 0) {
    std::cout << "LU arithmetic: exact LP64/ILP64 loop endpoints, strided "
                 "cursors, S/C roundup, D/Z capacity; no foreign arrays\n";
  }
  return test.Finish();
}
