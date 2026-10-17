#include <cstdint>
#include <limits>

#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "internal_rank_revealing_counts.h"

namespace {
using asc::extent_t;
using asc::internal_lapack_rank_revealing::AppendPacking;
using asc::internal_lapack_rank_revealing::MultiplyAdd;
using asc::internal_lapack_rank_revealing::Operation;
using asc::internal_lapack_rank_revealing::QueryCounts;
using asc::internal_lapack_rank_revealing::ReturnedInteger;
using asc_dense_test::TestContext;

template <typename Real>
void Normal(TestContext& test, extent_t limit) {
  asc_dense_test::AllocationProbe allocation;
  for (const bool complex : {false, true}) {
    const auto qr =
        QueryCounts<Real>(Operation::kGeqp3, 3, 2, 0, 3, complex, limit);
    ASC_DENSE_TEST_CHECK(test, qr.ok());
    if (qr.ok()) {
      ASC_DENSE_TEST_EQ(test, qr->minimum, complex ? 3 : 7);
      ASC_DENSE_TEST_EQ(test, qr->preferred, complex ? 96 : 100);
    }
    const auto solve =
        QueryCounts<Real>(Operation::kGelsy, 3, 2, 2, 3, complex, limit);
    ASC_DENSE_TEST_CHECK(test, solve.ok());
    if (solve.ok()) {
      ASC_DENSE_TEST_EQ(test, solve->minimum, complex ? 6 : 9);
      ASC_DENSE_TEST_EQ(test, solve->preferred, 102);
    }
    const auto empty =
        QueryCounts<Real>(Operation::kGelsy, 0, 6, 2, 1, complex, limit);
    ASC_DENSE_TEST_CHECK(test, empty.ok());
    if (empty.ok()) {
      ASC_DENSE_TEST_EQ(test, empty->minimum, complex ? 7 : 1);
      ASC_DENSE_TEST_EQ(test, empty->preferred, complex ? 236 : 1);
    }
    ASC_DENSE_TEST_CHECK(test, QueryCounts<Real>(Operation::kGeqp3, 1, 1, 0,
                                                 limit, complex, limit)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<Real>(Operation::kGeqp3, 32, 130, 0,
                                                  limit, complex, limit)
                                    .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<Real>(Operation::kGeqp3, 34, 34, 0,
                                                  limit, complex, limit)
                                    .ok());
    ASC_DENSE_TEST_EQ(
        test,
        QueryCounts<Real>(Operation::kGeqp3, 1, 2, 0, limit, complex, limit)
            .ok(),
        !complex);
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<Real>(Operation::kGeqp3, 128, 130,
                                                  0, limit, complex, limit)
                                    .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<Real>(Operation::kGeqp3, 129, 130,
                                                  0, limit, complex, limit)
                                    .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<Real>(Operation::kGelsy, 1, 2, 1,
                                                  limit, complex, limit)
                                    .ok());
    ASC_DENSE_TEST_CHECK(test, QueryCounts<Real>(Operation::kGelsy, 1, 1, 1,
                                                 limit, complex, limit)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<Real>(Operation::kGeqp3, limit, 1,
                                                  0, limit, complex, limit)
                                    .ok());
    ASC_DENSE_TEST_CHECK(test, QueryCounts<Real>(Operation::kGeqp3, limit, 0, 0,
                                                 limit, complex, limit)
                                   .ok());
    ASC_DENSE_TEST_CHECK(
        test,
        !QueryCounts<Real>(Operation::kGelsy, limit / 2 + 1, limit / 2 + 1, 0,
                           limit / 2 + 1, complex, limit)
             .ok());
  }
  ASC_DENSE_TEST_EQ(test, allocation.count(), 0U);
}

void Boundaries(TestContext& test) {
  constexpr extent_t kLp = std::numeric_limits<std::int32_t>::max();
  constexpr extent_t kIlp = std::numeric_limits<std::int64_t>::max();
  asc_dense_test::AllocationProbe allocation;
  const auto rounded_up = ReturnedInteger<float>(16777217, true, kLp);
  const auto rounded_down = ReturnedInteger<float>(16777217, false, kLp);
  ASC_DENSE_TEST_CHECK(test, rounded_up.ok() && *rounded_up == 16777218);
  ASC_DENSE_TEST_CHECK(test, rounded_down.ok() && *rounded_down == 16777216);
  ASC_DENSE_TEST_CHECK(test, !ReturnedInteger<float>(kLp, false, kLp).ok());
  ASC_DENSE_TEST_CHECK(test, !ReturnedInteger<float>(kLp, true, kLp).ok());
  ASC_DENSE_TEST_CHECK(test, ReturnedInteger<float>(kLp - 127, true, kLp).ok());
  ASC_DENSE_TEST_CHECK(test, ReturnedInteger<double>(kLp, false, kLp).ok());
  ASC_DENSE_TEST_CHECK(test, !ReturnedInteger<double>(kIlp, false, kIlp).ok());
  ASC_DENSE_TEST_CHECK(test,
                       ReturnedInteger<double>(kIlp - 1023, false, kIlp).ok());
  const auto double_down =
      ReturnedInteger<double>((extent_t{1} << 53) + 1, false, kIlp);
  ASC_DENSE_TEST_CHECK(test,
                       double_down.ok() && *double_down == (extent_t{1} << 53));
  for (const bool complex : {false, true}) {
    // Real-backed blocked paths cannot round their VN2 linked-list indices.
    for (const extent_t n : {(extent_t{1} << 24), (extent_t{1} << 24) + 1}) {
      const auto blocked =
          QueryCounts<float>(Operation::kGeqp3, 129, n, 0, 129, complex, kIlp);
      ASC_DENSE_TEST_EQ(test, blocked.ok(), n == (extent_t{1} << 24));
      ASC_DENSE_TEST_CHECK(test, QueryCounts<float>(Operation::kGeqp3, 128, n,
                                                    0, 128, complex, kIlp)
                                     .ok());
    }
    for (const extent_t n : {(extent_t{1} << 53), (extent_t{1} << 53) + 1}) {
      const auto blocked =
          QueryCounts<double>(Operation::kGeqp3, 129, n, 0, 129, complex, kIlp);
      ASC_DENSE_TEST_EQ(test, blocked.ok(), n == (extent_t{1} << 53));
      ASC_DENSE_TEST_CHECK(test, QueryCounts<double>(Operation::kGeqp3, 128, n,
                                                     0, 128, complex, kIlp)
                                     .ok());
    }
    // Actual GEMV row cursor, independently of total backing-size arithmetic.
    const extent_t lda = (kLp - 1) / 129;
    ASC_DENSE_TEST_CHECK(test, QueryCounts<double>(Operation::kGeqp3, 129, 130,
                                                   0, lda, complex, kLp)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<double>(Operation::kGeqp3, 129, 130,
                                                    0, lda + 1, complex, kLp)
                                    .ok());
    ASC_DENSE_TEST_CHECK(test, QueryCounts<double>(Operation::kGelsy, 1, 2, 1,
                                                   kLp - 1, complex, kLp)
                                   .ok());
    ASC_DENSE_TEST_CHECK(test, !QueryCounts<double>(Operation::kGelsy, 1, 2, 1,
                                                    kLp, complex, kLp)
                                    .ok());
  }
  ASC_DENSE_TEST_CHECK(test, MultiplyAdd(kIlp, 0, 1, kIlp).ok());
  ASC_DENSE_TEST_CHECK(test, !MultiplyAdd(kIlp, 1, 1, kIlp).ok());
  ASC_DENSE_TEST_CHECK(test, !MultiplyAdd(-1, 0, 0, kIlp).ok());
  ASC_DENSE_TEST_CHECK(test, !AppendPacking(kIlp, 1, 1, 8).ok());
  ASC_DENSE_TEST_CHECK(test, AppendPacking(0, 0, kIlp, 8).ok());
  ASC_DENSE_TEST_CHECK(test, !AppendPacking(0, 1, kIlp, 8).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !QueryCounts<float>(Operation::kGeqp3, 1, 1, 0, 1, false, 1024).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !QueryCounts<float>(Operation::kGeqp3, -1, 1, 0, 1, false, kLp).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !QueryCounts<float>(Operation::kGeqp3, 2, 1, 0, 1, false, kLp).ok());
  ASC_DENSE_TEST_EQ(test, allocation.count(), 0U);
}

void RhsCursors(TestContext& test) {
  constexpr extent_t kLimit = std::numeric_limits<std::int32_t>::max();
  // One A column avoids an A row-stride restriction. The packed B cursor
  // reaches precisely 1 + 2*m independently of small query workspace counts.
  const extent_t last_safe_m = (kLimit - 1) / 2;
  asc_dense_test::AllocationProbe allocation;
  for (const bool complex : {false, true}) {
    ASC_DENSE_TEST_CHECK(
        test, QueryCounts<double>(Operation::kGelsy, last_safe_m, 1, 2,
                                  last_safe_m, complex, kLimit)
                  .ok());
    ASC_DENSE_TEST_CHECK(
        test, !QueryCounts<double>(Operation::kGelsy, last_safe_m + 1, 1, 2,
                                   last_safe_m + 1, complex, kLimit)
                   .ok());
    // Empty NRHS takes the documented early return before any B traversal.
    ASC_DENSE_TEST_CHECK(
        test, QueryCounts<double>(Operation::kGelsy, last_safe_m + 1, 1, 0,
                                  last_safe_m + 1, complex, kLimit)
                  .ok());
  }
  ASC_DENSE_TEST_EQ(test, allocation.count(), 0U);
}

void ZeroRowCounts(TestContext& test) {
  constexpr extent_t kLimit = std::numeric_limits<std::int32_t>::max();
  asc_dense_test::AllocationProbe allocation;
  for (const bool complex : {false, true}) {
    const auto counts =
        QueryCounts<double>(Operation::kGeqp3, 0, 6, 0, 1, complex, kLimit);
    ASC_DENSE_TEST_CHECK(test, counts.ok());
    if (counts.ok()) {
      ASC_DENSE_TEST_EQ(test, counts->minimum, 6);
      ASC_DENSE_TEST_EQ(test, counts->preferred, 6);
      ASC_DENSE_TEST_EQ(test, counts->raw_preferred, 1);
      ASC_DENSE_TEST_EQ(test, counts->returned_preferred, 1);
    }
    const extent_t last_safe = (kLimit - 4160) / 32;
    ASC_DENSE_TEST_CHECK(
        test, QueryCounts<double>(Operation::kGeqp3, 0, last_safe, 0, 1,
                                  complex, kLimit)
                  .ok());
    ASC_DENSE_TEST_CHECK(
        test, !QueryCounts<double>(Operation::kGeqp3, 0, last_safe + 1, 0, 1,
                                   complex, kLimit)
                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, !QueryCounts<float>(Operation::kGeqp3, 0, last_safe, 0, 1,
                                  complex, kLimit)
                   .ok());
    ASC_DENSE_TEST_CHECK(
        test, QueryCounts<float>(Operation::kGeqp3, 0, last_safe - 3, 0, 1,
                                 complex, kLimit)
                  .ok());
  }
  ASC_DENSE_TEST_EQ(test, allocation.count(), 0U);
}
}  // namespace

int main() {
  TestContext test;
  for (const extent_t limit :
       {static_cast<extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<extent_t>::max()}) {
    Normal<float>(test, limit);
    Normal<double>(test, limit);
  }
  Boundaries(test);
  RhsCursors(test);
  ZeroRowCounts(test);
  return test.Finish();
}
