#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_indefinite_expert_counts.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"

namespace {
namespace counts = asc::internal_indefinite_expert_counts;
using asc_dense_test::TestContext;

void Boundaries(TestContext& test, asc::extent_t limit) {
  ASC_DENSE_TEST_CHECK(test, counts::Estimator(limit / 3, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Estimator(limit / 3 + 1, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Estimator(-1, limit).ok());
  ASC_DENSE_TEST_CHECK(test, counts::Refinement(0, limit, limit).ok());
  ASC_DENSE_TEST_CHECK(test, counts::Refinement(1, limit - 1, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Refinement(1, limit, limit).ok());
  ASC_DENSE_TEST_CHECK(test, counts::Refinement(limit / 3, 1, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Refinement(limit / 3 + 1, 1, limit).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Refinement(-1, 0, limit).ok());
  const auto real_empty = counts::Expert<float>(0, false, false, limit);
  const auto complex_empty = counts::Expert<double>(0, true, true, limit);
  ASC_DENSE_TEST_CHECK(test, real_empty.ok());
  ASC_DENSE_TEST_CHECK(test, complex_empty.ok());
  if (real_empty.ok() && complex_empty.ok()) {
    ASC_DENSE_TEST_EQ(test, *real_empty, 1);
    ASC_DENSE_TEST_EQ(test, *complex_empty, 1);
  }
  ASC_DENSE_TEST_CHECK(test,
                       counts::Expert<double>(1024, false, false, limit).ok());
  ASC_DENSE_TEST_CHECK(test,
                       counts::Expert<double>(1024, true, false, limit).ok());
  const auto real = counts::Expert<double>(1024, false, false, limit);
  const auto complex = counts::Expert<double>(1024, true, false, limit);
  if (real.ok() && complex.ok()) {
    ASC_DENSE_TEST_EQ(test, *real, 3072);
    ASC_DENSE_TEST_EQ(test, *complex, 2048);
  }
  ASC_DENSE_TEST_CHECK(
      test, counts::Expert<double>(limit / 64 + 1, true, false, limit).ok());
  ASC_DENSE_TEST_CHECK(
      test, !counts::Expert<double>(limit / 64 + 1, true, true, limit).ok());
}

void Rounding(TestContext& test) {
  constexpr auto k32 = std::numeric_limits<std::int32_t>::max();
  constexpr auto k64 = std::numeric_limits<std::int64_t>::max();
  const auto s32 = counts::Rounded<float>(2147483520, k32);
  const auto s64 = counts::Rounded<float>(9223371487098961920LL, k64);
  ASC_DENSE_TEST_CHECK(test, s32.ok());
  ASC_DENSE_TEST_CHECK(test, s64.ok());
  if (s32.ok() && s64.ok()) {
    ASC_DENSE_TEST_EQ(test, *s32, 2147483520);
    ASC_DENSE_TEST_EQ(test, *s64, 9223371487098961920LL);
  }
  ASC_DENSE_TEST_CHECK(test, !counts::Rounded<float>(2147483521, k32).ok());
  ASC_DENSE_TEST_CHECK(
      test, !counts::Rounded<float>(9223371487098961921LL, k64).ok());
  const auto downward = counts::Rounded<double>(288230376151711776LL, k64);
  ASC_DENSE_TEST_CHECK(test, downward.ok());
  if (downward.ok()) {
    ASC_DENSE_TEST_EQ(test, *downward, 288230376151711776LL);
  }
  ASC_DENSE_TEST_CHECK(test, !counts::Rounded<double>(k64, k64).ok());
  ASC_DENSE_TEST_CHECK(test, counts::Driver<float>(33554430, k32).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Driver<float>(33554431, k32).ok());
  ASC_DENSE_TEST_CHECK(test, counts::Driver<double>(33554431, k32).ok());
  ASC_DENSE_TEST_CHECK(test,
                       counts::Driver<float>(144115179485921280LL, k64).ok());
  ASC_DENSE_TEST_CHECK(test,
                       !counts::Driver<float>(144115179485921281LL, k64).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Rounded<float>(-1, k64).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Rounded<double>(1, 65535).ok());
}
}  // namespace

int main() {
  TestContext test;
  Boundaries(test, std::numeric_limits<std::int32_t>::max());
  Boundaries(test, std::numeric_limits<std::int64_t>::max());
  Rounding(test);
  return test.Finish();
}
