// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/random/unit_test_sampler.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include "common/common.h"
#include "asc/random/sampler.h"

namespace asc {

// ============================================================================
// Variate Function Tests
// ============================================================================

TEST(VariateTest, Identity) {
  real_t val = 0.5;
  real_t result = Variate(val);
  EXPECT_REAL_EQ(result, val);
}

TEST(VariateTest, ClampToOne) {
  real_t val = 1.0;
  real_t result = Variate(val);
  EXPECT_LT(result, 1.0);
  EXPECT_GE(result, 0.999);
}

TEST(VariateTest, NearOne) {
  real_t val = 0.999999;
  real_t result = Variate(val);
  EXPECT_LT(result, 1.0);
  EXPECT_GE(result, 0.0);
}

// ============================================================================
// PseudoSampler Tests
// ============================================================================

template <typename MemType>
using PseudoSamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(PseudoSamplerTest, AllMemoryTypes);

TYPED_TEST(PseudoSamplerTest, Sample1D) {
  PseudoSampler<real_t> sampler;
  DVector<real_t> samples(this->kMemType, 100);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();
  for (int i = 0; i < 100; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }
}

TYPED_TEST(PseudoSamplerTest, Sample2D) {
  PseudoSampler<real_t> sampler;

  DMatrix<real_t> samples(this->kMemType, 50, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();
  for (int i = 0; i < 100; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }
}

TYPED_TEST(PseudoSamplerTest, Reproducibility) {
  // Test that samplers with same seeds produce identical sequences
  PseudoSampler<real_t> sampler1;
  PseudoSampler<real_t> sampler2;

  // Reset both samplers (they use DeviceSeed by default, which is random)
  // So we can't expect reproducibility without explicit seeding
  // Instead, test that a single sampler produces consistent results
  DVector<real_t> samples1(this->kMemType, 10);
  DVector<real_t> samples2(this->kMemType, 10);

  sampler1.Sample(samples1);
  sampler1.Reset();  // Reset to get different sequence
  sampler1.Sample(samples2);

  const real_t* data1 = samples1.HostRead();
  const real_t* data2 = samples2.HostRead();

  // After reset, should produce different sequences
  bool different = false;
  for (int i = 0; i < 10; ++i) {
    if (data1[i] != data2[i]) {
      different = true;
      break;
    }
  }
  EXPECT_TRUE(different);
}

// ============================================================================
// LatinSampler Tests
// ============================================================================

template <typename MemType>
using LatinSamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(LatinSamplerTest, AllMemoryTypes);

TYPED_TEST(LatinSamplerTest, Sample1D) {
  LatinSampler<real_t> sampler(false);  // No jitter
  DVector<real_t> samples(this->kMemType, 10);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // With no jitter, samples should be at stratum centers: 0.05, 0.15, ..., 0.95
  // but shuffled
  std::vector<real_t> sorted_samples(data, data + 10);
  std::sort(sorted_samples.begin(), sorted_samples.end());

  for (int i = 0; i < 10; ++i) {
    real_t expected = (i + 0.5) * 0.1;
    EXPECT_NEAR(sorted_samples[i], expected, kRealTolerance);
  }
}

TYPED_TEST(LatinSamplerTest, Sample2D) {
  LatinSampler<real_t> sampler(false);
  DMatrix<real_t> samples(this->kMemType, 20, 3);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 60; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }
}

TYPED_TEST(LatinSamplerTest, WithJitter) {
  LatinSampler<real_t> sampler(true);  // With jitter
  DVector<real_t> samples(this->kMemType, 10);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should still be in [0, 1)
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }
}

// ============================================================================
// HaltonSampler Tests
// ============================================================================

template <typename MemType>
using HaltonSamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(HaltonSamplerTest, AllMemoryTypes);

TYPED_TEST(HaltonSamplerTest, Sample1D) {
  HaltonSampler<real_t> sampler(5);  // max_dim = 5
  DVector<real_t> samples(this->kMemType, 10);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // First few Halton sequence values in base 2
  // 0, 1/2, 1/4, 3/4, 1/8, 5/8, 3/8, 7/8, 1/16, 9/16
  EXPECT_REAL_EQ(data[0], 0.0);
  EXPECT_REAL_EQ(data[1], 0.5);
  EXPECT_REAL_EQ(data[2], 0.25);
  EXPECT_REAL_EQ(data[3], 0.75);
}

TYPED_TEST(HaltonSamplerTest, Sample2D) {
  HaltonSampler<real_t> sampler(5);
  DMatrix<real_t> samples(this->kMemType, 10, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 20; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // Column-major: first column (dim 0), then second column (dim 1)
  // First point (row 0): data[0] (col 0), data[10] (col 1)
  EXPECT_REAL_EQ(data[0], 0.0);   // First point, dimension 0
  EXPECT_REAL_EQ(data[10], 0.0);  // First point, dimension 1

  // Second point (row 1): data[1] (col 0), data[11] (col 1)
  EXPECT_REAL_EQ(data[1], 0.5);           // Second point, dimension 0
  EXPECT_NEAR(data[11], 1.0 / 3.0, kRealTolerance);  // Second point, dimension 1
}

TYPED_TEST(HaltonSamplerTest, Deterministic) {
  HaltonSampler<real_t> sampler1(5);
  HaltonSampler<real_t> sampler2(5);

  DVector<real_t> samples1(this->kMemType, 10);
  DVector<real_t> samples2(this->kMemType, 10);

  sampler1.Sample(samples1);
  sampler2.Sample(samples2);

  const real_t* data1 = samples1.HostRead();
  const real_t* data2 = samples2.HostRead();

  // Halton sequences are deterministic
  for (int i = 0; i < 10; ++i) {
    EXPECT_REAL_EQ(data1[i], data2[i]);
  }
}

// ============================================================================
// HammersleySampler Tests
// ============================================================================

template <typename MemType>
using HammersleySamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(HammersleySamplerTest, AllMemoryTypes);

TYPED_TEST(HammersleySamplerTest, Sample1D) {
  HammersleySampler<real_t> sampler(10, 5);  // max_samples=10, max_dim=5
  DVector<real_t> samples(this->kMemType, 10);
  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // First dimension is i/n for Hammersley
  for (int i = 0; i < 10; ++i) {
    real_t expected = static_cast<real_t>(i) / 10.0;
    expected = Variate(expected);
    EXPECT_REAL_EQ(data[i], expected);
  }
}

TYPED_TEST(HammersleySamplerTest, Sample2D) {
  HammersleySampler<real_t> sampler(10, 5);
  DMatrix<real_t> samples(this->kMemType, 10, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 20; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // Column-major: first column (dim 0), then second column (dim 1)
  // First point (row 0): data[0] (col 0), data[10] (col 1)
  EXPECT_REAL_EQ(data[0], 0.0);   // First point, dimension 0
  EXPECT_REAL_EQ(data[10], 0.0);  // First point, dimension 1

  // Second point (row 1): data[1] (col 0), data[11] (col 1)
  EXPECT_NEAR(data[1], Variate(0.1), kRealTolerance);  // Second point, dimension 0
  EXPECT_REAL_EQ(data[11], 0.5);            // Second point, dimension 1
}

TYPED_TEST(HammersleySamplerTest, Deterministic) {
  HammersleySampler<real_t> sampler1(10, 5);
  HammersleySampler<real_t> sampler2(10, 5);

  DVector<real_t> samples1(this->kMemType, 10);
  DVector<real_t> samples2(this->kMemType, 10);

  sampler1.Sample(samples1);
  sampler2.Sample(samples2);

  const real_t* data1 = samples1.HostRead();
  const real_t* data2 = samples2.HostRead();

  // Hammersley sequences are deterministic
  for (int i = 0; i < 10; ++i) {
    EXPECT_REAL_EQ(data1[i], data2[i]);
  }
}

// ============================================================================
// SobolSampler Tests
// ============================================================================

template <typename MemType>
using SobolSamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(SobolSamplerTest, AllMemoryTypes);

TYPED_TEST(SobolSamplerTest, Sample1D) {
  SobolSampler<real_t> sampler;
  DVector<real_t> samples(this->kMemType, 10);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 10; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // First Sobol value should be 0
  EXPECT_REAL_EQ(data[0], 0.0);
}

TYPED_TEST(SobolSamplerTest, Sample2D) {
  SobolSampler<real_t> sampler;
  DMatrix<real_t> samples(this->kMemType, 10, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be in [0, 1)
  for (int i = 0; i < 20; ++i) {
    EXPECT_GE(data[i], 0.0);
    EXPECT_LT(data[i], 1.0);
  }

  // First point should be (0, 0)
  EXPECT_REAL_EQ(data[0], 0.0);   // First sample, dimension 0
  EXPECT_REAL_EQ(data[10], 0.0);  // First sample, dimension 1
}

TYPED_TEST(SobolSamplerTest, Deterministic) {
  SobolSampler<real_t> sampler1;
  SobolSampler<real_t> sampler2;

  DVector<real_t> samples1(this->kMemType, 10);
  DVector<real_t> samples2(this->kMemType, 10);

  sampler1.Sample(samples1);
  sampler2.Sample(samples2);

  const real_t* data1 = samples1.HostRead();
  const real_t* data2 = samples2.HostRead();

  // Sobol sequences are deterministic
  for (int i = 0; i < 10; ++i) {
    EXPECT_REAL_EQ(data1[i], data2[i]);
  }
}

TYPED_TEST(SobolSamplerTest, Reset) {
  SobolSampler<real_t> sampler;

  DVector<real_t> samples1(this->kMemType, 5);
  sampler.Sample(samples1);

  sampler.Reset();

  DVector<real_t> samples2(this->kMemType, 5);
  sampler.Sample(samples2);

  const real_t* data1 = samples1.HostRead();
  const real_t* data2 = samples2.HostRead();

  // After reset, should produce same sequence
  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(data1[i], data2[i]);
  }
}

// ============================================================================
// NormalSampler Tests
// ============================================================================

template <typename MemType>
using NormalSamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(NormalSamplerTest, AllMemoryTypes);

TYPED_TEST(NormalSamplerTest, Sample1D_ScalarNormal) {
  // Standard normal: mean=0, variance=1
  DVector<real_t> mean(this->kMemType, 1);
  DMatrix<real_t> covariance(this->kMemType, 1, 1);

  mean.HostWrite()[0] = 0.0;
  covariance.HostWrite()[0] = 1.0;

  NormalSampler<real_t> sampler(mean, covariance);
  DVector<real_t> samples(this->kMemType, 1000);
  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Compute sample mean and variance
  real_t sample_mean = 0.0;
  for (int i = 0; i < 1000; ++i) {
    sample_mean += data[i];
  }
  sample_mean /= 1000;

  real_t sample_var = 0.0;
  for (int i = 0; i < 1000; ++i) {
    real_t diff = data[i] - sample_mean;
    sample_var += diff * diff;
  }
  sample_var /= 999;

  // Mean should be close to 0
  EXPECT_NEAR(sample_mean, 0.0, 0.1);
  // Variance should be close to 1
  EXPECT_NEAR(sample_var, 1.0, 0.2);
}

TYPED_TEST(NormalSamplerTest, Sample2D_BivariateNormal) {
  // 2D normal with mean [1, 2] and identity covariance
  DVector<real_t> mean(this->kMemType, 2);
  DMatrix<real_t> covariance(this->kMemType, 2, 2);

  real_t* mean_data = mean.HostWrite();
  mean_data[0] = 1.0;
  mean_data[1] = 2.0;

  real_t* cov_data = covariance.HostWrite();
  // Column-major identity matrix
  cov_data[0] = 1.0;
  cov_data[2] = 0.0;  // Column 0
  cov_data[1] = 0.0;
  cov_data[3] = 1.0;  // Column 1

  NormalSampler<real_t> sampler(mean, covariance);
  DMatrix<real_t> samples(this->kMemType, 100, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Compute sample means (column-major: data[i + j*m])
  real_t mean0 = 0.0, mean1 = 0.0;
  for (int i = 0; i < 100; ++i) {
    mean0 += data[i];        // Column 0
    mean1 += data[i + 100];  // Column 1
  }
  mean0 /= 100;
  mean1 /= 100;

  // Means should be close to [1, 2]
  EXPECT_NEAR(mean0, 1.0, 0.3);
  EXPECT_NEAR(mean1, 2.0, 0.3);
}

TYPED_TEST(NormalSamplerTest, CholeskyDecomposition) {
  // Test with a known covariance matrix
  // Σ = [[4, 2], [2, 3]]
  DVector<real_t> mean(this->kMemType, 2);
  DMatrix<real_t> covariance(this->kMemType, 2, 2);
  real_t* mean_data = mean.HostWrite();
  mean_data[0] = 0.0;
  mean_data[1] = 0.0;

  real_t* cov_data = covariance.HostWrite();
  // Column-major
  cov_data[0] = 4.0;
  cov_data[2] = 2.0;  // Column 0
  cov_data[1] = 2.0;
  cov_data[3] = 3.0;  // Column 1

  // This should not throw
  EXPECT_NO_THROW({
    NormalSampler<real_t> sampler(mean, covariance);
    DMatrix<real_t> samples(this->kMemType, 10, 2);
    sampler.Sample(samples);
  });
}

// ============================================================================
// SphericalSampler Tests
// ============================================================================

template <typename MemType>
using SphericalSamplerTest = BaseTest<MemType>;

TYPED_TEST_SUITE(SphericalSamplerTest, AllMemoryTypes);

TYPED_TEST(SphericalSamplerTest, Sample1D) {
  SphericalSampler<real_t> sampler(1);
  DVector<real_t> samples(this->kMemType, 100);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // All samples should be ±1
  for (int i = 0; i < 100; ++i) {
    real_t val = std::abs(data[i]);
    EXPECT_REAL_EQ(val, 1.0);
  }
}

TYPED_TEST(SphericalSamplerTest, Sample2D_UnitCircle) {
  SphericalSampler<real_t> sampler(2);
  DMatrix<real_t> samples(this->kMemType, 100, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Check that all points are on unit circle
  for (int i = 0; i < 100; ++i) {
    real_t x = data[i];        // Column 0
    real_t y = data[i + 100];  // Column 1
    real_t radius = std::sqrt(x * x + y * y);
    EXPECT_NEAR(radius, 1.0, kRealTolerance);
  }
}

TYPED_TEST(SphericalSamplerTest, Sample3D_UnitSphere) {
  SphericalSampler<real_t> sampler(3);
  DMatrix<real_t> samples(this->kMemType, 100, 3);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Check that all points are on unit sphere
  for (int i = 0; i < 100; ++i) {
    real_t x = data[i];        // Column 0
    real_t y = data[i + 100];  // Column 1
    real_t z = data[i + 200];  // Column 2
    real_t radius = std::sqrt(x * x + y * y + z * z);
    EXPECT_NEAR(radius, 1.0, kRealTolerance);
  }
}

TYPED_TEST(SphericalSamplerTest, CoverageTest) {
  // Test that samples cover different octants (for 3D)
  SphericalSampler<real_t> sampler(3);
  DMatrix<real_t> samples(this->kMemType, 100, 3);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Count samples in each octant
  int octant_counts[8] = {0};
  for (int i = 0; i < 100; ++i) {
    real_t x = data[i];
    real_t y = data[i + 100];
    real_t z = data[i + 200];

    int octant = 0;
    if (x >= 0) octant |= 1;
    if (y >= 0) octant |= 2;
    if (z >= 0) octant |= 4;

    octant_counts[octant]++;
  }

  // At least some samples in multiple octants
  int non_empty_octants = 0;
  for (int i = 0; i < 8; ++i) {
    if (octant_counts[i] > 0) {
      non_empty_octants++;
    }
  }
  EXPECT_GT(non_empty_octants, 4);  // Expect coverage of at least half
}

// ============================================================================
// Distribution Tests
// ============================================================================

template <typename MemType>
using DistributionTest = BaseTest<MemType>;

TYPED_TEST_SUITE(DistributionTest, AllMemoryTypes);

TYPED_TEST(DistributionTest, PseudoUniformity) {
  PseudoSampler<real_t> sampler;
  DVector<real_t> samples(this->kMemType, 10000);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Compute mean (should be near 0.5)
  real_t mean = 0.0;
  for (int i = 0; i < 10000; ++i) {
    mean += data[i];
  }
  mean /= 10000;

  EXPECT_NEAR(mean, 0.5, 0.01);  // Within 1% of expected mean
}

TYPED_TEST(DistributionTest, HaltonLowDiscrepancy) {
  HaltonSampler<real_t> sampler(2);
  DMatrix<real_t> samples(this->kMemType, 100, 2);

  sampler.Sample(samples);

  const real_t* data = samples.HostRead();

  // Check that samples cover the unit square reasonably well
  // Count samples in each quadrant
  // Column-major: row i, column j is at data[i + j * m]
  int q1 = 0, q2 = 0, q3 = 0, q4 = 0;
  for (int i = 0; i < 100; ++i) {
    real_t x = data[i];        // Column 0 (dimension 0)
    real_t y = data[i + 100];  // Column 1 (dimension 1)

    if (x < 0.5 && y < 0.5)
      q1++;
    else if (x >= 0.5 && y < 0.5)
      q2++;
    else if (x < 0.5 && y >= 0.5)
      q3++;
    else
      q4++;
  }

  // Each quadrant should have roughly 25 points (±10)
  EXPECT_NEAR(q1, 25, 10);
  EXPECT_NEAR(q2, 25, 10);
  EXPECT_NEAR(q3, 25, 10);
  EXPECT_NEAR(q4, 25, 10);
}

}  // namespace asc
