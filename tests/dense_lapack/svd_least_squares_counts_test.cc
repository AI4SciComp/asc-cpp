#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_svd_least_squares_counts.h"

namespace {
using asc::internal_lapack_svd_least_squares::Arithmetic;
using asc::internal_lapack_svd_least_squares::DivideTreeAdmission;
using asc::internal_lapack_svd_least_squares::Levels;
using asc::internal_lapack_svd_least_squares::Operation;
using asc::internal_lapack_svd_least_squares::QueryCounts;
using asc_dense_test::TestContext;

template <typename Real>
void Counts(TestContext& test, asc::extent_t limit) {
  for (const Operation operation : {Operation::kGelss, Operation::kGelsd}) {
    for (const bool complex : {false, true}) {
      for (const auto shape : {std::array<asc::extent_t, 2>{0, 0},
                               {0, 3},
                               {3, 0},
                               {1, 1},
                               {5, 3},
                               {3, 5},
                               {28, 26},
                               {26, 28},
                               {53, 53},
                               {1, 1000}}) {
        const auto value = QueryCounts<Real>(
            operation, shape[0], shape[1], 3,
            std::max<asc::extent_t>(1, shape[0]), complex, limit);
        ASC_DENSE_TEST_CHECK(test, value.ok());
        if (value.ok()) {
          ASC_DENSE_TEST_CHECK(test, value->preferred >= value->minimum);
          ASC_DENSE_TEST_CHECK(test, value->minimum >= value->source_minimum);
        }
      }
      for (const auto arguments : {std::array<asc::extent_t, 4>{-1, 2, 1, 1},
                                   {2, -1, 1, 2},
                                   {2, 2, -1, 2},
                                   {2, 2, 1, 1},
                                   {limit, 1, 1, limit},
                                   {1, limit, 1, 1},
                                   {1, 1, limit, 1},
                                   {limit / 2, 1, 3, limit / 2}}) {
        const auto value =
            QueryCounts<Real>(operation, arguments[0], arguments[1],
                              arguments[2], arguments[3], complex, limit);
        ASC_DENSE_TEST_CHECK(test, !value.ok());
        ASC_DENSE_TEST_EQ(test, value.status().code(),
                          asc::ErrorCode::kOverflow);
      }
    }
  }
  // Adjacent safe minima independently calculated from m=1, nrhs=1:
  // source MINWRK=739, fallback=3+n, Path2a=max(741,n+2).
  for (const auto entry : {std::array<asc::extent_t, 2>{736, 739},
                           {737, 740},
                           {738, 741},
                           {739, 741},
                           {740, 742},
                           {1000, 1002}}) {
    const auto value =
        QueryCounts<Real>(Operation::kGelsd, 1, entry[0], 1, 1, false, limit);
    ASC_DENSE_TEST_CHECK(test, value.ok());
    if (value.ok()) {
      ASC_DENSE_TEST_EQ(test, value->source_minimum, 739);
      ASC_DENSE_TEST_EQ(test, value->minimum, entry[1]);
    }
  }
  for (const bool complex : {false, true}) {
    const auto before = QueryCounts<Real>(Operation::kGelss, 1, 1, 1, limit - 1,
                                          complex, limit);
    const auto overflow =
        QueryCounts<Real>(Operation::kGelss, 1, 1, 1, limit, complex, limit);
    const auto divide =
        QueryCounts<Real>(Operation::kGelsd, 1, 1, 1, limit, complex, limit);
    ASC_DENSE_TEST_CHECK(test, before.ok());
    ASC_DENSE_TEST_CHECK(test, !overflow.ok());
    ASC_DENSE_TEST_CHECK(test, divide.ok());
  }
  // An empty query does not execute row/column loops or pivot surrogates.
  const auto empty =
      QueryCounts<Real>(Operation::kGelss, 0, limit, limit, 1, false, limit);
  ASC_DENSE_TEST_CHECK(test, empty.ok());
  ASC_DENSE_TEST_EQ(test, empty->minimum, 1);
  const auto invalid =
      QueryCounts<Real>(Operation::kGelss, 1, 1, 1, 1, false, 7);
  ASC_DENSE_TEST_EQ(test, invalid.status().code(),
                    asc::ErrorCode::kInvalidArgument);
}

void ScalarRounding(TestContext& test) {
  for (const asc::extent_t limit :
       {asc::extent_t{2147483647}, std::numeric_limits<asc::extent_t>::max()}) {
    Arithmetic single(limit);
    ASC_DENSE_TEST_EQ(test, single.Round<float>(16777217), 16777218);
    ASC_DENSE_TEST_CHECK(test, single.valid());
    Arithmetic single_plain(limit);
    ASC_DENSE_TEST_EQ(test, single_plain.Round<float>(16777217, false),
                      16777216);
    ASC_DENSE_TEST_CHECK(test, single_plain.valid());
    Arithmetic overflow(limit);
    overflow.Sum({limit, 1});
    ASC_DENSE_TEST_CHECK(test, !overflow.valid());
    Arithmetic product(limit);
    product.Product(limit / 2 + 1, 2);
    ASC_DENSE_TEST_CHECK(test, !product.valid());
    Arithmetic negative(limit);
    negative.Sum({-1, 2});
    ASC_DENSE_TEST_CHECK(test, !negative.valid());
    Arithmetic conversion(limit);
    conversion.Round<float>(limit);
    ASC_DENSE_TEST_CHECK(test, !conversion.valid());
    Arithmetic crossover(limit);
    crossover.Crossover(limit);
    ASC_DENSE_TEST_CHECK(test, !crossover.valid());
  }
  Arithmetic wide(std::numeric_limits<asc::extent_t>::max());
  ASC_DENSE_TEST_EQ(test, wide.Round<double>(9007199254740993LL, false),
                    9007199254740992LL);
  ASC_DENSE_TEST_CHECK(test, wide.valid());
}

void Tree(TestContext& test) {
  ASC_DENSE_TEST_EQ(test, Levels<float>(13), 0);
  ASC_DENSE_TEST_EQ(test, Levels<float>(14), 1);
  ASC_DENSE_TEST_EQ(test, Levels<double>(25), 1);
  ASC_DENSE_TEST_EQ(test, Levels<double>(26), 1);
  ASC_DENSE_TEST_EQ(test, Levels<double>(51), 1);
  ASC_DENSE_TEST_EQ(test, Levels<double>(52), 2);
  ASC_DENSE_TEST_CHECK(test, DivideTreeAdmission<float>(212991).ok());
  ASC_DENSE_TEST_EQ(test, DivideTreeAdmission<float>(212992).code(),
                    asc::ErrorCode::kUnsupported);
  ASC_DENSE_TEST_EQ(test, DivideTreeAdmission<float>(212993).code(),
                    asc::ErrorCode::kUnsupported);
  ASC_DENSE_TEST_CHECK(test, DivideTreeAdmission<double>(212992).ok());
  ASC_DENSE_TEST_EQ(test, DivideTreeAdmission<double>(-1).code(),
                    asc::ErrorCode::kInvalidArgument);
  // Exhaust every possible input-dependent nontrivial subproblem admitted
  // in S/C, independently using integer tree-size recursion as the oracle.
  for (asc::extent_t size = 26; size < 212992; ++size) {
    const auto levels = Levels<float>(size);
    asc::extent_t maximum_leaf = size;
    for (asc::extent_t i = 0; i < levels; ++i) {
      maximum_leaf /= 2;
    }
    ASC_DENSE_TEST_CHECK(test, maximum_leaf <= 25);
  }
}
}  // namespace

int main() {
  TestContext test;
  {
    asc_dense_test::AllocationProbe probe;
    for (const asc::extent_t limit :
         {asc::extent_t{2147483647},
          std::numeric_limits<asc::extent_t>::max()}) {
      Counts<float>(test, limit);
      Counts<double>(test, limit);
    }
    ScalarRounding(test);
    Tree(test);
    ASC_DENSE_TEST_CHECK(
        test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
  }
  const int result = test.Finish();
  if (result == 0) {
    std::cout << "SVD_LS_COUNTS actual_backing=none synthetic_views=none "
                 "abi_limits=both tree_subproblems=212966 allocations=0\n";
  }
  return result;
}
