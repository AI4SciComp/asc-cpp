#include <array>
#include <cstdint>

#include "../../src/dense/lapack/internal_indefinite_packed_refinement_counts.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  asc_dense_test::TestContext test;
  using asc::internal_indefinite_packed_refinement_counts::Refine;
  for (const asc::extent_t limit :
       std::array<asc::extent_t, 2>{INT32_MAX, INT64_MAX}) {
    // Independent roots of the full source N*(N+1) product before division.
    const asc::extent_t bound = limit == INT32_MAX ? 46340 : 3037000499;
    for (const asc::extent_t n :
         std::array<asc::extent_t, 4>{1, 2, bound - 1, bound}) {
      ASC_DENSE_TEST_CHECK(test, Refine(n, 3, n, n, limit).ok());
    }
    ASC_DENSE_TEST_EQ(test,
                      Refine(bound + 1, 1, bound + 1, bound + 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Refine(1, limit, 1, 1, limit).code(),
                      asc::ErrorCode::kOverflow);
    // RFS calls TRS with one compact RHS. No caller NRHS*LDB expression is
    // introduced here; descriptors and private error-buffer bytes are checked
    // separately by the public query.
    ASC_DENSE_TEST_CHECK(test, Refine(2, limit - 1, limit, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Refine(0, limit, 1, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Refine(limit, 0, 1, 1, limit).ok());
    for (auto leading : {0, 1}) {
      ASC_DENSE_TEST_EQ(test, Refine(2, 1, leading, 2, limit).code(),
                        asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_EQ(test, Refine(2, 1, 2, leading, limit).code(),
                        asc::ErrorCode::kInvalidArgument);
    }
    ASC_DENSE_TEST_EQ(test, Refine(-1, 1, 1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, Refine(1, -1, 1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
    if (limit == INT32_MAX) {
      ASC_DENSE_TEST_EQ(test, Refine(limit + 1, 0, 1, 1, limit).code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, Refine(0, limit + 1, 1, 1, limit).code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, Refine(1, 1, limit + 1, 1, limit).code(),
                        asc::ErrorCode::kOverflow);
      ASC_DENSE_TEST_EQ(test, Refine(1, 1, 1, limit + 1, limit).code(),
                        asc::ErrorCode::kOverflow);
    }
  }
  for (const asc::extent_t limit : std::array<asc::extent_t, 3>{-1, 0, 17}) {
    ASC_DENSE_TEST_EQ(test, Refine(0, 0, 1, 1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  return test.Finish();
}
