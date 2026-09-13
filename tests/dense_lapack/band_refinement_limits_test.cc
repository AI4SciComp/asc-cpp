#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_band_refinement_limits.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace {
using asc_dense_test::TestContext;
asc::Status Check(
    asc::extent_t n, asc::extent_t kd, asc::extent_t nrhs,
    asc::extent_t maximum,
    asc::DenseBlasTriangle triangle = asc::DenseBlasTriangle::kUpper) {
  return asc::internal_band_refinement_limits::Check(
      n, kd, kd + 1, kd + 1, nrhs, n == 0 ? 1 : n, n == 0 ? 1 : n, triangle,
      maximum);
}
void Run(TestContext& test, asc::extent_t maximum) {
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    ASC_DENSE_TEST_CHECK(
        test, Check(0, maximum - 1, maximum - 1, maximum, triangle).ok());
    ASC_DENSE_TEST_EQ(test, Check(0, 0, maximum, maximum, triangle).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, Check(maximum, maximum - 1, 0, maximum, triangle).ok());
    ASC_DENSE_TEST_CHECK(test,
                         Check(maximum / 3, 0, 1, maximum, triangle).ok());
    ASC_DENSE_TEST_EQ(test,
                      Check(maximum / 3 + 1, 0, 1, maximum, triangle).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(
        test, Check(1, (maximum - 2) / 2, 1, maximum, triangle).ok());
    ASC_DENSE_TEST_EQ(
        test, Check(1, (maximum - 2) / 2 + 1, 1, maximum, triangle).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         Check(1, 0, maximum - 1, maximum, triangle).ok());
    ASC_DENSE_TEST_EQ(test, Check(1, 0, maximum, maximum, triangle).code(),
                      asc::ErrorCode::kOverflow);
  }
  ASC_DENSE_TEST_EQ(
      test,
      asc::internal_band_refinement_limits::Check(
          3, 1, 1, 2, 1, 3, 3, asc::DenseBlasTriangle::kUpper, maximum)
          .code(),
      asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(
      test,
      asc::internal_band_refinement_limits::Check(
          3, 1, 2, 2, 1, 2, 3, asc::DenseBlasTriangle::kUpper, maximum)
          .code(),
      asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(
      test,
      asc::internal_band_refinement_limits::Check(
          3, 1, 2, 2, 1, 3, 2, asc::DenseBlasTriangle::kUpper, maximum)
          .code(),
      asc::ErrorCode::kShape);
  if (maximum < std::numeric_limits<asc::extent_t>::max()) {
    ASC_DENSE_TEST_EQ(test,
                      asc::internal_band_refinement_limits::Check(
                          0, 0, 1, 1, 0, 1, maximum + 1,
                          asc::DenseBlasTriangle::kUpper, maximum)
                          .code(),
                      asc::ErrorCode::kOverflow);
  }
}
}  // namespace
int main() {
  TestContext test;
  Run(test, std::numeric_limits<std::int32_t>::max());
  Run(test, std::numeric_limits<std::int64_t>::max());
  return test.Finish();
}
