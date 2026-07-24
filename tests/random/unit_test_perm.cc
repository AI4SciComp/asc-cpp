// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/random/unit_test_perm.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <vector>

#include "common/common.h"
#include "asc/random/permutation.h"

namespace asc {

// ============================================================================
// Prime Number Tests
// ============================================================================

TEST(PrimeTest, FirstPrimes) {
  // Prime::Find uses 1-based indexing: Find(n) returns the n-th prime
  EXPECT_EQ(Prime::Get().Find(1), 2);
  EXPECT_EQ(Prime::Get().Find(2), 3);
  EXPECT_EQ(Prime::Get().Find(3), 5);
  EXPECT_EQ(Prime::Get().Find(4), 7);
  EXPECT_EQ(Prime::Get().Find(5), 11);
  EXPECT_EQ(Prime::Get().Find(6), 13);
}

TEST(PrimeTest, LargerPrimes) {
  // Prime::Find uses 1-based indexing
  EXPECT_EQ(Prime::Get().Find(10), 29);    // 10th prime
  EXPECT_EQ(Prime::Get().Find(25), 97);    // 25th prime
  EXPECT_EQ(Prime::Get().Find(100), 541);  // 100th prime
}

TEST(PrimeTest, Caching) {
  Prime& p = Prime::Get();

  int initial_size = p.GetSize();
  // Request a prime beyond what previous tests cached (LargerPrimes caches 100)
  p.Find(initial_size + 10, true);
  int after_size = p.GetSize();

  EXPECT_GT(after_size, initial_size);
  EXPECT_GE(after_size, initial_size + 10);
}

TEST(PrimeTest, CacheLookup) {
  Prime& p = Prime::Get();

  // Find with caching
  int prime10 = p.Find(10, true);

  // Second call should use cache
  int prime10_cached = p.Find(10, true);

  EXPECT_EQ(prime10, prime10_cached);
}

TEST(PrimeTest, CacheAccess) {
  Prime& p = Prime::Get();
  p.Find(10, true);  // Find 10th prime

  const int* cache = p.GetData();
  ASSERT_NE(cache, nullptr);

  // Cache is 1-indexed: cache[1] is the 1st prime
  EXPECT_EQ(cache[1], 2);
  EXPECT_EQ(cache[2], 3);
  EXPECT_EQ(cache[3], 5);
}

TEST(PrimeTest, CacheSumAccess) {
  Prime& p = Prime::Get();
  p.Find(5, true);  // Find 5th prime

  const int* cache_sum = p.GetCumsum();
  ASSERT_NE(cache_sum, nullptr);

  // Cumulative sum: cache_sum[i] = sum of first i primes (1-indexed)
  // cache_sum[3] = 2 + 3 + 5 = 10
  EXPECT_EQ(cache_sum[3], 10);
}

// ============================================================================
// Radical Inverse Tests
// ============================================================================

TEST(RadicalInverseTest, Base2) {
  // Binary: 5 = 101₂ -> reversed = 0.101₂ = 0.625
  real_t result = RadicalInverse<real_t>(2, 5);
  EXPECT_REAL_EQ(result, 0.625);
}

TEST(RadicalInverseTest, Base3) {
  // Ternary: 4 = 11₃ -> reversed = 0.11₃ = 1/3 + 1/9 = 4/9
  real_t result = RadicalInverse<real_t>(3, 4);
  EXPECT_NEAR(result, 4.0 / 9.0, kRealTolerance);
}

TEST(RadicalInverseTest, Zero) {
  real_t result = RadicalInverse<real_t>(2, 0);
  EXPECT_REAL_EQ(result, 0.0);
}

TEST(RadicalInverseTest, FloatType) {
  float result = RadicalInverse<float>(2, 5);
  EXPECT_FLOAT_EQ(result, 0.625f);
}

// ============================================================================
// Scrambled Radical Inverse Tests
// ============================================================================

TEST(RadicalInverseScrambledTest, IdentityPermutation) {
  int perm[3] = {0, 1, 2};
  real_t result = RadicalInverseScrambled<real_t>(3, perm, 4);
  real_t expected = RadicalInverse<real_t>(3, 4);
  EXPECT_REAL_EQ(result, expected);
}

TEST(RadicalInverseScrambledTest, Permutation) {
  int perm[3] = {0, 2, 1};  // Swap digits 1 and 2
  real_t result = RadicalInverseScrambled<real_t>(3, perm, 4);

  // 4 = 11₃, with permutation: 22₃ = 0.22₃ = 2/3 + 2/9 = 8/9
  EXPECT_NEAR(result, 8.0 / 9.0, kRealTolerance);
}

// ============================================================================
// Multi-dimensional Radical Inverse Tests
// ============================================================================

TEST(RadicalInverseMultiDimTest, Dimension0) {
  // Dimension 0 should use base 2 without scrambling
  real_t result = RadicalInverse<real_t>(0, 2, nullptr, 5);
  real_t expected = RadicalInverse<real_t>(2, 5);
  EXPECT_REAL_EQ(result, expected);
}

TEST(RadicalInverseMultiDimTest, Dimension1) {
  // Dimension 1 should use base 3 without scrambling
  real_t result = RadicalInverse<real_t>(1, 3, nullptr, 4);
  real_t expected = RadicalInverse<real_t>(3, 4);
  EXPECT_REAL_EQ(result, expected);
}

TEST(RadicalInverseMultiDimTest, HigherDimensions) {
  // Dimension 2+ should use prime bases with scrambling
  int perm[5] = {0, 1, 2, 3, 4};
  real_t result = RadicalInverse<real_t>(2, 5, perm, 10);

  // Should return a value in [0, 1)
  EXPECT_GE(result, 0.0);
  EXPECT_LT(result, 1.0);
}

// ============================================================================
// Low-Discrepancy Permutation Tests
// ============================================================================

TEST(LowDiscrepancyPermutationTest, CacheNotNull) {
  const int* cache = LowDiscrepancyPermutation::Get().GetData();
  ASSERT_NE(cache, nullptr);
}

TEST(LowDiscrepancyPermutationTest, Singleton) {
  LowDiscrepancyPermutation& p1 = LowDiscrepancyPermutation::Get();
  LowDiscrepancyPermutation& p2 = LowDiscrepancyPermutation::Get();

  // Should be the same instance
  EXPECT_EQ(&p1, &p2);
}

TEST(LowDiscrepancyPermutationTest, PermutationValues) {
  Prime& prime = Prime::Get();
  prime.Find(10, true);  // Find up to 10th prime

  const int* cache = LowDiscrepancyPermutation::Get().GetData();
  const int* prime_sums = prime.GetCumsum();

  // Check that first prime (2) permutation is identity (0-indexed array, 0 and
  // 1)
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(cache[i], i);
  }

  // Check that second prime (3) permutation is identity
  // Offset is the cumulative sum of the first prime's elements
  int offset =
      prime_sums[1];  // Sum of first 1 prime = size of first permutation
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(cache[offset + i], i);
  }
}

TEST(LowDiscrepancyPermutationTest, ExpandsWithPrimeCache) {
  Prime& prime = Prime::Get();
  LowDiscrepancyPermutation& permutation = LowDiscrepancyPermutation::Get();

  prime.Find(5, true);
  permutation.EnsureSize(5);

  prime.Find(30, true);
  permutation.EnsureSize(prime.GetSize());

  const int* cache = permutation.GetData();
  const int* primes = prime.GetData();
  const int* prime_sums = prime.GetCumsum();

  for (int i = 1; i <= prime.GetSize(); ++i) {
    const int base = primes[i];
    const int offset = prime_sums[i - 1];
    std::vector<int> seen(base, 0);

    for (int j = 0; j < base; ++j) {
      const int value = cache[offset + j];
      ASSERT_GE(value, 0);
      ASSERT_LT(value, base);
      seen[value] += 1;
    }

    for (int value = 0; value < base; ++value) {
      EXPECT_EQ(seen[value], 1);
    }
  }
}

}  // namespace asc
