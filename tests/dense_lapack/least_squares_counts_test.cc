#include <cstdint>
#include <limits>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "src/dense/lapack/internal_least_squares_counts.h"

namespace {
using asc::extent_t;
using asc::internal_lapack_least_squares::AppendPacking;
using asc::internal_lapack_least_squares::Operation;
using asc::internal_lapack_least_squares::QueryCounts;
using asc::internal_lapack_least_squares::ReturnedInteger;
using asc_dense_test::TestContext;

void Check(TestContext& test, extent_t limit) {
  const auto gels =
      QueryCounts<double>(Operation::kGels, 3, 2, 2, 3, false, limit);
  const auto gelst =
      QueryCounts<double>(Operation::kGelst, 3, 2, 2, 3, false, limit);
  const auto getsls =
      QueryCounts<double>(Operation::kGetsls, 3, 2, 2, 3, false, limit);
  ASC_DENSE_TEST_CHECK(test, gels.ok() && gelst.ok() && getsls.ok());
  if (gels.ok() && gelst.ok() && getsls.ok()) {
    ASC_DENSE_TEST_EQ(test, gels->minimum, 4);
    ASC_DENSE_TEST_EQ(test, gels->preferred, 66);
    ASC_DENSE_TEST_EQ(test, gelst->minimum, 4);
    ASC_DENSE_TEST_EQ(test, gelst->preferred, 4);
    ASC_DENSE_TEST_EQ(test, getsls->minimum, 9);
    ASC_DENSE_TEST_EQ(test, getsls->preferred, 9);
  }
  const auto tall =
      QueryCounts<double>(Operation::kGetsls, 17000, 8, 2, 17000, false, limit);
  const auto wide =
      QueryCounts<double>(Operation::kGetsls, 8, 17000, 2, 8, false, limit);
  ASC_DENSE_TEST_CHECK(test, tall.ok() && wide.ok());
  if (tall.ok() && wide.ok()) {
    ASC_DENSE_TEST_EQ(test, tall->minimum, 21);
    ASC_DENSE_TEST_EQ(test, tall->preferred, 53);
    ASC_DENSE_TEST_EQ(test, wide->minimum, 17013);
    ASC_DENSE_TEST_EQ(test, wide->preferred, 17013);
  }
  const auto real =
      QueryCounts<float>(Operation::kGels, 1, 1, 524288, 1, false, limit);
  const auto complex =
      QueryCounts<float>(Operation::kGels, 1, 1, 524288, 1, true, limit);
  ASC_DENSE_TEST_CHECK(test, real.ok() && complex.ok());
  if (real.ok() && complex.ok()) {
    ASC_DENSE_TEST_EQ(test, real->raw_preferred, 16777217);
    ASC_DENSE_TEST_EQ(test, real->preferred, 16777218);
    ASC_DENSE_TEST_EQ(test, complex->preferred, 16777217);
  }
  for (const auto operation :
       {Operation::kGels, Operation::kGelst, Operation::kGetsls}) {
    ASC_DENSE_TEST_CHECK(
        test,
        !QueryCounts<double>(operation, limit, 1, 1, limit, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, !QueryCounts<double>(operation, -1, 1, 1, 1, false, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, !QueryCounts<double>(operation, 1, 1, 1, 1, false, 123).ok());
  }
  ASC_DENSE_TEST_CHECK(
      test,
      !QueryCounts<double>(Operation::kGetsls, 65536, 65536, 1, 65536, false,
                           std::numeric_limits<std::int32_t>::max())
           .ok());
  // Legitimate zero-row/zero-column shapes require no fabricated backing.
  const auto empty =
      QueryCounts<double>(Operation::kGetsls, limit, 0, 0, limit, false, limit);
  ASC_DENSE_TEST_CHECK(test, empty.ok());
  if (empty.ok()) {
    ASC_DENSE_TEST_EQ(test, empty->preferred, 1);
  }
}

void RowCursors(TestContext& test, extent_t limit) {
  using asc::internal_lapack_least_squares::ReflectorCursorBounds;
  for (const auto operation :
       {Operation::kGels, Operation::kGelst, Operation::kGetsls}) {
    for (const bool complex : {false, true}) {
      ASC_DENSE_TEST_CHECK(test, QueryCounts<double>(operation, 2, 2, 1,
                                                     limit - 1, complex, limit)
                                     .ok());
      ASC_DENSE_TEST_CHECK(
          test,
          !QueryCounts<double>(operation, 2, 2, 1, limit, complex, limit).ok());
      ASC_DENSE_TEST_CHECK(
          test, ReflectorCursorBounds(operation, (limit - 1) / 2, 1, 2,
                                      (limit - 1) / 2, complex, limit)
                    .ok());
      ASC_DENSE_TEST_CHECK(
          test, !ReflectorCursorBounds(operation, (limit - 1) / 2 + 1, 1, 2,
                                       (limit - 1) / 2 + 1, complex, limit)
                     .ok());
      // Scalar and zero-RHS paths retain legitimate unused extreme strides.
      ASC_DENSE_TEST_CHECK(
          test,
          QueryCounts<double>(operation, 1, 1, 1, limit, complex, limit).ok());
      ASC_DENSE_TEST_CHECK(
          test, ReflectorCursorBounds(operation, 2, 2, 0, limit, complex, limit)
                    .ok());
    }
  }
}

void Precision(TestContext& test) {
  constexpr auto k32 = std::numeric_limits<std::int32_t>::max();
  constexpr auto k64 = std::numeric_limits<std::int64_t>::max();
  ASC_DENSE_TEST_EQ(test, *ReturnedInteger<float>(16777217, false, k32),
                    16777216);
  ASC_DENSE_TEST_EQ(test, *ReturnedInteger<float>(16777217, true, k32),
                    16777218);
  ASC_DENSE_TEST_EQ(test, *ReturnedInteger<float>(2147483520, true, k32),
                    2147483520);
  ASC_DENSE_TEST_CHECK(test,
                       !ReturnedInteger<float>(2147483521, true, k32).ok());
  ASC_DENSE_TEST_EQ(test,
                    *ReturnedInteger<double>(9007199254740993LL, false, k64),
                    9007199254740992LL);
  ASC_DENSE_TEST_EQ(test,
                    *ReturnedInteger<double>(288230376151711776LL, false, k64),
                    288230376151711744LL);
  ASC_DENSE_TEST_CHECK(test, !ReturnedInteger<double>(k64, false, k64).ok());
  ASC_DENSE_TEST_CHECK(
      test, !QueryCounts<float>(Operation::kGetsls, 16777216, 16777216, 1,
                                16777216, false, k64)
                 .ok());
  ASC_DENSE_TEST_CHECK(test, !QueryCounts<float>(Operation::kGetsls, 16777217,
                                                 1, 1, 16777217, false, k64)
                                  .ok());
  ASC_DENSE_TEST_CHECK(test, QueryCounts<float>(Operation::kGetsls, 16777216, 1,
                                                1, 16777216, false, k64)
                                 .ok());
  ASC_DENSE_TEST_EQ(test, *AppendPacking(3, 2, 5, 8), 13);
  ASC_DENSE_TEST_CHECK(test, !AppendPacking(k64, 1, 1, 1).ok());
  ASC_DENSE_TEST_CHECK(test, !AppendPacking(0, k64, 1, 8).ok());
  ASC_DENSE_TEST_CHECK(test, !AppendPacking(0, 1, 1, 0).ok());
}
}  // namespace

int main() {
  TestContext test;
  asc_dense_test::AllocationProbe probe;
  Check(test, std::numeric_limits<std::int32_t>::max());
  Check(test, std::numeric_limits<std::int64_t>::max());
  RowCursors(test, std::numeric_limits<std::int32_t>::max());
  RowCursors(test, std::numeric_limits<std::int64_t>::max());
  Precision(test);
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  return test.Finish();
}
