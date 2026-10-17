#include <cstddef>
#include <cstdint>
#include <limits>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_sylvester_counts.h"

namespace {
using asc::extent_t;
using asc::internal_lapack_sylvester::CheckDimensions;
using asc::internal_lapack_sylvester::PackingEntries;
using asc_dense_test::TestContext;

void Boundaries(TestContext& test, extent_t limit,
                extent_t last_square_cursor) {
  ASC_DENSE_TEST_CHECK(test,
                       CheckDimensions(0, limit, 1, false, true, limit).ok());
  ASC_DENSE_TEST_CHECK(test,
                       CheckDimensions(limit, 0, 1, false, true, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test, !CheckDimensions(limit, 1, limit, true, false, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test, CheckDimensions(limit - 1, 1, limit - 1, true, false, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test, CheckDimensions(last_square_cursor, 1, last_square_cursor, false,
                            false, limit)
                .ok());
  ASC_DENSE_TEST_CHECK(
      test, !CheckDimensions(last_square_cursor + 1, 1, last_square_cursor + 1,
                             false, false, limit)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, CheckDimensions(1, last_square_cursor, 1, true, true, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !CheckDimensions(1, last_square_cursor + 1, 1, true, true, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test, CheckDimensions(1, 2, limit - 1, true, false, limit).ok());
  ASC_DENSE_TEST_CHECK(test,
                       !CheckDimensions(1, 2, limit, true, false, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test, CheckDimensions(limit / 2, 2, limit / 2, true, false, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !CheckDimensions(limit / 2 + 1, 2, limit / 2 + 1,
                                              true, false, limit)
                                  .ok());
  ASC_DENSE_TEST_EQ(test, CheckDimensions(-1, 1, 1, false, false, limit).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, CheckDimensions(1, -1, 1, false, false, limit).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, CheckDimensions(2, 1, 1, false, false, limit).code(),
                    asc::ErrorCode::kInvalidArgument);
}

void Packing(TestContext& test) {
  for (const std::size_t bytes : {4U, 8U, 16U}) {
    const auto column = PackingEntries(3, 2, false, bytes);
    const auto row = PackingEntries(3, 2, true, bytes);
    ASC_DENSE_TEST_CHECK(test, column.ok() && *column == 13);
    ASC_DENSE_TEST_CHECK(test, row.ok() && *row == 19);
    const auto empty =
        PackingEntries(0, std::numeric_limits<extent_t>::max(), true, bytes);
    ASC_DENSE_TEST_CHECK(test, empty.ok() && *empty == 0);
  }
  ASC_DENSE_TEST_CHECK(test, !PackingEntries(1, 1, true, 0).ok());
  ASC_DENSE_TEST_CHECK(test, !PackingEntries(-1, 1, false, 8).ok());
  ASC_DENSE_TEST_CHECK(test, !PackingEntries(1, -1, false, 8).ok());
  // Each square, their sum, and the byte product are separate ASC limits.
  ASC_DENSE_TEST_CHECK(test, !PackingEntries(3037000500, 1, false, 1).ok());
  ASC_DENSE_TEST_CHECK(test,
                       !PackingEntries(2147483648, 2147483648, false, 1).ok());
  ASC_DENSE_TEST_CHECK(test,
                       !PackingEntries(2147483647, 2147483647, true, 1).ok());
  ASC_DENSE_TEST_CHECK(test, !PackingEntries(2147483647, 1, false, 8).ok());
}
}  // namespace

int main() {
  TestContext test;
  asc_dense_test::AllocationProbe allocation;
  Boundaries(test, std::numeric_limits<std::int32_t>::max(), 46341);
  Boundaries(test, std::numeric_limits<std::int64_t>::max(), 3037000500);
  Packing(test);
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(allocation.count(), 0));
  return test.Finish();
}
