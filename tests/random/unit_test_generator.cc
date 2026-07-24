// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/random/unit_test_generator.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include "common/common.h"
#include "asc/random/generator.h"

namespace asc {

// ============================================================================
// Splitmix64 Tests
// ============================================================================

TEST(Splitmix64Test, DefaultConstruction) {
  Splitmix64 gen;
  uint64_t val = gen();
  EXPECT_NE(val, 0);  // Very unlikely to be zero
}

TEST(Splitmix64Test, SeededConstruction) {
  Splitmix64 gen1(42);
  Splitmix64 gen2(42);

  // Same seed should produce same sequence
  EXPECT_EQ(gen1(), gen2());
  EXPECT_EQ(gen1(), gen2());
}

TEST(Splitmix64Test, DifferentSeeds) {
  Splitmix64 gen1(42);
  Splitmix64 gen2(43);

  // Different seeds should produce different values
  EXPECT_NE(gen1(), gen2());
}

TEST(Splitmix64Test, Reseed) {
  Splitmix64 gen;
  gen.Seed(100);
  uint64_t val1 = gen();

  gen.Seed(100);
  uint64_t val2 = gen();

  EXPECT_EQ(val1, val2);
}

TEST(Splitmix64Test, MinMax) {
  EXPECT_EQ(Splitmix64::min(), 0ULL);
  EXPECT_EQ(Splitmix64::max(), UINT64_MAX);
}

// ============================================================================
// Pcg32 Tests
// ============================================================================

TEST(Pcg32Test, DefaultConstruction) {
  Pcg32 gen;
  uint32_t val = gen();
  EXPECT_NE(val, 0);  // Very unlikely to be zero
}

TEST(Pcg32Test, SeededConstruction) {
  Pcg32 gen1(42);
  Pcg32 gen2(42);

  // Same seed should produce same sequence
  EXPECT_EQ(gen1(), gen2());
  EXPECT_EQ(gen1(), gen2());
}

TEST(Pcg32Test, StateStreamConstruction) {
  Pcg32 gen1(1000, 2000);
  Pcg32 gen2(1000, 2000);

  EXPECT_EQ(gen1(), gen2());
}

TEST(Pcg32Test, MinMax) {
  EXPECT_EQ(Pcg32::min(), 0U);
  EXPECT_EQ(Pcg32::max(), UINT32_MAX);
}

// ============================================================================
// Xoroshiro64Star Tests
// ============================================================================

TEST(Xoroshiro64StarTest, DefaultConstruction) {
  Xoroshiro64Star gen;
  uint32_t val = gen();
  EXPECT_NE(val, 0);
}

TEST(Xoroshiro64StarTest, SeededConstruction) {
  Xoroshiro64Star gen1(42);
  Xoroshiro64Star gen2(42);

  EXPECT_EQ(gen1(), gen2());
  EXPECT_EQ(gen1(), gen2());
}

TEST(Xoroshiro64StarTest, MinMax) {
  EXPECT_EQ(Xoroshiro64Star::min(), 0U);
  EXPECT_EQ(Xoroshiro64Star::max(), UINT32_MAX);
}

// ============================================================================
// Xoroshiro128Plus Tests
// ============================================================================

TEST(Xoroshiro128PlusTest, DefaultConstruction) {
  Xoroshiro128Plus gen;
  uint64_t val = gen();
  EXPECT_NE(val, 0);
}

TEST(Xoroshiro128PlusTest, SeededConstruction) {
  Xoroshiro128Plus gen1(42);
  Xoroshiro128Plus gen2(42);

  EXPECT_EQ(gen1(), gen2());
  EXPECT_EQ(gen1(), gen2());
}

TEST(Xoroshiro128PlusTest, MinMax) {
  EXPECT_EQ(Xoroshiro128Plus::min(), 0ULL);
  EXPECT_EQ(Xoroshiro128Plus::max(), UINT64_MAX);
}

// ============================================================================
// UniformGenerator Tests
// ============================================================================

TEST(UniformGeneratorTest, RealRange) {
  UniformGenerator<real_t> gen(0.0, 1.0);

  for (int i = 0; i < 100; ++i) {
    real_t val = gen.Next();
    EXPECT_GE(val, 0.0);
    EXPECT_LT(val, 1.0);
  }
}

TEST(UniformGeneratorTest, IntegerRange) {
  UniformGenerator<int> gen(1, 10);

  for (int i = 0; i < 100; ++i) {
    int val = gen.Next();
    EXPECT_GE(val, 1);
    EXPECT_LE(val, 10);
  }
}

TEST(UniformGeneratorTest, Reset) {
  UniformGenerator<real_t, TimeSeed> gen(0.0, 1.0);
  gen.Reset();  // Should not crash
  real_t val = gen.Next();
  EXPECT_GE(val, 0.0);
  EXPECT_LT(val, 1.0);
}

TEST(UniformGeneratorTest, ExplicitSeed) {
  UniformGenerator<real_t> gen1(0.0, 1.0);
  UniformGenerator<real_t> gen2(0.0, 1.0);
  gen1.SetSeed(2025);
  gen2.SetSeed(2025);

  EXPECT_EQ(gen1.Next(), gen2.Next());
  EXPECT_EQ(gen1.Next(), gen2.Next());
}

// ============================================================================
// NormalGenerator Tests
// ============================================================================

TEST(NormalGeneratorTest, MeanStdDev) {
  NormalGenerator<real_t> gen(0.0, 1.0);

  real_t sum = 0.0;
  const int n = 10000;

  for (int i = 0; i < n; ++i) {
    sum += gen.Next();
  }

  real_t mean = sum / n;
  // Mean should be close to 0 (within 3 sigma / sqrt(n))
  EXPECT_NEAR(mean, 0.0, 0.03);
}

}  // namespace asc
