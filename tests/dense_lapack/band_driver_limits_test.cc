#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_band_driver_limits.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace {
using asc::internal_band_driver_limits::Check;
using asc_dense_test::TestContext;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;

void Boundaries(TestContext& test, asc::extent_t maximum) {
  for (const char mode : {'N', 'E', 'F'}) {
    for (const bool complex_scalar : {false, true}) {
      for (const auto triangle : {kUpper, kLower}) {
        ASC_DENSE_TEST_CHECK(test, Check(16, 3, 4, 4, 2, 16, 16, triangle, mode,
                                         complex_scalar, maximum)
                                       .ok());
        ASC_DENSE_TEST_CHECK(
            test, Check(0, maximum - 1, maximum, maximum, maximum - 1, 1, 1,
                        triangle, mode, complex_scalar, maximum)
                      .ok());
        ASC_DENSE_TEST_EQ(test,
                          Check(0, 0, 1, 1, maximum, 1, 1, triangle, mode,
                                complex_scalar, maximum)
                              .code(),
                          asc::ErrorCode::kOverflow);
        ASC_DENSE_TEST_EQ(test,
                          Check(1, maximum, maximum, maximum, 0, 1, 1, triangle,
                                mode, complex_scalar, maximum)
                              .code(),
                          asc::ErrorCode::kOverflow);
        // NRHS=0 still reaches condition estimation and INTEGER 3*N.
        const auto n = maximum / 3 + 1;
        ASC_DENSE_TEST_EQ(
            test,
            Check(n, 0, 1, 1, 0, n, n, triangle, mode, complex_scalar, maximum)
                .code(),
            asc::ErrorCode::kOverflow);
      }
    }
  }
}

void EquilibrationIndices(TestContext& test, asc::extent_t maximum) {
  // N=1 has no complex off-diagonal update, but the real source forms
  // KD+1+I-J at I=J=1. These are arithmetic-only tests, not matrix views.
  ASC_DENSE_TEST_CHECK(test, Check(1, maximum - 1, maximum, maximum, 0, 1, 1,
                                   kUpper, 'E', true, maximum)
                                 .ok());
  ASC_DENSE_TEST_EQ(test,
                    Check(1, maximum - 1, maximum, maximum, 0, 1, 1, kUpper,
                          'E', false, maximum)
                        .code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test, Check(1, maximum - 2, maximum - 1, maximum - 1, 0,
                                   1, 1, kUpper, 'E', false, maximum)
                                 .ok());
  ASC_DENSE_TEST_CHECK(test, Check(2, maximum - 2, maximum - 1, maximum - 1, 0,
                                   2, 2, kUpper, 'E', true, maximum)
                                 .ok());
  ASC_DENSE_TEST_EQ(test,
                    Check(2, maximum - 2, maximum - 1, maximum - 1, 0, 2, 2,
                          kUpper, 'E', false, maximum)
                        .code(),
                    asc::ErrorCode::kOverflow);
  for (const char mode : {'N', 'F'}) {
    ASC_DENSE_TEST_CHECK(test, Check(2, maximum - 2, maximum - 1, maximum - 1,
                                     0, 2, 2, kUpper, mode, false, maximum)
                                   .ok());
  }
}
}  // namespace

int main() {
  TestContext test;
  for (const asc::extent_t maximum :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<std::int64_t>::max()}) {
    Boundaries(test, maximum);
    EquilibrationIndices(test, maximum);
  }
  return test.Finish();
}
