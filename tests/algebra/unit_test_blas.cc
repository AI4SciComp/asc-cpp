// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/algebra/unit_test_blas.cc
// Author: Yi Cai
// ============================================================================

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "asc/linalg/blas.h"
#include "common/common.h"

namespace asc {

// ============================================================================
// Typed Test Suite Definition
// ============================================================================

template <typename MemType>
using BlasTest = BaseTest<MemType>;

TYPED_TEST_SUITE(BlasTest, AllMemoryTypes);

// ============================================================================
// Category 1: Basic Math Operations
// ============================================================================

TYPED_TEST(BlasTest, Abs) {
  DVector<real_t> x(this->kMemType, 5);
  x << -1.0, 2.0, -3.0, 4.0, -5.0;

  DVector<real_t> y(this->kMemType, 5);
  Abs(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 2.0);
  EXPECT_REAL_EQ(y_ptr[2], 3.0);
  EXPECT_REAL_EQ(y_ptr[3], 4.0);
  EXPECT_REAL_EQ(y_ptr[4], 5.0);
}

TYPED_TEST(BlasTest, Sqrt) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 4.0, 9.0, 16.0;

  DVector<real_t> y(this->kMemType, 4);
  Sqrt(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 2.0);
  EXPECT_REAL_EQ(y_ptr[2], 3.0);
  EXPECT_REAL_EQ(y_ptr[3], 4.0);
}

TYPED_TEST(BlasTest, Square) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, 2.0, 3.0, -4.0, 5.0;

  DVector<real_t> y(this->kMemType, 5);
  Square(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 4.0);
  EXPECT_REAL_EQ(y_ptr[2], 9.0);
  EXPECT_REAL_EQ(y_ptr[3], 16.0);
  EXPECT_REAL_EQ(y_ptr[4], 25.0);
}

TYPED_TEST(BlasTest, Cbrt) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 8.0, 27.0, 64.0;

  DVector<real_t> y(this->kMemType, 4);
  Cbrt(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 2.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 3.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 4.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Reciprocal) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 2.0, 4.0, 5.0;

  DVector<real_t> y(this->kMemType, 4);
  Reciprocal(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 0.5, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 0.25, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 0.2, kRealTolerance);
}

TYPED_TEST(BlasTest, Sign) {
  DVector<real_t> x(this->kMemType, 5);
  x << -3.0, -1.0, 0.0, 2.0, 5.0;

  DVector<real_t> y(this->kMemType, 5);
  Sign(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], -1.0);
  EXPECT_REAL_EQ(y_ptr[1], -1.0);
  EXPECT_REAL_EQ(y_ptr[2], 0.0);
  EXPECT_REAL_EQ(y_ptr[3], 1.0);
  EXPECT_REAL_EQ(y_ptr[4], 1.0);
}

TYPED_TEST(BlasTest, Negative) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, -2.0, 3.0, -4.0;

  DVector<real_t> y(this->kMemType, 4);
  Negative(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], -1.0);
  EXPECT_REAL_EQ(y_ptr[1], 2.0);
  EXPECT_REAL_EQ(y_ptr[2], -3.0);
  EXPECT_REAL_EQ(y_ptr[3], 4.0);
}

TYPED_TEST(BlasTest, Fabs) {
  DVector<real_t> x(this->kMemType, 4);
  x << -1.5, 2.3, -3.7, 4.2;

  DVector<real_t> y(this->kMemType, 4);
  Fabs(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.5, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 2.3, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 3.7, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 4.2, kRealTolerance);
}

// ============================================================================
// Category 2: Exponential & Logarithmic
// ============================================================================

TYPED_TEST(BlasTest, Exp) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 1.0, 2.0;

  DVector<real_t> y(this->kMemType, 3);
  Exp(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::exp(1.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::exp(2.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Exp2) {
  DVector<real_t> x(this->kMemType, 4);
  x << 0.0, 1.0, 2.0, 3.0;

  DVector<real_t> y(this->kMemType, 4);
  Exp2(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 2.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 4.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 8.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Expm1) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 0.001, 1.0;

  DVector<real_t> y(this->kMemType, 3);
  Expm1(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::expm1(0.001), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::expm1(1.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Log) {
  DVector<real_t> x(this->kMemType, 3);
  x << 1.0, std::exp(1.0), std::exp(2.0);

  DVector<real_t> y(this->kMemType, 3);
  Log(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 2.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Log2) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 2.0, 4.0, 8.0;

  DVector<real_t> y(this->kMemType, 4);
  Log2(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 2.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 3.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Log10) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 10.0, 100.0, 1000.0;

  DVector<real_t> y(this->kMemType, 4);
  Log10(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 2.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 3.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Log1p) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 0.001, 1.0;

  DVector<real_t> y(this->kMemType, 3);
  Log1p(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::log1p(0.001), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::log1p(1.0), kRealTolerance);
}

// ============================================================================
// Category 3: Trigonometric Functions
// ============================================================================

TYPED_TEST(BlasTest, Sin) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, M_PI / 6.0, M_PI / 2.0;

  DVector<real_t> y(this->kMemType, 3);
  Sin(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 0.5, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 1.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Cos) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, M_PI / 3.0, M_PI / 2.0;

  DVector<real_t> y(this->kMemType, 3);
  Cos(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 0.5, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 0.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Tan) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, M_PI / 4.0, M_PI / 6.0;

  DVector<real_t> y(this->kMemType, 3);
  Tan(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 1.0 / std::sqrt(3.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Asin) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 0.5, 1.0;

  DVector<real_t> y(this->kMemType, 3);
  Asin(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], M_PI / 6.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], M_PI / 2.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Acos) {
  DVector<real_t> x(this->kMemType, 3);
  x << 1.0, 0.5, 0.0;

  DVector<real_t> y(this->kMemType, 3);
  Acos(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], M_PI / 3.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], M_PI / 2.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Atan) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 1.0, std::sqrt(3.0);

  DVector<real_t> y(this->kMemType, 3);
  Atan(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], M_PI / 4.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], M_PI / 3.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Atan2) {
  DVector<real_t> y_in(this->kMemType, 4);
  DVector<real_t> x_in(this->kMemType, 4);
  y_in << 1.0, 1.0, -1.0, 0.0;
  x_in << 1.0, 0.0, 0.0, 1.0;

  DVector<real_t> z(this->kMemType, 4);
  Atan2(y_in, x_in, z);

  const real_t* z_ptr = z.Read(false);
  EXPECT_NEAR(z_ptr[0], M_PI / 4.0, kRealTolerance);
  EXPECT_NEAR(z_ptr[1], M_PI / 2.0, kRealTolerance);
  EXPECT_NEAR(z_ptr[2], -M_PI / 2.0, kRealTolerance);
  EXPECT_NEAR(z_ptr[3], 0.0, kRealTolerance);
}

// ============================================================================
// Category 4: Hyperbolic Functions
// ============================================================================

TYPED_TEST(BlasTest, Sinh) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 1.0, 2.0;

  DVector<real_t> y(this->kMemType, 3);
  Sinh(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::sinh(1.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::sinh(2.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Cosh) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 1.0, 2.0;

  DVector<real_t> y(this->kMemType, 3);
  Cosh(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::cosh(1.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::cosh(2.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Tanh) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 1.0, 2.0;

  DVector<real_t> y(this->kMemType, 3);
  Tanh(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::tanh(1.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::tanh(2.0), kRealTolerance);
}

// ============================================================================
// Category 5: Rounding Functions
// ============================================================================

TYPED_TEST(BlasTest, Floor) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.5, 2.8, -1.3, -2.7, 3.0;

  DVector<real_t> y(this->kMemType, 5);
  Floor(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 2.0);
  EXPECT_REAL_EQ(y_ptr[2], -2.0);
  EXPECT_REAL_EQ(y_ptr[3], -3.0);
  EXPECT_REAL_EQ(y_ptr[4], 3.0);
}

TYPED_TEST(BlasTest, Ceil) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.5, 2.8, -1.3, -2.7, 3.0;

  DVector<real_t> y(this->kMemType, 5);
  Ceil(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 2.0);
  EXPECT_REAL_EQ(y_ptr[1], 3.0);
  EXPECT_REAL_EQ(y_ptr[2], -1.0);
  EXPECT_REAL_EQ(y_ptr[3], -2.0);
  EXPECT_REAL_EQ(y_ptr[4], 3.0);
}

TYPED_TEST(BlasTest, Round) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.4, 2.5, 3.6, -1.5, -2.4;

  DVector<real_t> y(this->kMemType, 5);
  Round(x, y);

  // std::round uses "round half away from zero":
  // 2.5 -> 3, -1.5 -> -2
  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);   // 1.4 -> 1
  EXPECT_REAL_EQ(y_ptr[1], 3.0);   // 2.5 -> 3 (away from zero)
  EXPECT_REAL_EQ(y_ptr[2], 4.0);   // 3.6 -> 4
  EXPECT_REAL_EQ(y_ptr[3], -2.0);  // -1.5 -> -2 (away from zero)
  EXPECT_REAL_EQ(y_ptr[4], -2.0);  // -2.4 -> -2
}

TYPED_TEST(BlasTest, Trunc) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.9, 2.1, -1.9, -2.1, 3.0;

  DVector<real_t> y(this->kMemType, 5);
  Trunc(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 2.0);
  EXPECT_REAL_EQ(y_ptr[2], -1.0);
  EXPECT_REAL_EQ(y_ptr[3], -2.0);
  EXPECT_REAL_EQ(y_ptr[4], 3.0);
}

// ============================================================================
// Category 6: Special Functions
// ============================================================================

TYPED_TEST(BlasTest, Erf) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 0.5, 1.0;

  DVector<real_t> y(this->kMemType, 3);
  Erf(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 0.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::erf(0.5), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::erf(1.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Erfc) {
  DVector<real_t> x(this->kMemType, 3);
  x << 0.0, 0.5, 1.0;

  DVector<real_t> y(this->kMemType, 3);
  Erfc(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::erfc(0.5), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::erfc(1.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Tgamma) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 2.0, 3.0, 4.0;

  DVector<real_t> y(this->kMemType, 4);
  Tgamma(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[1], 1.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[2], 2.0, kRealTolerance);
  EXPECT_NEAR(y_ptr[3], 6.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Lgamma) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, 2.0, 3.0, 4.0;

  DVector<real_t> y(this->kMemType, 4);
  Lgamma(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_NEAR(y_ptr[0], std::lgamma(1.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[1], std::lgamma(2.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[2], std::lgamma(3.0), kRealTolerance);
  EXPECT_NEAR(y_ptr[3], std::lgamma(4.0), kRealTolerance);
}

// ============================================================================
// Category 7: Binary Arithmetic Operations
// ============================================================================

TYPED_TEST(BlasTest, Max) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 2.0, 6.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  Max(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 3.0);
  EXPECT_REAL_EQ(c_ptr[1], 5.0);
  EXPECT_REAL_EQ(c_ptr[2], 6.0);
  EXPECT_REAL_EQ(c_ptr[3], 8.0);
  EXPECT_REAL_EQ(c_ptr[4], 7.0);
}

TYPED_TEST(BlasTest, Min) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 2.0, 6.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  Min(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 1.0);
  EXPECT_REAL_EQ(c_ptr[1], 2.0);
  EXPECT_REAL_EQ(c_ptr[2], 3.0);
  EXPECT_REAL_EQ(c_ptr[3], 4.0);
  EXPECT_REAL_EQ(c_ptr[4], 2.0);
}

TYPED_TEST(BlasTest, PowArrayArray) {
  DVector<real_t> base(this->kMemType, 4);
  DVector<real_t> exp(this->kMemType, 4);
  base << 2.0, 3.0, 4.0, 5.0;
  exp << 2.0, 3.0, 2.0, 1.0;

  DVector<real_t> result(this->kMemType, 4);
  Pow(base, exp, result);

  const real_t* result_ptr = result.Read(false);
  EXPECT_REAL_EQ(result_ptr[0], 4.0);
  EXPECT_REAL_EQ(result_ptr[1], 27.0);
  EXPECT_REAL_EQ(result_ptr[2], 16.0);
  EXPECT_REAL_EQ(result_ptr[3], 5.0);
}

TYPED_TEST(BlasTest, PowArrayScalar) {
  DVector<real_t> base(this->kMemType, 4);
  base << 2.0, 3.0, 4.0, 5.0;

  DVector<real_t> result(this->kMemType, 4);
  Pow(base, 3.0, result);

  const real_t* result_ptr = result.Read(false);
  EXPECT_REAL_EQ(result_ptr[0], 8.0);
  EXPECT_REAL_EQ(result_ptr[1], 27.0);
  EXPECT_REAL_EQ(result_ptr[2], 64.0);
  EXPECT_REAL_EQ(result_ptr[3], 125.0);
}

TYPED_TEST(BlasTest, Mod) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 10.0, 15.0, 20.0, 25.0;
  b << 3.0, 4.0, 6.0, 7.0;

  DVector<real_t> c(this->kMemType, 4);
  Mod(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_NEAR(c_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(c_ptr[1], 3.0, kRealTolerance);
  EXPECT_NEAR(c_ptr[2], 2.0, kRealTolerance);
  EXPECT_NEAR(c_ptr[3], 4.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Remainder) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 10.0, 15.0, 20.0, 25.0;
  b << 3.0, 4.0, 6.0, 7.0;

  DVector<real_t> c(this->kMemType, 4);
  Remainder(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_NEAR(c_ptr[0], std::remainder(10.0, 3.0), kRealTolerance);
  EXPECT_NEAR(c_ptr[1], std::remainder(15.0, 4.0), kRealTolerance);
  EXPECT_NEAR(c_ptr[2], std::remainder(20.0, 6.0), kRealTolerance);
  EXPECT_NEAR(c_ptr[3], std::remainder(25.0, 7.0), kRealTolerance);
}

TYPED_TEST(BlasTest, Hypot) {
  DVector<real_t> x(this->kMemType, 3);
  DVector<real_t> y(this->kMemType, 3);
  x << 3.0, 5.0, 8.0;
  y << 4.0, 12.0, 15.0;

  DVector<real_t> z(this->kMemType, 3);
  Hypot(x, y, z);

  const real_t* z_ptr = z.Read(false);
  EXPECT_NEAR(z_ptr[0], 5.0, kRealTolerance);
  EXPECT_NEAR(z_ptr[1], 13.0, kRealTolerance);
  EXPECT_NEAR(z_ptr[2], 17.0, kRealTolerance);
}

TYPED_TEST(BlasTest, Clip) {
  DVector<real_t> x(this->kMemType, 6);
  x << -5.0, -2.0, 0.0, 3.0, 7.0, 10.0;

  DVector<real_t> y(this->kMemType, 6);
  Clip(x, -3.0, 5.0, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], -3.0);
  EXPECT_REAL_EQ(y_ptr[1], -2.0);
  EXPECT_REAL_EQ(y_ptr[2], 0.0);
  EXPECT_REAL_EQ(y_ptr[3], 3.0);
  EXPECT_REAL_EQ(y_ptr[4], 5.0);
  EXPECT_REAL_EQ(y_ptr[5], 5.0);
}

// ============================================================================
// Category 8: Logical Operations
// ============================================================================

TYPED_TEST(BlasTest, LogicalNot) {
  DVector<real_t> x(this->kMemType, 4);
  x << 0.0, 1.0, 2.0, 0.0;

  DVector<real_t> y(this->kMemType, 4);
  LogicalNot(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 0.0);
  EXPECT_REAL_EQ(y_ptr[2], 0.0);
  EXPECT_REAL_EQ(y_ptr[3], 1.0);
}

TYPED_TEST(BlasTest, LogicalAnd) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 0.0, 1.0, 0.0, 1.0;
  b << 0.0, 0.0, 1.0, 1.0;

  DVector<real_t> c(this->kMemType, 4);
  LogicalAnd(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 0.0);
  EXPECT_REAL_EQ(c_ptr[2], 0.0);
  EXPECT_REAL_EQ(c_ptr[3], 1.0);
}

TYPED_TEST(BlasTest, LogicalOr) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 0.0, 1.0, 0.0, 1.0;
  b << 0.0, 0.0, 1.0, 1.0;

  DVector<real_t> c(this->kMemType, 4);
  LogicalOr(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 1.0);
  EXPECT_REAL_EQ(c_ptr[2], 1.0);
  EXPECT_REAL_EQ(c_ptr[3], 1.0);
}

TYPED_TEST(BlasTest, LogicalXor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 0.0, 1.0, 0.0, 1.0;
  b << 0.0, 0.0, 1.0, 1.0;

  DVector<real_t> c(this->kMemType, 4);
  LogicalXor(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 1.0);
  EXPECT_REAL_EQ(c_ptr[2], 1.0);
  EXPECT_REAL_EQ(c_ptr[3], 0.0);
}

// ============================================================================
// Category 9: Comparison Operations
// ============================================================================

TYPED_TEST(BlasTest, Greater) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 2.0, 3.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  Greater(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 1.0);
  EXPECT_REAL_EQ(c_ptr[2], 0.0);
  EXPECT_REAL_EQ(c_ptr[3], 1.0);
  EXPECT_REAL_EQ(c_ptr[4], 0.0);
}

TYPED_TEST(BlasTest, GreaterEqual) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 2.0, 3.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  GreaterEqual(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 1.0);
  EXPECT_REAL_EQ(c_ptr[2], 1.0);
  EXPECT_REAL_EQ(c_ptr[3], 1.0);
  EXPECT_REAL_EQ(c_ptr[4], 0.0);
}

TYPED_TEST(BlasTest, Less) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 2.0, 3.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  Less(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 1.0);
  EXPECT_REAL_EQ(c_ptr[1], 0.0);
  EXPECT_REAL_EQ(c_ptr[2], 0.0);
  EXPECT_REAL_EQ(c_ptr[3], 0.0);
  EXPECT_REAL_EQ(c_ptr[4], 1.0);
}

TYPED_TEST(BlasTest, LessEqual) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 2.0, 3.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  LessEqual(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 1.0);
  EXPECT_REAL_EQ(c_ptr[1], 0.0);
  EXPECT_REAL_EQ(c_ptr[2], 1.0);
  EXPECT_REAL_EQ(c_ptr[3], 0.0);
  EXPECT_REAL_EQ(c_ptr[4], 1.0);
}

TYPED_TEST(BlasTest, Equal) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 5.0, 3.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  Equal(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 1.0);
  EXPECT_REAL_EQ(c_ptr[2], 1.0);
  EXPECT_REAL_EQ(c_ptr[3], 0.0);
  EXPECT_REAL_EQ(c_ptr[4], 0.0);
}

TYPED_TEST(BlasTest, NotEqual) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 5.0, 3.0, 8.0, 2.0;
  b << 3.0, 5.0, 3.0, 4.0, 7.0;

  DVector<real_t> c(this->kMemType, 5);
  NotEqual(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 1.0);
  EXPECT_REAL_EQ(c_ptr[1], 0.0);
  EXPECT_REAL_EQ(c_ptr[2], 0.0);
  EXPECT_REAL_EQ(c_ptr[3], 1.0);
  EXPECT_REAL_EQ(c_ptr[4], 1.0);
}

// ============================================================================
// Category 10: Testing Functions
// ============================================================================

TYPED_TEST(BlasTest, IsNan) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, std::numeric_limits<real_t>::quiet_NaN(), 3.0,
      std::numeric_limits<real_t>::quiet_NaN();

  DVector<real_t> y(this->kMemType, 4);
  IsNan(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 0.0);
  EXPECT_REAL_EQ(y_ptr[1], 1.0);
  EXPECT_REAL_EQ(y_ptr[2], 0.0);
  EXPECT_REAL_EQ(y_ptr[3], 1.0);
}

TYPED_TEST(BlasTest, IsInf) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, std::numeric_limits<real_t>::infinity(), 3.0,
      -std::numeric_limits<real_t>::infinity();

  DVector<real_t> y(this->kMemType, 4);
  IsInf(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 0.0);
  EXPECT_REAL_EQ(y_ptr[1], 1.0);
  EXPECT_REAL_EQ(y_ptr[2], 0.0);
  EXPECT_REAL_EQ(y_ptr[3], 1.0);
}

TYPED_TEST(BlasTest, IsFinite) {
  DVector<real_t> x(this->kMemType, 4);
  x << 1.0, std::numeric_limits<real_t>::infinity(), 3.0,
      std::numeric_limits<real_t>::quiet_NaN();

  DVector<real_t> y(this->kMemType, 4);
  IsFinite(x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], 0.0);
  EXPECT_REAL_EQ(y_ptr[2], 1.0);
  EXPECT_REAL_EQ(y_ptr[3], 0.0);
}

// ============================================================================
// Category 11: Linear Algebra Operations
// ============================================================================

TYPED_TEST(BlasTest, Dot) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 2.0, 3.0, 4.0, 5.0;
  b << 2.0, 3.0, 4.0, 5.0, 6.0;

  // Test free function syntax
  real_t dot = Dot(a, b);
  EXPECT_REAL_EQ(dot, 2.0 + 6.0 + 12.0 + 20.0 + 30.0);

  // Test member function syntax
  real_t dot_member = a.Dot(b);
  EXPECT_REAL_EQ(dot_member, 2.0 + 6.0 + 12.0 + 20.0 + 30.0);
}

TYPED_TEST(BlasTest, MatMul_MatrixMatrix) {
  // A: 3x4, B: 4x2, C: 3x2
  DMatrix<real_t> A(this->kMemType, 3, 4);
  DMatrix<real_t> B(this->kMemType, 4, 2);

  // Initialize A (column-major)
  A << 1.0, 2.0, 3.0,     // Column 0
      4.0, 5.0, 6.0,      // Column 1
      7.0, 8.0, 9.0,      // Column 2
      10.0, 11.0, 12.0;   // Column 3

  // Initialize B (column-major)
  B << 1.0, 2.0, 3.0, 4.0,   // Column 0
      5.0, 6.0, 7.0, 8.0;    // Column 1

  DMatrix<real_t> C(this->kMemType, 3, 2);
  MatMul(A, B, C);

  const real_t* C_ptr = C.Read(false);
  EXPECT_REAL_EQ(C_ptr[0], 70.0);    // C(0,0)
  EXPECT_REAL_EQ(C_ptr[1], 80.0);    // C(1,0)
  EXPECT_REAL_EQ(C_ptr[2], 90.0);    // C(2,0)
  EXPECT_REAL_EQ(C_ptr[3], 158.0);   // C(0,1)
  EXPECT_REAL_EQ(C_ptr[4], 184.0);   // C(1,1)
  EXPECT_REAL_EQ(C_ptr[5], 210.0);   // C(2,1)
}

TYPED_TEST(BlasTest, MatMul_MatrixVector) {
  // A: 3x4, x: 4, y: 3
  DMatrix<real_t> A(this->kMemType, 3, 4);
  DVector<real_t> x(this->kMemType, 4);

  // Initialize A (column-major)
  A << 1.0, 2.0, 3.0,     // Column 0
      4.0, 5.0, 6.0,      // Column 1
      7.0, 8.0, 9.0,      // Column 2
      10.0, 11.0, 12.0;   // Column 3

  x << 1.0, 2.0, 3.0, 4.0;

  DVector<real_t> y(this->kMemType, 3);
  MatMul(A, x, y);

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 70.0);
  EXPECT_REAL_EQ(y_ptr[1], 80.0);
  EXPECT_REAL_EQ(y_ptr[2], 90.0);
}

TYPED_TEST(BlasTest, Outer) {
  DVector<real_t> a(this->kMemType, 3);
  DVector<real_t> b(this->kMemType, 4);
  a << 1.0, 2.0, 3.0;
  b << 4.0, 5.0, 6.0, 7.0;

  DMatrix<real_t> C(this->kMemType, 3, 4);
  Outer(a, b, C);

  const real_t* C_ptr = C.Read(false);
  // Column-major: C(i,j) = a(i) * b(j)
  EXPECT_REAL_EQ(C_ptr[0], 4.0);   // C(0,0) = 1*4
  EXPECT_REAL_EQ(C_ptr[1], 8.0);   // C(1,0) = 2*4
  EXPECT_REAL_EQ(C_ptr[2], 12.0);  // C(2,0) = 3*4
  EXPECT_REAL_EQ(C_ptr[3], 5.0);   // C(0,1) = 1*5
  EXPECT_REAL_EQ(C_ptr[4], 10.0);  // C(1,1) = 2*5
  EXPECT_REAL_EQ(C_ptr[5], 15.0);  // C(2,1) = 3*5
}

// ============================================================================
// Category 12: Tensor Contraction Operations
// ============================================================================

TYPED_TEST(BlasTest, TensorDot_MatrixMultiplication) {
  // Test TensorDot as matrix multiplication
  // A: 3x4, B: 4x2, C: 3x2
  DMatrix<real_t> A(this->kMemType, 3, 4);
  DMatrix<real_t> B(this->kMemType, 4, 2);

  A << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0;
  B << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0;

  DMatrix<real_t> C(this->kMemType, 3, 2);

  UArray<int> axes_a({1});
  UArray<int> axes_b({0});
  TensorDot(A, B, axes_a, axes_b, C);

  const real_t* C_ptr = C.Read(false);
  EXPECT_REAL_EQ(C_ptr[0], 70.0);
  EXPECT_REAL_EQ(C_ptr[1], 80.0);
  EXPECT_REAL_EQ(C_ptr[2], 90.0);
}

TYPED_TEST(BlasTest, Kron) {
  // A: 2x2, B: 2x2, C: 4x4
  DMatrix<real_t> A(this->kMemType, 2, 2);
  DMatrix<real_t> B(this->kMemType, 2, 2);

  A << 1.0, 2.0, 3.0, 4.0;
  B << 5.0, 6.0, 7.0, 8.0;

  DMatrix<real_t> C(this->kMemType, 4, 4);
  Kron(A, B, C);

  const real_t* C_ptr = C.Read(false);
  // Kronecker product structure (column-major):
  // First column (j=0): A(0,0)*B(:,0) then A(1,0)*B(:,0)
  EXPECT_REAL_EQ(C_ptr[0], 5.0);   // A(0,0)*B(0,0) = 1*5
  EXPECT_REAL_EQ(C_ptr[1], 6.0);   // A(0,0)*B(1,0) = 1*6
  EXPECT_REAL_EQ(C_ptr[2], 10.0);  // A(1,0)*B(0,0) = 2*5
  EXPECT_REAL_EQ(C_ptr[3], 12.0);  // A(1,0)*B(1,0) = 2*6
}

TYPED_TEST(BlasTest, Cross) {
  DVector<real_t> a(this->kMemType, 3);
  DVector<real_t> b(this->kMemType, 3);
  a << 1.0, 0.0, 0.0;
  b << 0.0, 1.0, 0.0;

  DVector<real_t> c(this->kMemType, 3);
  Cross(a, b, c);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 0.0);
  EXPECT_REAL_EQ(c_ptr[1], 0.0);
  EXPECT_REAL_EQ(c_ptr[2], 1.0);
}

TYPED_TEST(BlasTest, Cross_General) {
  DVector<real_t> a(this->kMemType, 3);
  DVector<real_t> b(this->kMemType, 3);
  a << 2.0, 3.0, 4.0;
  b << 5.0, 6.0, 7.0;

  DVector<real_t> c(this->kMemType, 3);
  Cross(a, b, c);

  const real_t* c_ptr = c.Read(false);
  // c[0] = a[1]*b[2] - a[2]*b[1] = 3*7 - 4*6 = 21 - 24 = -3
  // c[1] = a[2]*b[0] - a[0]*b[2] = 4*5 - 2*7 = 20 - 14 = 6
  // c[2] = a[0]*b[1] - a[1]*b[0] = 2*6 - 3*5 = 12 - 15 = -3
  EXPECT_REAL_EQ(c_ptr[0], -3.0);
  EXPECT_REAL_EQ(c_ptr[1], 6.0);
  EXPECT_REAL_EQ(c_ptr[2], -3.0);
}

TYPED_TEST(BlasTest, StandardLevel1Routines) {
  DVector<real_t> x(this->kMemType, 4);
  DVector<real_t> y(this->kMemType, 4);
  x << 1.0, -2.0, 3.0, -4.0;
  y << 4.0, 3.0, 2.0, 1.0;

  DVector<real_t> z(this->kMemType);
  Copy(x, z);
  Scal(2.0_r, z);
  Axpy(-0.5_r, x, z);
  Axpby(2.0_r, x, 0.5_r, z);

  const real_t* z_ptr = z.Read(false);
  EXPECT_REAL_EQ(z_ptr[0], 2.75);
  EXPECT_REAL_EQ(z_ptr[1], -5.5);
  EXPECT_REAL_EQ(z_ptr[2], 8.25);
  EXPECT_REAL_EQ(z_ptr[3], -11.0);

  EXPECT_NEAR(Nrm2(x), std::sqrt(30.0), kRealTolerance);
  EXPECT_REAL_EQ(Asum(x), 10.0);
  EXPECT_EQ(Iamax(x), 3);

  Swap(x, y);
  const real_t* x_ptr = x.Read(false);
  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(x_ptr[0], 4.0);
  EXPECT_REAL_EQ(x_ptr[1], 3.0);
  EXPECT_REAL_EQ(x_ptr[2], 2.0);
  EXPECT_REAL_EQ(x_ptr[3], 1.0);
  EXPECT_REAL_EQ(y_ptr[0], 1.0);
  EXPECT_REAL_EQ(y_ptr[1], -2.0);
  EXPECT_REAL_EQ(y_ptr[2], 3.0);
  EXPECT_REAL_EQ(y_ptr[3], -4.0);
}

TYPED_TEST(BlasTest, StandardLevel2Routines) {
  DMatrix<real_t> A(this->kMemType, 2, 3);
  A << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  DVector<real_t> x(this->kMemType, 3);
  x << 1.0, 2.0, 3.0;

  DVector<real_t> y(this->kMemType);
  Gemv(A, x, y);
  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], 22.0);
  EXPECT_REAL_EQ(y_ptr[1], 28.0);

  DVector<real_t> tx(this->kMemType, 2);
  tx << 1.0, 2.0;
  DVector<real_t> ty(this->kMemType);
  Gemv(A, tx, ty, 1.0_r, 0.0_r, TransposeMode::kTranspose);
  const real_t* ty_ptr = ty.Read(false);
  EXPECT_REAL_EQ(ty_ptr[0], 5.0);
  EXPECT_REAL_EQ(ty_ptr[1], 11.0);
  EXPECT_REAL_EQ(ty_ptr[2], 17.0);

  DVector<real_t> u(this->kMemType, 2);
  DVector<real_t> v(this->kMemType, 3);
  u << 1.0, 2.0;
  v << 3.0, 4.0, 5.0;
  DMatrix<real_t> G(this->kMemType, 2, 3);
  G = 1.0_r;
  Ger(2.0_r, u, v, G);
  const real_t* g_ptr = G.Read(false);
  EXPECT_REAL_EQ(g_ptr[0], 7.0);
  EXPECT_REAL_EQ(g_ptr[1], 13.0);
  EXPECT_REAL_EQ(g_ptr[2], 9.0);
  EXPECT_REAL_EQ(g_ptr[3], 17.0);
  EXPECT_REAL_EQ(g_ptr[4], 11.0);
  EXPECT_REAL_EQ(g_ptr[5], 21.0);

  DMatrix<real_t> S(this->kMemType, 3, 3);
  S << 4.0, 1.0, 2.0, 0.0, 5.0, 3.0, 0.0, 0.0, 6.0;
  DVector<real_t> sx(this->kMemType, 3);
  sx << 1.0, 2.0, 3.0;
  DVector<real_t> sy(this->kMemType);
  Symv(S, sx, sy, 1.0_r, 0.0_r, TriangleMode::kLower);
  const real_t* sy_ptr = sy.Read(false);
  EXPECT_REAL_EQ(sy_ptr[0], 12.0);
  EXPECT_REAL_EQ(sy_ptr[1], 20.0);
  EXPECT_REAL_EQ(sy_ptr[2], 26.0);

  DMatrix<real_t> L(this->kMemType, 3, 3);
  L << 2.0, 1.0, 4.0, 0.0, 3.0, 5.0, 0.0, 0.0, 6.0;
  DVector<real_t> rhs(this->kMemType, 3);
  rhs << 2.0, 7.0, 32.0;
  Trsv(L, rhs, TriangleMode::kLower);
  const real_t* rhs_ptr = rhs.Read(false);
  EXPECT_NEAR(rhs_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(rhs_ptr[1], 2.0, kRealTolerance);
  EXPECT_NEAR(rhs_ptr[2], 3.0, kRealTolerance);
}

TYPED_TEST(BlasTest, StandardLevel3Routines) {
  DMatrix<real_t> A(this->kMemType, 2, 3);
  DMatrix<real_t> B(this->kMemType, 3, 2);
  A << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  B << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;

  DMatrix<real_t> C(this->kMemType);
  Gemm(A, B, C);
  const real_t* c_ptr = C.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 22.0);
  EXPECT_REAL_EQ(c_ptr[1], 28.0);
  EXPECT_REAL_EQ(c_ptr[2], 49.0);
  EXPECT_REAL_EQ(c_ptr[3], 64.0);

  DMatrix<real_t> AtA(this->kMemType);
  Gemm(A, A, AtA, 1.0_r, 0.0_r, TransposeMode::kTranspose,
       TransposeMode::kNoTranspose);
  const real_t* ata_ptr = AtA.Read(false);
  EXPECT_REAL_EQ(ata_ptr[0], 5.0);
  EXPECT_REAL_EQ(ata_ptr[1], 11.0);
  EXPECT_REAL_EQ(ata_ptr[2], 17.0);
  EXPECT_REAL_EQ(ata_ptr[3], 11.0);
  EXPECT_REAL_EQ(ata_ptr[4], 25.0);
  EXPECT_REAL_EQ(ata_ptr[5], 39.0);
  EXPECT_REAL_EQ(ata_ptr[6], 17.0);
  EXPECT_REAL_EQ(ata_ptr[7], 39.0);
  EXPECT_REAL_EQ(ata_ptr[8], 61.0);

  DMatrix<real_t> AAt(this->kMemType);
  Syrk(A, AAt);
  const real_t* aat_ptr = AAt.Read(false);
  EXPECT_REAL_EQ(aat_ptr[0], 35.0);
  EXPECT_REAL_EQ(aat_ptr[1], 44.0);
  EXPECT_REAL_EQ(aat_ptr[2], 44.0);
  EXPECT_REAL_EQ(aat_ptr[3], 56.0);

  DMatrix<real_t> L(this->kMemType, 2, 2);
  L << 2.0, 1.0, 0.0, 3.0;
  DMatrix<real_t> TB(this->kMemType, 2, 2);
  TB << 1.0, 2.0, 3.0, 4.0;
  Trmm(L, TB);
  const real_t* tb_ptr = TB.Read(false);
  EXPECT_REAL_EQ(tb_ptr[0], 2.0);
  EXPECT_REAL_EQ(tb_ptr[1], 7.0);
  EXPECT_REAL_EQ(tb_ptr[2], 6.0);
  EXPECT_REAL_EQ(tb_ptr[3], 15.0);

  Trsm(L, TB);
  tb_ptr = TB.Read(false);
  EXPECT_NEAR(tb_ptr[0], 1.0, kRealTolerance);
  EXPECT_NEAR(tb_ptr[1], 2.0, kRealTolerance);
  EXPECT_NEAR(tb_ptr[2], 3.0, kRealTolerance);
  EXPECT_NEAR(tb_ptr[3], 4.0, kRealTolerance);

  DMatrix<real_t> S(this->kMemType, 2, 2);
  S << 2.0, 1.0, 0.0, 3.0;
  DMatrix<real_t> SB(this->kMemType, 2, 2);
  DMatrix<real_t> SC(this->kMemType);
  SB << 1.0, 2.0, 3.0, 4.0;
  Symm(S, SB, SC);
  const real_t* sc_ptr = SC.Read(false);
  EXPECT_REAL_EQ(sc_ptr[0], 4.0);
  EXPECT_REAL_EQ(sc_ptr[1], 7.0);
  EXPECT_REAL_EQ(sc_ptr[2], 10.0);
  EXPECT_REAL_EQ(sc_ptr[3], 15.0);
}

}  // namespace asc
