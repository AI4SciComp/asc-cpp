#include <array>
#include <cstdint>

#include "../../src/dense/lapack/internal_indefinite_packed_solve_counts.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  asc_dense_test::TestContext test;
  using asc::internal_indefinite_packed_solve_counts::Solve;
  for (const asc::extent_t limit :
       std::array<asc::extent_t, 2>{INT32_MAX, INT64_MAX}) {
    // Independently derived largest N for N*(N+1) <= INTEGER_MAX.
    const asc::extent_t bound = limit == INT32_MAX ? 46340 : 3037000499;
    ASC_DENSE_TEST_CHECK(test, Solve(bound, 1, bound, limit).ok());
    ASC_DENSE_TEST_EQ(test, Solve(bound + 1, 1, bound + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Solve(limit, 1, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, Solve(limit, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Solve(0, limit, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Solve(1, limit - 1, 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, Solve(1, limit, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, Solve(2, 3, (limit - 1) / 3, limit).ok());
    ASC_DENSE_TEST_EQ(test, Solve(2, 3, (limit - 1) / 3 + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Solve(-1, 1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, Solve(1, -1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, Solve(1, 1, 0, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  ASC_DENSE_TEST_EQ(test, Solve(0, 0, 1, 17).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, Solve(0, 0, 1, 0).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test,
                    Solve(INT64_MAX, INT64_MAX, INT64_MAX, INT32_MAX).code(),
                    asc::ErrorCode::kOverflow);
  return test.Finish();
}
