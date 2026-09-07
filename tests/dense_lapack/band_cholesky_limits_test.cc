#include <cstdint>
#include <limits>

#include "../../src/dense/lapack/internal_band_limits.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;

void Prefix(TestContext& test, asc::extent_t maximum) {
  using asc::internal_band_limits::NormalizedDiagonalPrefix;
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(0, 0, true, 0), 0);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, true, 0), 160);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, true, 1), 1);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, true, 32), 32);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, true, 33), 97);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, true, 64), 97);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, true, 65), 129);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, false, 32), 96);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, false, 33), 97);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, false, 64), 128);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(160, 65, false, 65), 129);
  for (const bool blocked : {false, true}) {
    ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(5, 0, blocked, 3), 3);
    ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(5, 9, blocked, 1), 1);
    ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(5, 9, blocked, 2), 5);
    ASC_DENSE_TEST_EQ(
        test, NormalizedDiagonalPrefix(maximum, maximum, blocked, 1), 1);
    ASC_DENSE_TEST_EQ(
        test, NormalizedDiagonalPrefix(maximum, maximum, blocked, 33), maximum);
    ASC_DENSE_TEST_EQ(
        test, NormalizedDiagonalPrefix(maximum, maximum, blocked, maximum),
        maximum);
    ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(5, 2, blocked, -1), 0);
    ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(5, 2, blocked, 6), 0);
  }
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(maximum, maximum, false, 2),
                    maximum);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(maximum, maximum, true, 32),
                    32);
  ASC_DENSE_TEST_EQ(test, NormalizedDiagonalPrefix(80, 80, true, 33), 80);
}

void Run(TestContext& test, asc::extent_t maximum) {
  Prefix(test, maximum);
  using asc::internal_band_limits::CheckFactor;
  using asc::internal_band_limits::CheckSolve;
  using asc::internal_band_limits::CheckStorage;
  ASC_DENSE_TEST_CHECK(test,
                       CheckStorage(0, maximum - 1, maximum, maximum).ok());
  ASC_DENSE_TEST_EQ(test, CheckStorage(0, maximum, maximum, maximum).code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, CheckStorage(0, -1, 1, maximum).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, CheckStorage(3, 2, 2, maximum).code(),
                    asc::ErrorCode::kShape);
  for (const auto triangle : {kUpper, kLower}) {
    for (const bool blocked : {false, true}) {
      ASC_DENSE_TEST_CHECK(
          test, CheckFactor(0, maximum - 1, maximum, triangle, blocked, maximum)
                    .ok());
      ASC_DENSE_TEST_CHECK(
          test,
          CheckFactor(maximum - 1, 0, 1, triangle, blocked, maximum).ok());
      ASC_DENSE_TEST_EQ(
          test, CheckFactor(maximum, 0, 1, triangle, blocked, maximum).code(),
          asc::ErrorCode::kOverflow);
    }
    // A blocked loop's final I+=32 must itself remain representable.
    const auto last_blocked_order = ((maximum - 1) / 32) * 32;
    ASC_DENSE_TEST_CHECK(
        test,
        CheckFactor(last_blocked_order, 65, 66, triangle, true, maximum).ok());
    ASC_DENSE_TEST_EQ(
        test,
        CheckFactor(last_blocked_order + 1, 65, 66, triangle, true, maximum)
            .code(),
        asc::ErrorCode::kOverflow);
    // POTF2's longest strided vector in the first block has IB-1=31 entries.
    const auto last_ld = (maximum - 1) / 31 + 1;
    ASC_DENSE_TEST_CHECK(
        test, CheckFactor(32, 65, last_ld, triangle, true, maximum).ok());
    ASC_DENSE_TEST_EQ(
        test, CheckFactor(32, 65, last_ld + 1, triangle, true, maximum).code(),
        asc::ErrorCode::kOverflow);
    // Empty RHS suppresses all TBSV loops, not KD+1 argument validation.
    ASC_DENSE_TEST_CHECK(test, CheckSolve(maximum, maximum - 1, maximum, 0,
                                          maximum, triangle, maximum)
                                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, CheckSolve(0, maximum - 1, maximum, maximum, 1, triangle, maximum)
                  .ok());
    ASC_DENSE_TEST_EQ(test,
                      CheckSolve(1, 0, 1, maximum, 1, triangle, maximum).code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, CheckSolve(maximum, 0, 1, 1, maximum, triangle, maximum).code(),
        asc::ErrorCode::kOverflow);
  }
  const auto last_upper_ld = (maximum - 1) / 2 + 1;
  ASC_DENSE_TEST_CHECK(
      test, CheckFactor(3, 2, last_upper_ld, kUpper, false, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, CheckFactor(3, 2, last_upper_ld + 1, kUpper, false, maximum).code(),
      asc::ErrorCode::kOverflow);
  // Lower PBTF2 uses unit-stride X even when its rank-update matrix has huge
  // LD.
  ASC_DENSE_TEST_CHECK(test,
                       CheckFactor(3, 2, maximum, kLower, false, maximum).ok());
  ASC_DENSE_TEST_CHECK(
      test, CheckSolve(3, maximum - 1, maximum, 1, 3, kUpper, maximum).ok());
  ASC_DENSE_TEST_EQ(
      test, CheckSolve(3, maximum - 1, maximum, 1, 3, kLower, maximum).code(),
      asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(
      test, CheckSolve(3, maximum - 3, maximum, 1, 3, kLower, maximum).ok());
}
}  // namespace

int main() {
  TestContext test;
  Run(test, std::numeric_limits<std::int32_t>::max());
  Run(test, std::numeric_limits<std::int64_t>::max());
  return test.Finish();
}
