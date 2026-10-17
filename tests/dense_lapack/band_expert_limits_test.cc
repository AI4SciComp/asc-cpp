#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_band_expert_limits.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace {
using asc_dense_test::TestContext;

void Run(TestContext& test, asc::extent_t maximum) {
  using asc::internal_band_expert_limits::CheckEquilibration;
  using asc::internal_band_expert_limits::CheckSimpleDriver;
  ASC_DENSE_TEST_CHECK(
      test, CheckEquilibration(0, maximum - 1, maximum, maximum).ok());
  ASC_DENSE_TEST_CHECK(test,
                       CheckEquilibration(maximum - 1, 0, 1, maximum).ok());
  ASC_DENSE_TEST_EQ(test, CheckEquilibration(maximum, 0, 1, maximum).code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test,
                    CheckEquilibration(0, maximum, maximum, maximum).code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, CheckEquilibration(0, -1, 1, maximum).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, CheckEquilibration(2, 2, 2, maximum).code(),
                    asc::ErrorCode::kShape);
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    ASC_DENSE_TEST_CHECK(test, CheckSimpleDriver(0, maximum - 1, maximum,
                                                 maximum, 1, triangle, maximum)
                                   .ok());
    // Unlike a stand-alone PBTRS quick return, PBSV must factor with NRHS=0.
    ASC_DENSE_TEST_EQ(
        test,
        CheckSimpleDriver(maximum, 0, 1, 0, maximum, triangle, maximum).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test, CheckSimpleDriver(maximum - 1, 0, 1, 0,
                                                 maximum - 1, triangle, maximum)
                                   .ok());
    const auto last_blocked_order = ((maximum - 1) / 32) * 32;
    ASC_DENSE_TEST_CHECK(
        test, CheckSimpleDriver(last_blocked_order, 65, 66, 0,
                                last_blocked_order, triangle, maximum)
                  .ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckSimpleDriver(last_blocked_order + 1, 65, 66, 0,
                          last_blocked_order + 1, triangle, maximum)
            .code(),
        asc::ErrorCode::kOverflow);
    // The nested blocked POTF2 vector cursor still matters without RHS.
    const auto last_ld = (maximum - 1) / 31 + 1;
    ASC_DENSE_TEST_CHECK(
        test,
        CheckSimpleDriver(32, 65, last_ld, 0, 32, triangle, maximum).ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckSimpleDriver(32, 65, last_ld + 1, 0, 32, triangle, maximum).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, CheckSimpleDriver(1, 0, 1, maximum, 1, triangle, maximum).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, CheckSimpleDriver(2, 0, 1, 1, 1, triangle, maximum).code(),
        asc::ErrorCode::kShape);
  }
  // Lower TBSV evaluates J+KD. The upper route subtracts KD instead.
  // N=2 keeps the nested PBTRF vector cursor representable at this exact LD.
  ASC_DENSE_TEST_CHECK(
      test, CheckSimpleDriver(2, maximum - 1, maximum, 1, 2,
                              asc::DenseBlasTriangle::kUpper, maximum)
                .ok());
  ASC_DENSE_TEST_EQ(test,
                    CheckSimpleDriver(2, maximum - 1, maximum, 1, 2,
                                      asc::DenseBlasTriangle::kLower, maximum)
                        .code(),
                    asc::ErrorCode::kOverflow);
}
}  // namespace

int main() {
  TestContext test;
  Run(test, std::numeric_limits<std::int32_t>::max());
  Run(test, std::numeric_limits<std::int64_t>::max());
  return test.Finish();
}
