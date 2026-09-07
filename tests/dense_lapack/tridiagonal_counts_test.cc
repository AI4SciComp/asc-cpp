#include <array>
#include <cstdio>

#include "../../src/dense/lapack/internal_tridiagonal_counts.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

int main() {
  asc_dense_test::TestContext test;
  namespace counts = asc::internal_tridiagonal_counts;
  for (const asc::extent_t limit :
       std::array<asc::extent_t, 2>{2147483647, 9223372036854775807}) {
    ASC_DENSE_TEST_CHECK(test, counts::Factor(0, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Factor(limit - 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Factor(limit, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Factor(-1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Solve(limit - 1, limit - 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(0, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(limit, 0, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Solve(1, limit, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Solve(limit, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Driver(limit, 0, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Driver(limit, limit - 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Driver(0, limit, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Driver(1, limit, limit).code(),
                      asc::ErrorCode::kOverflow);
    const asc::extent_t largest =
        limit == 2147483647 ? 715827882 : 3074457345618258602;
    ASC_DENSE_TEST_CHECK(test, counts::Estimator(largest, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Estimator(largest + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         counts::Expert(largest, limit - 1, true, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Expert(limit, 0, false, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Expert(limit, 0, true, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, counts::Expert(0, limit, true, limit).ok());
    ASC_DENSE_TEST_EQ(test, counts::Expert(-1, 0, true, limit).code(),
                      asc::ErrorCode::kOverflow);
  }
  ASC_DENSE_TEST_EQ(test, counts::Factor(1, 17).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, counts::Solve(1, 1, 17).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, counts::Driver(1, 1, 17).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, counts::Estimator(1, 17).code(),
                    asc::ErrorCode::kInvalidArgument);
  std::puts(
      "Both source INTEGER widths: GTTRF/GTTRS/GTSV/LACN2 exact arithmetic "
      "boundaries, no descriptors or fake backing");
  return test.Finish();
}
