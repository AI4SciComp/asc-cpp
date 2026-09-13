#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_band_estimation_limits.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;

void Condition(TestContext& test, asc::extent_t maximum) {
  using asc::internal_band_estimation_limits::CheckCondition;
  for (const auto triangle : {kUpper, kLower}) {
    ASC_DENSE_TEST_CHECK(
        test,
        CheckCondition(0, maximum - 1, maximum, triangle, true, maximum).ok());
    ASC_DENSE_TEST_CHECK(test, CheckCondition(maximum, maximum - 1, maximum,
                                              triangle, false, maximum)
                                   .ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckCondition(0, maximum, maximum, triangle, false, maximum).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, CheckCondition(maximum / 3, 0, 1, triangle, true, maximum).ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckCondition(maximum / 3 + 1, 0, 1, triangle, true, maximum).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test,
                      CheckCondition(3, 2, 2, triangle, true, maximum).code(),
                      asc::ErrorCode::kShape);
    ASC_DENSE_TEST_EQ(test,
                      CheckCondition(3, -1, 1, triangle, true, maximum).code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  // Upper LATBS must form KD+I before subtracting JLEN in its inline dot.
  ASC_DENSE_TEST_CHECK(
      test,
      CheckCondition(3, maximum - 2, maximum, kUpper, true, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test,
      CheckCondition(3, maximum - 1, maximum, kUpper, true, maximum).code(),
      asc::ErrorCode::kOverflow);
  // Lower LATBS can dispatch TBSV, whose J+KD expression reaches N+KD.
  ASC_DENSE_TEST_CHECK(
      test,
      CheckCondition(3, maximum - 3, maximum, kLower, true, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test,
      CheckCondition(3, maximum - 2, maximum, kLower, true, maximum).code(),
      asc::ErrorCode::kOverflow);
}
}  // namespace

int main() {
  TestContext test;
  Condition(test, std::numeric_limits<std::int32_t>::max());
  Condition(test, std::numeric_limits<std::int64_t>::max());
  return test.Finish();
}
