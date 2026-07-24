// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/generic/unit_test_expr.cc
// Author: Yi Cai
// ============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include "common/common.h"
#include "asc/array/marray.h"
#include "asc/array/expr.h"
// Note: algebra module removed, arithmetic operations now in MObject

namespace asc {

// ============================================================================
// Typed Test Suite Definition
// ============================================================================

template <typename MemType>
using ExprTest = BaseTest<MemType>;

TYPED_TEST_SUITE(ExprTest, AllMemoryTypes);

// ============================================================================
// ObjectExpr Tests
// ============================================================================

TYPED_TEST(ExprTest, ObjectExpr) {
  const bool use_dev = this->kMemType == MemoryType::kDevice;
  DVector<real_t> a(this->kMemType, 5);
  a << 1.0, 2.0, 3.0, 4.0, 5.0;
  a.UseDevice(use_dev);

  ObjectExpr<DVector<real_t>> expr(a);

  EXPECT_EQ(expr.GetSize(), 5);
  EXPECT_EQ(expr.UseDevice(), use_dev);

  const real_t* a_ptr = a.Read(use_dev);
  for (int i = 0; i < 5; ++i) {
    EXPECT_REAL_EQ(expr[i], a_ptr[i]);
  }
}

// ============================================================================
// ScalarExpr Tests
// ============================================================================

TYPED_TEST(ExprTest, ScalarExpr) {
  ScalarExpr<real_t> expr(3.14_r, 10, false);

  EXPECT_EQ(expr.GetSize(), 10);
  EXPECT_FALSE(expr.UseDevice());

  for (int i = 0; i < 10; ++i) {
    EXPECT_REAL_EQ(expr[i], 3.14_r);
  }
}

TEST(ExprObjectTest, UArrayExpression) {
  UArray<real_t> a(4);
  UArray<real_t> b(4);
  UArray<real_t> c(4);
  for (int i = 0; i < 4; ++i) {
    a[i] = static_cast<real_t>(i + 1);
    b[i] = static_cast<real_t>(10 * (i + 1));
  }

  ObjectExpr<UArray<real_t>> object_expr(a);
  EXPECT_EQ(object_expr.GetSize(), 4);
  EXPECT_REAL_EQ(object_expr[2], 3.0);

  auto expr = a + 2.0_r * b - 1.0_r;
  static_assert(ExpressionLike<decltype(expr)>);
  c <<= expr;

  const real_t* c_ptr = c.HostRead();
  EXPECT_REAL_EQ(c_ptr[0], 20.0);
  EXPECT_REAL_EQ(c_ptr[1], 41.0);
  EXPECT_REAL_EQ(c_ptr[2], 62.0);
  EXPECT_REAL_EQ(c_ptr[3], 83.0);
}

TEST(ExprObjectTest, SparseExpressionReadsImplicitZeros) {
  SpDMatrix<real_t, SparseLayoutStride> sparse(DShape<2>(2, 3));
  sparse.Insert(1, 0, 4.0_r);
  sparse.Insert(0, 2, 7.0_r);
  sparse.Finalize();

  ObjectExpr<SpDMatrix<real_t, SparseLayoutStride>> expr(sparse);
  EXPECT_EQ(expr.GetSize(), 6);
  EXPECT_FALSE(expr.UseDevice());
  EXPECT_REAL_EQ(expr[0], 0.0_r);
  EXPECT_REAL_EQ(expr[1], 4.0_r);
  EXPECT_REAL_EQ(expr[2], 0.0_r);
  EXPECT_REAL_EQ(expr[4], 7.0_r);
}

TEST(ExprObjectTest, SparseDenseExpressionEvaluatesToDense) {
  SpDMatrix<real_t, SparseLayoutStride> coo(DShape<2>(2, 3));
  coo.Insert(0, 1, 5.0_r);
  coo.Insert(1, 2, -2.0_r);
  coo.Finalize();
  auto sparse = coo.ToLayout<SparseLayoutRight>();

  DMatrix<real_t> dense(2, 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      dense(i, j) = static_cast<real_t>(10 * i + j);
    }
  }

  DMatrix<real_t> result(2, 3);
  result <<= sparse + 2.0_r * dense;

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      real_t expected = 2.0_r * dense(i, j);
      if (i == 0 && j == 1) expected += 5.0_r;
      if (i == 1 && j == 2) expected -= 2.0_r;
      EXPECT_REAL_EQ(result(i, j), expected);
    }
  }
}

TEST(ExprObjectTest, SparseCompressedTensorExpressionUsesLogicalIndex) {
  SparseMArray<real_t, DShape<3>, SparseLayoutStride> coo(DShape<3>(2, 3, 4));
  coo.Insert(UArray<int>{1, 2, 3}, 9.0_r);
  coo.Finalize();
  auto sparse = coo.ToLayout<SparseLayoutRight>();

  ObjectExpr<SparseMArray<real_t, DShape<3>, SparseLayoutRight>> expr(sparse);
  EXPECT_REAL_EQ(expr[23], 9.0_r);
  EXPECT_REAL_EQ(expr[22], 0.0_r);
}

// ============================================================================
// Binary Expression Tests
// ============================================================================

TYPED_TEST(ExprTest, AddExprExpr) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 1.0, 2.0, 3.0, 4.0, 5.0;
  b << 10.0, 20.0, 30.0, 40.0, 50.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) + ObjectExpr<DVector<real_t>>(b);

  DVector<real_t> c(this->kMemType, 5);
  c <<= expr;

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 11.0);
  EXPECT_REAL_EQ(c_ptr[1], 22.0);
  EXPECT_REAL_EQ(c_ptr[2], 33.0);
  EXPECT_REAL_EQ(c_ptr[3], 44.0);
  EXPECT_REAL_EQ(c_ptr[4], 55.0);
}

TYPED_TEST(ExprTest, SubExprExpr) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  a << 50.0, 40.0, 30.0, 20.0, 10.0;
  b << 10.0, 20.0, 30.0, 20.0, 5.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) - ObjectExpr<DVector<real_t>>(b);

  DVector<real_t> c(this->kMemType, 5);
  c <<= expr;

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 40.0);
  EXPECT_REAL_EQ(c_ptr[1], 20.0);
  EXPECT_REAL_EQ(c_ptr[2], 0.0);
  EXPECT_REAL_EQ(c_ptr[3], 0.0);
  EXPECT_REAL_EQ(c_ptr[4], 5.0);
}

TYPED_TEST(ExprTest, MulExprExpr) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 2.0, 3.0, 4.0, 5.0;
  b << 10.0, 10.0, 10.0, 10.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) * ObjectExpr<DVector<real_t>>(b);

  DVector<real_t> c(this->kMemType, 4);
  c <<= expr;

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 20.0);
  EXPECT_REAL_EQ(c_ptr[1], 30.0);
  EXPECT_REAL_EQ(c_ptr[2], 40.0);
  EXPECT_REAL_EQ(c_ptr[3], 50.0);
}

TYPED_TEST(ExprTest, DivExprExpr) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  a << 100.0, 50.0, 25.0, 10.0;
  b << 10.0, 5.0, 5.0, 2.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) / ObjectExpr<DVector<real_t>>(b);

  DVector<real_t> c(this->kMemType, 4);
  c <<= expr;

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 10.0);
  EXPECT_REAL_EQ(c_ptr[1], 10.0);
  EXPECT_REAL_EQ(c_ptr[2], 5.0);
  EXPECT_REAL_EQ(c_ptr[3], 5.0);
}

// ============================================================================
// Expression with Scalar Tests
// ============================================================================

TYPED_TEST(ExprTest, AddExprScalar) {
  DVector<real_t> a(this->kMemType, 5);
  a << 1.0, 2.0, 3.0, 4.0, 5.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) + 10.0_r;

  DVector<real_t> b(this->kMemType, 5);
  b <<= expr;

  const real_t* b_ptr = b.Read(false);
  EXPECT_REAL_EQ(b_ptr[0], 11.0);
  EXPECT_REAL_EQ(b_ptr[1], 12.0);
  EXPECT_REAL_EQ(b_ptr[2], 13.0);
  EXPECT_REAL_EQ(b_ptr[3], 14.0);
  EXPECT_REAL_EQ(b_ptr[4], 15.0);
}

TYPED_TEST(ExprTest, AddScalarExpr) {
  DVector<real_t> a(this->kMemType, 5);
  a << 1.0, 2.0, 3.0, 4.0, 5.0;

  auto expr = 100.0_r + ObjectExpr<DVector<real_t>>(a);

  DVector<real_t> b(this->kMemType, 5);
  b <<= expr;

  const real_t* b_ptr = b.Read(false);
  EXPECT_REAL_EQ(b_ptr[0], 101.0);
  EXPECT_REAL_EQ(b_ptr[1], 102.0);
  EXPECT_REAL_EQ(b_ptr[2], 103.0);
  EXPECT_REAL_EQ(b_ptr[3], 104.0);
  EXPECT_REAL_EQ(b_ptr[4], 105.0);
}

TYPED_TEST(ExprTest, MulExprScalar) {
  DVector<real_t> a(this->kMemType, 4);
  a << 2.0, 3.0, 4.0, 5.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) * 3.0_r;

  DVector<real_t> b(this->kMemType, 4);
  b <<= expr;

  const real_t* b_ptr = b.Read(false);
  EXPECT_REAL_EQ(b_ptr[0], 6.0);
  EXPECT_REAL_EQ(b_ptr[1], 9.0);
  EXPECT_REAL_EQ(b_ptr[2], 12.0);
  EXPECT_REAL_EQ(b_ptr[3], 15.0);
}

TYPED_TEST(ExprTest, DivExprScalar) {
  DVector<real_t> a(this->kMemType, 4);
  a << 10.0, 20.0, 30.0, 40.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) / 10.0_r;

  DVector<real_t> b(this->kMemType, 4);
  b <<= expr;

  const real_t* b_ptr = b.Read(false);
  EXPECT_REAL_EQ(b_ptr[0], 1.0);
  EXPECT_REAL_EQ(b_ptr[1], 2.0);
  EXPECT_REAL_EQ(b_ptr[2], 3.0);
  EXPECT_REAL_EQ(b_ptr[3], 4.0);
}

// ============================================================================
// Unary Expression Tests
// ============================================================================

TYPED_TEST(ExprTest, NegateExpr) {
  DVector<real_t> a(this->kMemType, 5);
  a << 1.0, -2.0, 3.0, -4.0, 5.0;

  auto expr = -ObjectExpr<DVector<real_t>>(a);

  DVector<real_t> b(this->kMemType, 5);
  b <<= expr;

  const real_t* b_ptr = b.Read(false);
  EXPECT_REAL_EQ(b_ptr[0], -1.0);
  EXPECT_REAL_EQ(b_ptr[1], 2.0);
  EXPECT_REAL_EQ(b_ptr[2], -3.0);
  EXPECT_REAL_EQ(b_ptr[3], 4.0);
  EXPECT_REAL_EQ(b_ptr[4], -5.0);
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TYPED_TEST(ExprTest, ComplexExpr1) {
  // Test: d = a + b * c
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  DVector<real_t> c(this->kMemType, 5);
  a << 1.0, 2.0, 3.0, 4.0, 5.0;
  b << 2.0, 2.0, 2.0, 2.0, 2.0;
  c << 10.0, 20.0, 30.0, 40.0, 50.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) +
              ObjectExpr<DVector<real_t>>(b) * ObjectExpr<DVector<real_t>>(c);

  DVector<real_t> d(this->kMemType, 5);
  d <<= expr;

  const real_t* d_ptr = d.Read(false);
  EXPECT_REAL_EQ(d_ptr[0], 21.0);   // 1 + 2*10
  EXPECT_REAL_EQ(d_ptr[1], 42.0);   // 2 + 2*20
  EXPECT_REAL_EQ(d_ptr[2], 63.0);   // 3 + 2*30
  EXPECT_REAL_EQ(d_ptr[3], 84.0);   // 4 + 2*40
  EXPECT_REAL_EQ(d_ptr[4], 105.0);  // 5 + 2*50
}

TYPED_TEST(ExprTest, ComplexExpr2) {
  // Test: e = (a + b) * (c - d)
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  DVector<real_t> d(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;
  b << 2.0, 3.0, 4.0, 5.0;
  c << 10.0, 20.0, 30.0, 40.0;
  d << 5.0, 10.0, 15.0, 20.0;

  auto expr_lhs =
      ObjectExpr<DVector<real_t>>(a) + ObjectExpr<DVector<real_t>>(b);
  auto expr_rhs =
      ObjectExpr<DVector<real_t>>(c) - ObjectExpr<DVector<real_t>>(d);
  auto expr = expr_lhs * expr_rhs;

  DVector<real_t> e(this->kMemType, 4);
  e <<= expr;

  const real_t* e_ptr = e.Read(false);
  EXPECT_REAL_EQ(e_ptr[0], 15.0);   // (1+2) * (10-5) = 3*5
  EXPECT_REAL_EQ(e_ptr[1], 50.0);   // (2+3) * (20-10) = 5*10
  EXPECT_REAL_EQ(e_ptr[2], 105.0);  // (3+4) * (30-15) = 7*15
  EXPECT_REAL_EQ(e_ptr[3], 180.0);  // (4+5) * (40-20) = 9*20
}

TYPED_TEST(ExprTest, ComplexExpr3) {
  // Test: f = 2*a + (b - c)/d
  DVector<real_t> a(this->kMemType, 3);
  DVector<real_t> b(this->kMemType, 3);
  DVector<real_t> c(this->kMemType, 3);
  DVector<real_t> d(this->kMemType, 3);
  a << 1.0, 2.0, 3.0;
  b << 10.0, 20.0, 30.0;
  c << 5.0, 10.0, 15.0;
  d << 5.0, 5.0, 5.0;

  auto expr =
      2.0_r * ObjectExpr<DVector<real_t>>(a) +
      (ObjectExpr<DVector<real_t>>(b) - ObjectExpr<DVector<real_t>>(c)) /
          ObjectExpr<DVector<real_t>>(d);

  DVector<real_t> f(this->kMemType, 3);
  f <<= expr;

  const real_t* f_ptr = f.Read(false);
  EXPECT_REAL_EQ(f_ptr[0], 3.0);  // 2*1 + (10-5)/5 = 2 + 1
  EXPECT_REAL_EQ(f_ptr[1], 6.0);  // 2*2 + (20-10)/5 = 4 + 2
  EXPECT_REAL_EQ(f_ptr[2], 9.0);  // 2*3 + (30-15)/5 = 6 + 3
}

TYPED_TEST(ExprTest, ComplexExprNested) {
  // Test: g = -((a + b) * c - d) / 2
  DVector<real_t> a(this->kMemType, 3);
  DVector<real_t> b(this->kMemType, 3);
  DVector<real_t> c(this->kMemType, 3);
  DVector<real_t> d(this->kMemType, 3);
  a << 2.0, 4.0, 6.0;
  b << 3.0, 6.0, 9.0;
  c << 2.0, 2.0, 2.0;
  d << 10.0, 20.0, 30.0;

  auto expr_sum =
      ObjectExpr<DVector<real_t>>(a) + ObjectExpr<DVector<real_t>>(b);
  auto expr_prod = expr_sum * ObjectExpr<DVector<real_t>>(c);
  auto expr_diff = expr_prod - ObjectExpr<DVector<real_t>>(d);
  auto expr_neg = -expr_diff;
  auto expr = expr_neg / 2.0_r;

  DVector<real_t> g(this->kMemType, 3);
  g <<= expr;

  const real_t* g_ptr = g.Read(false);
  EXPECT_REAL_EQ(g_ptr[0], 0.0);  // -((2+3)*2 - 10)/2 = -(10-10)/2 = 0
  EXPECT_REAL_EQ(g_ptr[1], 0.0);  // -((4+6)*2 - 20)/2 = -(20-20)/2 = 0
  EXPECT_REAL_EQ(g_ptr[2], 0.0);  // -((6+9)*2 - 30)/2 = -(30-30)/2 = 0
}

// ============================================================================
// Assignment Operator Tests
// ============================================================================

TYPED_TEST(ExprTest, AssignmentOperator) {
  DVector<real_t> a(this->kMemType, 5);
  DVector<real_t> b(this->kMemType, 5);
  DVector<real_t> c(this->kMemType, 5);

  a << 1.0, 2.0, 3.0, 4.0, 5.0;
  b << 10.0, 20.0, 30.0, 40.0, 50.0;

  auto expr = ObjectExpr<DVector<real_t>>(a) + ObjectExpr<DVector<real_t>>(b);
  c <<= expr;

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 11.0);
  EXPECT_REAL_EQ(c_ptr[1], 22.0);
  EXPECT_REAL_EQ(c_ptr[2], 33.0);
  EXPECT_REAL_EQ(c_ptr[3], 44.0);
  EXPECT_REAL_EQ(c_ptr[4], 55.0);
}

// ============================================================================
// Performance Comparison Tests (Conceptual)
// ============================================================================

TYPED_TEST(ExprTest, NoTemporariesCreated) {
  // This test demonstrates that expression templates avoid temporaries
  // by building the expression tree and evaluating in a single pass.
  //
  // Traditional approach (with temporaries):
  //   auto temp1 = a + b;    // temporary 1
  //   auto temp2 = temp1 * c; // temporary 2
  //   auto temp3 = temp2 - d; // temporary 3
  //   result = temp3;
  //
  // Expression template approach (zero temporaries):
  //   result <<= (a + b) * c - d;  // Single pass, no temps

  DVector<real_t> a(this->kMemType, 1000);
  DVector<real_t> b(this->kMemType, 1000);
  DVector<real_t> c(this->kMemType, 1000);
  DVector<real_t> d(this->kMemType, 1000);
  DVector<real_t> result(this->kMemType, 1000);

  a = 1.0;
  b = 2.0;
  c = 3.0;
  d = 4.0;

  // Build expression tree (compile-time, zero cost)
  auto expr_sum =
      ObjectExpr<DVector<real_t>>(a) + ObjectExpr<DVector<real_t>>(b);
  auto expr_prod = expr_sum * ObjectExpr<DVector<real_t>>(c);
  auto expr = expr_prod - ObjectExpr<DVector<real_t>>(d);

  // Evaluate in single pass (runtime, one loop)
  result <<= expr;

  // Verify result: (1+2)*3 - 4 = 9 - 4 = 5
  const real_t* result_ptr = result.Read(false);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_REAL_EQ(result_ptr[i], 5.0);
  }
}

// TYPED_TEST(ExprTest, EvalMemberFunction) {
//   // Test the Expression::Eval() member function for creating new DenseMArray from
//   // expression
//   DVector<real_t> a(this->kMemType, 100);
//   DVector<real_t> b(this->kMemType, 100);

//   a = 1.0;
//   b = 2.0;

//   // Test 1: Simple binary operation with member function Eval()
//   auto c = (a + b * 2.0).Eval();
//   const real_t* c_ptr = c.Read(false);
//   for (int i = 0; i < 100; ++i) {
//     EXPECT_REAL_EQ(c_ptr[i], 5.0);  // 1 + 2*2 = 5
//   }

//   // Test 2: Complex expression with member function Eval()
//   auto d = (2.0 * a + (b - a) / (a + 1.0)).Eval();
//   const real_t* d_ptr = d.Read(false);
//   for (int i = 0; i < 100; ++i) {
//     // 2*1 + (2-1)/(1+1) = 2 + 1/2 = 2.5
//     EXPECT_REAL_EQ(d_ptr[i], 2.5);
//   }

//   // Test 4: Both methods should produce identical results
//   auto member_result = (a - b).Eval();
//   const real_t* member_ptr = member_result.Read(false);
//   for (int i = 0; i < 100; ++i) {
//     EXPECT_REAL_EQ(member_ptr[i], -1.0);  // 1 - 2 = -1
//   }
// }

// ============================================================================
// Unary Operator Tests
// ============================================================================

TYPED_TEST(ExprTest, UnaryMinus) {
  DVector<real_t> x(this->kMemType, 5);
  x << 1.0, -2.0, 3.0, -4.0, 5.0;

  DVector<real_t> y = -x;

  const real_t* y_ptr = y.Read(false);
  EXPECT_REAL_EQ(y_ptr[0], -1.0);
  EXPECT_REAL_EQ(y_ptr[1], 2.0);
  EXPECT_REAL_EQ(y_ptr[2], -3.0);
  EXPECT_REAL_EQ(y_ptr[3], 4.0);
  EXPECT_REAL_EQ(y_ptr[4], -5.0);
}

// ============================================================================
// Binary Arithmetic Operator Tests
// ============================================================================

TYPED_TEST(ExprTest, Addition_TensorTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;
  b << 5.0, 6.0, 7.0, 8.0;

  c <<= (a + b);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 6.0);
  EXPECT_REAL_EQ(c_ptr[1], 8.0);
  EXPECT_REAL_EQ(c_ptr[2], 10.0);
  EXPECT_REAL_EQ(c_ptr[3], 12.0);
}

TYPED_TEST(ExprTest, AdditionAcrossLayoutsUsesLogicalIndices) {
  DMatrix<real_t, LayoutLeft> col_major(this->kMemType, 2, 3);
  DMatrix<real_t, LayoutRight> row_major(this->kMemType, 2, 3);
  DMatrix<real_t, LayoutRight> result(this->kMemType, 2, 3);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      col_major(i, j) = static_cast<real_t>(10 * i + j);
      row_major(i, j) = static_cast<real_t>(100 + 10 * i + j);
    }
  }

  result <<= (col_major + row_major);

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_REAL_EQ(result(i, j),
                     static_cast<real_t>(100 + 20 * i + 2 * j));
    }
  }
}

TYPED_TEST(ExprTest, MixedValueTypesPromoteExpressionResult) {
  DVector<int> ints(this->kMemType, 3);
  DVector<real_t> reals(this->kMemType, 3);
  DVector<real_t> result(this->kMemType, 3);

  ints << 1, 2, 3;
  reals << 0.5_r, 1.5_r, 2.5_r;

  result <<= (ints + reals);

  EXPECT_REAL_EQ(result[0], 1.5_r);
  EXPECT_REAL_EQ(result[1], 3.5_r);
  EXPECT_REAL_EQ(result[2], 5.5_r);
}

TYPED_TEST(ExprTest, TransposedViewExpressionUsesLogicalIndices) {
  DMatrix<real_t> mat(this->kMemType, 2, 3);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      mat(i, j) = static_cast<real_t>(10 * i + j);
    }
  }

  auto transposed = mat.Transpose();
  DMatrix<real_t, LayoutRight> result(this->kMemType, 3, 2);
  result <<= (transposed + 1.0_r);

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      EXPECT_REAL_EQ(result(i, j), mat(j, i) + 1.0_r);
    }
  }
}

TYPED_TEST(ExprTest, Addition_TensorScalar) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;

  c <<= (a + 10.0);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 11.0);
  EXPECT_REAL_EQ(c_ptr[1], 12.0);
  EXPECT_REAL_EQ(c_ptr[2], 13.0);
  EXPECT_REAL_EQ(c_ptr[3], 14.0);
}

TYPED_TEST(ExprTest, Addition_ScalarTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;

  c <<= (10.0 + a);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 11.0);
  EXPECT_REAL_EQ(c_ptr[1], 12.0);
  EXPECT_REAL_EQ(c_ptr[2], 13.0);
  EXPECT_REAL_EQ(c_ptr[3], 14.0);
}

TYPED_TEST(ExprTest, Subtraction_TensorTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 10.0, 20.0, 30.0, 40.0;
  b << 5.0, 6.0, 7.0, 8.0;

  c <<= (a - b);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 5.0);
  EXPECT_REAL_EQ(c_ptr[1], 14.0);
  EXPECT_REAL_EQ(c_ptr[2], 23.0);
  EXPECT_REAL_EQ(c_ptr[3], 32.0);
}

TYPED_TEST(ExprTest, Subtraction_TensorScalar) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 10.0, 20.0, 30.0, 40.0;

  c <<= (a - 5.0);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 5.0);
  EXPECT_REAL_EQ(c_ptr[1], 15.0);
  EXPECT_REAL_EQ(c_ptr[2], 25.0);
  EXPECT_REAL_EQ(c_ptr[3], 35.0);
}

TYPED_TEST(ExprTest, Subtraction_ScalarTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 1.0, 2.0, 3.0, 4.0;

  c <<= (10.0 - a);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 9.0);
  EXPECT_REAL_EQ(c_ptr[1], 8.0);
  EXPECT_REAL_EQ(c_ptr[2], 7.0);
  EXPECT_REAL_EQ(c_ptr[3], 6.0);
}

TYPED_TEST(ExprTest, Multiplication_TensorTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 2.0, 3.0, 4.0, 5.0;
  b << 3.0, 4.0, 5.0, 6.0;

  c <<= (a * b);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 6.0);
  EXPECT_REAL_EQ(c_ptr[1], 12.0);
  EXPECT_REAL_EQ(c_ptr[2], 20.0);
  EXPECT_REAL_EQ(c_ptr[3], 30.0);
}

TYPED_TEST(ExprTest, Multiplication_TensorScalar) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 2.0, 3.0, 4.0, 5.0;

  c <<= (a * 3.0);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 6.0);
  EXPECT_REAL_EQ(c_ptr[1], 9.0);
  EXPECT_REAL_EQ(c_ptr[2], 12.0);
  EXPECT_REAL_EQ(c_ptr[3], 15.0);
}

TYPED_TEST(ExprTest, Multiplication_ScalarTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 2.0, 3.0, 4.0, 5.0;

  c <<= (3.0 * a);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 6.0);
  EXPECT_REAL_EQ(c_ptr[1], 9.0);
  EXPECT_REAL_EQ(c_ptr[2], 12.0);
  EXPECT_REAL_EQ(c_ptr[3], 15.0);
}

TYPED_TEST(ExprTest, Division_TensorTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> b(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 12.0, 15.0, 20.0, 30.0;
  b << 3.0, 5.0, 4.0, 6.0;

  c <<= (a / b);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 4.0);
  EXPECT_REAL_EQ(c_ptr[1], 3.0);
  EXPECT_REAL_EQ(c_ptr[2], 5.0);
  EXPECT_REAL_EQ(c_ptr[3], 5.0);
}

TYPED_TEST(ExprTest, Division_TensorScalar) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 12.0, 15.0, 20.0, 30.0;

  c <<= (a / 5.0);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 2.4);
  EXPECT_REAL_EQ(c_ptr[1], 3.0);
  EXPECT_REAL_EQ(c_ptr[2], 4.0);
  EXPECT_REAL_EQ(c_ptr[3], 6.0);
}

TYPED_TEST(ExprTest, Division_ScalarTensor) {
  DVector<real_t> a(this->kMemType, 4);
  DVector<real_t> c(this->kMemType, 4);
  a << 2.0, 4.0, 5.0, 10.0;

  c <<= (20.0 / a);

  const real_t* c_ptr = c.Read(false);
  EXPECT_REAL_EQ(c_ptr[0], 10.0);
  EXPECT_REAL_EQ(c_ptr[1], 5.0);
  EXPECT_REAL_EQ(c_ptr[2], 4.0);
  EXPECT_REAL_EQ(c_ptr[3], 2.0);
}


}  // namespace asc
