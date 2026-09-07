#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "src/dense/lapack/internal_qr_counts.h"

namespace {

using asc::internal_lapack_qr::AppendPacking;
using asc::internal_lapack_qr::GuardQueryCapacity;
using asc::internal_lapack_qr::MultiplyAdd;
using asc::internal_lapack_qr::Operation;
using asc::internal_lapack_qr::QueryCounts;
using asc_dense_test::TestContext;
constexpr asc::extent_t k32 = std::numeric_limits<std::int32_t>::max();
constexpr asc::extent_t k64 = std::numeric_limits<std::int64_t>::max();

void IntegerCounts(TestContext& test) {
  for (const auto limit : {k32, k64}) {
    auto result = QueryCounts(Operation::kGeqrf, 5, 3, 3, true, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    ASC_DENSE_TEST_EQ(test, result->minimum, 3);
    ASC_DENSE_TEST_EQ(test, result->raw_preferred, 96);
    result = QueryCounts(Operation::kGeqrf, 0, limit, 0, true, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    ASC_DENSE_TEST_EQ(test, result->minimum, 1);
    ASC_DENSE_TEST_EQ(test, result->raw_preferred, 1);
    result = QueryCounts(Operation::kGeqr2, 0, 7, 0, true, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    ASC_DENSE_TEST_EQ(test, result->minimum, 7);
    ASC_DENSE_TEST_EQ(test, result->raw_preferred, 7);
    result = QueryCounts(Operation::kGenerate, 5, 3, 2, true, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    ASC_DENSE_TEST_EQ(test, result->minimum, 3);
    ASC_DENSE_TEST_EQ(test, result->raw_preferred, 96);
    result = QueryCounts(Operation::kApply, 5, 3, 2, true, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    ASC_DENSE_TEST_EQ(test, result->minimum, 3);
    ASC_DENSE_TEST_EQ(test, result->raw_preferred, 4256);
    result = QueryCounts(Operation::kApply, 5, 3, 2, false, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    ASC_DENSE_TEST_EQ(test, result->minimum, 5);
    ASC_DENSE_TEST_EQ(test, result->raw_preferred, 4320);
    ASC_DENSE_TEST_EQ(
        test,
        QueryCounts(Operation::kGeqrf, 1, limit / 32 + 1, 1, true, limit)
            .status()
            .code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test,
        QueryCounts(Operation::kGenerate, limit, limit / 32 + 1, 0, true, limit)
            .status()
            .code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test,
                      QueryCounts(Operation::kApply, 1, (limit - 4160) / 32 + 1,
                                  1, true, limit)
                          .status()
                          .code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test,
        QueryCounts(Operation::kGenerate, 2, 3, 1, true, limit).status().code(),
        asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(
        test,
        QueryCounts(Operation::kApply, 2, 3, 3, true, limit).status().code(),
        asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(test, MultiplyAdd(limit, 1, 1, limit).status().code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, *MultiplyAdd(limit, 0, 0, limit), 0);
    ASC_DENSE_TEST_EQ(test, MultiplyAdd(-1, 0, 0, limit).status().code(),
                      asc::ErrorCode::kOverflow);
  }
}

void RowCursors(TestContext& test) {
  using asc::internal_lapack_qr::RowCursorBounds;
  for (const auto limit : {k32, k64}) {
    for (const bool complex : {false, true}) {
      for (const auto operation :
           {Operation::kGeqrf, Operation::kGeqr2, Operation::kGenerate}) {
        const asc::extent_t reflectors =
            operation == Operation::kGenerate ? 1 : 2;
        ASC_DENSE_TEST_CHECK(
            test, RowCursorBounds(operation, 2, 2, reflectors, true, complex,
                                  limit - 1, limit)
                      .ok());
        ASC_DENSE_TEST_EQ(test,
                          RowCursorBounds(operation, 2, 2, reflectors, true,
                                          complex, limit, limit)
                              .code(),
                          asc::ErrorCode::kOverflow);
      }
      ASC_DENSE_TEST_CHECK(
          test, RowCursorBounds(Operation::kApply, 2, 3, 1, true, complex,
                                (limit - 1) / 3, limit)
                    .ok());
      ASC_DENSE_TEST_EQ(test,
                        RowCursorBounds(Operation::kApply, 2, 3, 1, true,
                                        complex, (limit - 1) / 3 + 1, limit)
                            .code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_CHECK(test, RowCursorBounds(Operation::kApply, 2, 3, 1,
                                                 false, complex, limit, limit)
                                     .ok());
      ASC_DENSE_TEST_CHECK(test, RowCursorBounds(Operation::kApply, 1, 1, 0,
                                                 true, complex, limit, limit)
                                     .ok());
      ASC_DENSE_TEST_CHECK(test, RowCursorBounds(Operation::kGeqrf, 1, 1, 1,
                                                 true, complex, limit, limit)
                                     .ok());
    }
    // Real order-one LARFG is the source-proven TAU=0 exception. A complex
    // phase reflector can still scale the trailing row with increment LDA.
    ASC_DENSE_TEST_CHECK(test, RowCursorBounds(Operation::kGeqr2, 1, 2, 1, true,
                                               false, limit, limit)
                                   .ok());
    ASC_DENSE_TEST_EQ(
        test,
        RowCursorBounds(Operation::kGeqr2, 1, 2, 1, true, true, limit, limit)
            .code(),
        asc::ErrorCode::kOverflow);
    // The worst N-1 unblocked cursor also bounds blocked LARFB tails.
    ASC_DENSE_TEST_CHECK(
        test, RowCursorBounds(Operation::kGeqrf, 129, 129, 129, true, false,
                              (limit - 1) / 128, limit)
                  .ok());
    ASC_DENSE_TEST_EQ(test,
                      RowCursorBounds(Operation::kGeqrf, 129, 129, 129, true,
                                      false, (limit - 1) / 128 + 1, limit)
                          .code(),
                      asc::ErrorCode::kOverflow);
  }
}

void FloatQueryCounts(TestContext& test) {
  // Independent exact integers surrounding IEEE binary32 mantissa and signed
  // integer boundaries. No huge array or conversion result serves as oracle.
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(16777216, k32), 16777216);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(16777217, k32), 16777218);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(16777219, k32), 16777220);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(2147483519, k32),
                    2147483520);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(2147483520, k32),
                    2147483520);
  ASC_DENSE_TEST_EQ(test,
                    GuardQueryCapacity<float>(2147483521, k32).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test,
                    GuardQueryCapacity<float>(2147483584, k32).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, GuardQueryCapacity<float>(k32, k32).status().code(),
                    asc::ErrorCode::kOverflow);
  constexpr asc::extent_t kBelow64 = 9223371487098961920;
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(kBelow64 - 1, k64),
                    kBelow64);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<float>(kBelow64, k64), kBelow64);
  ASC_DENSE_TEST_EQ(
      test, GuardQueryCapacity<float>(kBelow64 + 1, k64).status().code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, GuardQueryCapacity<float>(k64, k64).status().code(),
                    asc::ErrorCode::kOverflow);
}

void DoubleQueryCounts(TestContext& test) {
  // D/Z QR does not invoke DROUNDUP_LWORK: a downward-rounded returned double
  // must not reduce the known checked source-integer capacity.
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<double>(9007199254740993, k64),
                    9007199254740993);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<double>(9007199254740995, k64),
                    9007199254740996);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<double>(288230376151711776, k64),
                    288230376151711776);
  constexpr asc::extent_t kBelow64 = 9223372036854774784;
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<double>(kBelow64, k64), kBelow64);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<double>(kBelow64 + 1, k64),
                    kBelow64 + 1);
  ASC_DENSE_TEST_EQ(test, GuardQueryCapacity<double>(k64, k64).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, *GuardQueryCapacity<double>(k32, k32), k32);
  ASC_DENSE_TEST_EQ(test, GuardQueryCapacity<double>(-1, k64).status().code(),
                    asc::ErrorCode::kOverflow);
}

void PackingCounts(TestContext& test) {
  ASC_DENSE_TEST_EQ(test, *AppendPacking(2, 3, 4, 16), 14);
  ASC_DENSE_TEST_EQ(test, *AppendPacking(0, k64, 0, 16), 0);
  ASC_DENSE_TEST_EQ(test, *AppendPacking(0, 0, k64, 16), 0);
  ASC_DENSE_TEST_EQ(test, AppendPacking(k64, 1, 1, 8).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, AppendPacking(0, k64, 2, 1).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, AppendPacking(0, k64, 1, 16).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, AppendPacking(0, 2, 3, 0).status().code(),
                    asc::ErrorCode::kInvalidArgument);
}

void IterationCounts(TestContext& test) {
  using asc::internal_lapack_qr::CheckIterationBounds;
  for (const auto limit : {k32, k64}) {
    ASC_DENSE_TEST_EQ(
        test,
        CheckIterationBounds(Operation::kGeqr2, limit, 1, 1, limit).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test,
        CheckIterationBounds(Operation::kApply, limit - 1, 1, limit - 31, limit)
            .code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test,
        CheckIterationBounds(Operation::kApply, limit - 1, 1, limit - 32, limit)
            .ok());
    ASC_DENSE_TEST_CHECK(
        test, CheckIterationBounds(Operation::kGeqr2, limit, 0, 0, limit).ok());
  }
}

}  // namespace

int main() {
  TestContext test;
  std::size_t allocations = 0;
  {
    const asc_dense_test::AllocationProbe probe;
    IntegerCounts(test);
    RowCursors(test);
    FloatQueryCounts(test);
    DoubleQueryCounts(test);
    PackingCounts(test);
    IterationCounts(test);
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  if (test.Finish() == 0) {
    std::cout << "native-free QR count checks: LP64/ILP64, S/C roundup, D/Z "
                 "rounding, source products, packing bytes; no allocation\n";
  }
  return test.Finish();
}
