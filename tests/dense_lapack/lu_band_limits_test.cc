#include <array>
#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_lu_band_limits.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace {
namespace limits = asc::internal_lu_band_limits;
using asc_dense_test::TestContext;
void Check(TestContext& test, asc::extent_t maximum) {
  ASC_DENSE_TEST_CHECK(
      test, limits::Storage(0, 0, 0, maximum - 1, maximum, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, limits::Storage(0, 0, 1, maximum - 1, maximum, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, limits::Storage(3, 4, 2, 1, 5, maximum).code(),
                    asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, limits::Storage(-1, 4, 2, 1, 6, maximum).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(
      test, limits::Factor(0, maximum, 0, maximum - 1, maximum, maximum).ok());
  ASC_DENSE_TEST_CHECK(
      test, limits::Factor(maximum, 0, 0, maximum - 1, maximum, maximum).ok());
  // Very wide/tall logical dimensions are not rejected when the source never
  // loops through them; only actual executed intermediate arithmetic is
  // bounded.
  ASC_DENSE_TEST_CHECK(test, limits::Factor(1, maximum, 0, 0, 1, maximum).ok());
  ASC_DENSE_TEST_CHECK(test, limits::Factor(maximum, 1, 0, 0, 1, maximum).ok());
  ASC_DENSE_TEST_CHECK(
      test, limits::Factor(1, 1, 0, maximum - 2, maximum - 1, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, limits::Factor(1, 1, 0, maximum - 1, maximum, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(
      test, limits::Factor(maximum - 1, maximum - 1, 0, 0, 1, maximum).ok());
  ASC_DENSE_TEST_EQ(test,
                    limits::Factor(maximum, maximum, 0, 0, 1, maximum).code(),
                    asc::ErrorCode::kOverflow);
  // DSWAP's two-element strided vector has a final 1+2*(LDAB-1) cursor.
  const auto ld = (maximum - 1) / 2 + 1;
  ASC_DENSE_TEST_CHECK(test, limits::Factor(2, 2, 1, 0, ld, maximum).ok());
  ASC_DENSE_TEST_EQ(test, limits::Factor(2, 2, 1, 0, ld + 1, maximum).code(),
                    asc::ErrorCode::kOverflow);
  // The blocked case adds KV to the largest possible pivot row before a
  // subsequent subtraction. Exercise the exact largest admitted row and +1.
  const auto count = maximum - 130;
  ASC_DENSE_TEST_CHECK(
      test, limits::Factor(count + 32, count, 32, 65, 130, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, limits::Factor(count + 33, count + 1, 32, 65, 130, maximum).code(),
      asc::ErrorCode::kOverflow);
  const auto long_ld = (maximum - 1) / 100 + 1;
  ASC_DENSE_TEST_CHECK(test,
                       limits::Factor(100, 100, 40, 67, long_ld, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, limits::Factor(100, 100, 40, 67, long_ld + 1, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(
      test,
      limits::Solve(0, 0, maximum - 1, maximum, maximum, 1, maximum).ok());
  ASC_DENSE_TEST_CHECK(
      test, limits::Solve(maximum, 0, 0, 1, 0, maximum, maximum).ok());
  ASC_DENSE_TEST_CHECK(
      test, limits::Solve(maximum - 1, 0, 0, 1, 1, maximum - 1, maximum).ok());
  ASC_DENSE_TEST_EQ(test,
                    limits::Solve(maximum, 0, 0, 1, 1, maximum, maximum).code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test,
                       limits::Solve(2, 0, 0, 1, maximum - 1, 2, maximum).ok());
  ASC_DENSE_TEST_EQ(test, limits::Solve(2, 0, 0, 1, maximum, 2, maximum).code(),
                    asc::ErrorCode::kOverflow);
  const auto nrhs = (maximum - 1) / 3;
  ASC_DENSE_TEST_CHECK(test, limits::Solve(2, 1, 0, 3, nrhs, 3, maximum).ok());
  ASC_DENSE_TEST_EQ(test,
                    limits::Solve(2, 1, 0, 3, nrhs + 1, 3, maximum).code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, limits::Solve(3, 1, 0, 3, 1, 2, maximum).code(),
                    asc::ErrorCode::kShape);
}
void CheckUnblocked(TestContext& test, asc::extent_t maximum) {
  const auto count = maximum - 130;
  // Explicit GBTF2 does not acquire the blocked cursor/pivot-row bounds.
  ASC_DENSE_TEST_CHECK(
      test,
      limits::Factor(count + 33, count + 1, 32, 65, 130, maximum, false).ok());
  ASC_DENSE_TEST_CHECK(test, limits::Factor(maximum - 98, maximum - 98, 32, 65,
                                            130, maximum, false)
                                 .ok());
  ASC_DENSE_TEST_EQ(
      test,
      limits::Factor(maximum - 97, maximum - 97, 32, 65, 130, maximum, false)
          .code(),
      asc::ErrorCode::kOverflow);
}
}  // namespace
int main() {
  TestContext test;
  for (const asc::extent_t maximum :
       std::array<asc::extent_t, 2>{std::numeric_limits<std::int32_t>::max(),
                                    std::numeric_limits<std::int64_t>::max()}) {
    Check(test, maximum);
    CheckUnblocked(test, maximum);
  }
  return test.Finish();
}
