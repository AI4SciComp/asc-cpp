#include <array>
#include <cstdint>

#include "../../src/dense/lapack/internal_indefinite_packed_inverse_counts.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  asc_dense_test::TestContext test;
  using asc::internal_indefinite_packed_inverse_counts::Inverse;
  for (const asc::extent_t limit :
       std::array<asc::extent_t, 2>{INT32_MAX, INT64_MAX}) {
    // Independent integer roots of N*(N+1) <= INTEGER_MAX. The full product,
    // not only the final packed size, must fit before the source divides by 2.
    const asc::extent_t bound = limit == INT32_MAX ? 46340 : 3037000499;
    for (const asc::extent_t n :
         std::array<asc::extent_t, 5>{0, 1, 2, bound - 1, bound}) {
      ASC_DENSE_TEST_CHECK(test, Inverse(n, limit).ok());
    }
    for (const asc::extent_t n :
         std::array<asc::extent_t, 4>{bound + 1, limit - 1, limit, INT64_MAX}) {
      ASC_DENSE_TEST_EQ(test, Inverse(n, limit).code(),
                        asc::ErrorCode::kOverflow);
    }
    ASC_DENSE_TEST_EQ(test, Inverse(-1, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  for (const asc::extent_t limit : std::array<asc::extent_t, 3>{-1, 0, 17}) {
    ASC_DENSE_TEST_EQ(test, Inverse(0, limit).code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  return test.Finish();
}
