#include "../../src/dense/lapack/internal_indefinite_counts.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"

int main() {
  namespace counts = asc::internal_indefinite_counts;
  asc_dense_test::TestContext test;
  constexpr asc::extent_t k32 = 2147483647;
  constexpr asc::extent_t k64 = 9223372036854775807;
  for (const auto limit : {k32, k64}) {
    ASC_DENSE_TEST_CHECK(test, counts::Factor(0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Factor(2, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Factor(3, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Factor(3, (limit - 1) / 2, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         !counts::Factor(3, (limit - 1) / 2 + 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Factor(limit, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Solve(limit, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Solve(limit, 1, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         counts::Solve(1, 2, (limit - 1) / 2, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         !counts::Solve(1, 2, (limit - 1) / 2 + 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Cursor(1, limit - 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Cursor(1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test,
                         !counts::Preferred<float>(limit / 64 + 1, limit).ok());
    ASC_DENSE_TEST_CHECK(
        test, !counts::Preferred<double>(limit / 64 + 1, limit).ok());
  }
  ASC_DENSE_TEST_CHECK(test, counts::Factor(46341, 46341, k32).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Factor(46342, 46342, k32).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Factor(-1, 1, k64).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Factor(1, 0, k64).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Factor(1, 1, 99).ok());
  // Exact constants independently expose both SROUNDUP overflow paths and
  // D/Z's required raw-integer lower bound, without fabricated descriptors.
  ASC_DENSE_TEST_EQ(test, *counts::Preferred<float>(33554430, k32), 2147483520);
  ASC_DENSE_TEST_CHECK(test, !counts::Preferred<float>(33554431, k32).ok());
  ASC_DENSE_TEST_EQ(test, *counts::Preferred<float>(144115179485921280, k64),
                    9223371487098961920LL);
  ASC_DENSE_TEST_CHECK(test,
                       !counts::Preferred<float>(144115179485921281, k64).ok());
  ASC_DENSE_TEST_EQ(test, *counts::Preferred<double>(9007199254740993, k64),
                    576460752303423552LL);
  ASC_DENSE_TEST_CHECK(test, !counts::Preferred<double>(k64 / 64, k64).ok());
  return test.Finish();
}
