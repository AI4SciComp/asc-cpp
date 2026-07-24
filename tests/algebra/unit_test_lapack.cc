// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/algebra/unit_test_lapack.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include "common/common.h"
#include "asc/linalg/lapack.h"

namespace asc {

// ============================================================================
// Test Fixture
// ============================================================================

template <typename MemType>
using LinearSolverTest = BaseTest<MemType>;

TYPED_TEST_SUITE(LinearSolverTest, AllMemoryTypes);

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Compute residual norm ||Ax - b|| / ||b||
template <typename T>
T ComputeResidual(const DMatrix<T>& A, const DVector<T>& x,
                  const DVector<T>& b) {
  const int N = A.GetExtent(0);
  const T* A_data = A.Read(false);
  const T* x_data = x.Read(false);
  const T* b_data = b.Read(false);

  T residual_norm = 0;
  T b_norm = 0;

  for (int i = 0; i < N; ++i) {
    T Ax_i = 0;
    for (int j = 0; j < N; ++j) {
      Ax_i += A_data[j * N + i] * x_data[j];  // A(i,j) * x[j]
    }
    T r_i = Ax_i - b_data[i];
    residual_norm += r_i * r_i;
    b_norm += b_data[i] * b_data[i];
  }

  return std::sqrt(residual_norm) / std::sqrt(b_norm + 1e-30);
}

// ============================================================================
// Basic Tests
// ============================================================================

#ifdef ASC_USE_EIGEN

TYPED_TEST(LinearSolverTest, BasicSolve3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Set up a simple system (column-major)
  // A = [2 1 1; 4 3 3; 8 7 9]
  A << 2.0, 4.0, 8.0,  // Column 0
      1.0, 3.0, 7.0,   // Column 1
      1.0, 3.0, 9.0;   // Column 2

  // b = [4, 10, 24]
  b << 4.0, 10.0, 24.0;

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Check status
  EXPECT_TRUE(solver.IsReady());
  // Should use PartialPivLU for general matrix
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kPartialPivLU);

  // Check residual
  real_t residual = ComputeResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, IdentityMatrix) {
  const int N = 5;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Identity matrix
  real_t* A_data = A.HostWrite();
  for (int i = 0; i < N * N; ++i) {
    A_data[i] = 0.0;
  }
  for (int i = 0; i < N; ++i) {
    A_data[i * N + i] = 1.0;  // A(i,i) = 1
  }

  // RHS
  b << 1.0, 2.0, 3.0, 4.0, 5.0;

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as diagonal
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kDiagonal);

  // x should equal b for identity matrix
  const real_t* x_data = x.Read(false);
  const real_t* b_data = b.Read(false);
  for (int i = 0; i < N; ++i) {
    EXPECT_NEAR(x_data[i], b_data[i], 1e-14);
  }
}

TYPED_TEST(LinearSolverTest, DiagonalMatrix) {
  const int N = 4;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Diagonal matrix: A = diag(2, 3, 4, 5)
  real_t* A_data = A.HostWrite();
  for (int i = 0; i < N * N; ++i) {
    A_data[i] = 0.0;
  }
  for (int i = 0; i < N; ++i) {
    A_data[i * N + i] = static_cast<real_t>(i + 2);  // A(i,i) = i + 2
  }

  // b = [2, 6, 12, 20]
  b << 2.0, 6.0, 12.0, 20.0;

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as diagonal
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kDiagonal);

  // Expected: x = [1, 2, 3, 4]
  const real_t* x_data = x.Read(false);
  for (int i = 0; i < N; ++i) {
    EXPECT_NEAR(x_data[i], static_cast<real_t>(i + 1), 1e-14);
  }
}

// ============================================================================
// Triangular Matrix Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, LowerTriangularMatrix) {
  const int N = 4;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Lower triangular matrix (column-major)
  real_t* A_data = A.HostWrite();
  for (int i = 0; i < N * N; ++i) {
    A_data[i] = 0.0;
  }
  // L = [2 0 0 0; 1 3 0 0; 1 1 4 0; 1 1 1 5]
  A_data[0 * N + 0] = 2.0;  // L(0,0)
  A_data[0 * N + 1] = 1.0;  // L(1,0)
  A_data[0 * N + 2] = 1.0;  // L(2,0)
  A_data[0 * N + 3] = 1.0;  // L(3,0)
  A_data[1 * N + 1] = 3.0;  // L(1,1)
  A_data[1 * N + 2] = 1.0;  // L(2,1)
  A_data[1 * N + 3] = 1.0;  // L(3,1)
  A_data[2 * N + 2] = 4.0;  // L(2,2)
  A_data[2 * N + 3] = 1.0;  // L(3,2)
  A_data[3 * N + 3] = 5.0;  // L(3,3)

  // b = [2, 4, 7, 11]
  b << 2.0, 4.0, 7.0, 11.0;

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as lower triangular
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kLowerTriangular);

  // Verify solution
  real_t residual = ComputeResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, UpperTriangularMatrix) {
  const int N = 4;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Upper triangular matrix (column-major)
  real_t* A_data = A.HostWrite();
  for (int i = 0; i < N * N; ++i) {
    A_data[i] = 0.0;
  }
  // U = [2 1 1 1; 0 3 1 1; 0 0 4 1; 0 0 0 5]
  A_data[0 * N + 0] = 2.0;  // U(0,0)
  A_data[1 * N + 0] = 1.0;  // U(0,1)
  A_data[2 * N + 0] = 1.0;  // U(0,2)
  A_data[3 * N + 0] = 1.0;  // U(0,3)
  A_data[1 * N + 1] = 3.0;  // U(1,1)
  A_data[2 * N + 1] = 1.0;  // U(1,2)
  A_data[3 * N + 1] = 1.0;  // U(1,3)
  A_data[2 * N + 2] = 4.0;  // U(2,2)
  A_data[3 * N + 2] = 1.0;  // U(2,3)
  A_data[3 * N + 3] = 5.0;  // U(3,3)

  // b = [5, 5, 5, 5]
  b << 5.0, 5.0, 5.0, 5.0;

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as upper triangular
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kUpperTriangular);

  // Verify solution
  real_t residual = ComputeResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

// ============================================================================
// SPD Matrix Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, SPDMatrix3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // SPD matrix (diagonally dominant)
  // A = [4 1 1; 1 4 1; 1 1 4]
  A << 4.0, 1.0, 1.0,  // Column 0
      1.0, 4.0, 1.0,   // Column 1
      1.0, 1.0, 4.0;   // Column 2

  // b = [6, 6, 6]
  b << 6.0, 6.0, 6.0;

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as symmetric and use LLT or LDLT
  SolverMethod method = solver.GetMethod();
  EXPECT_TRUE(method == SolverMethod::kLLT || method == SolverMethod::kLDLT);

  // Check residual
  real_t residual = ComputeResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);

  // Expected solution: x = [1, 1, 1]
  const real_t* x_data = x.Read(false);
  for (int i = 0; i < N; ++i) {
    EXPECT_NEAR(x_data[i], 1.0, kRealTolerance);
  }
}

// ============================================================================
// Repeated Solves Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, RepeatedSolvesDifferentRHS) {
  const int N = 4;
  DMatrix<real_t> A(this->kMemType, N, N);

  // Random-like matrix (column-major)
  A << 5.0, 1.0, 2.0, 1.0,  // Column 0
      2.0, 6.0, 1.0, 2.0,   // Column 1
      1.0, 2.0, 7.0, 1.0,   // Column 2
      2.0, 1.0, 2.0, 8.0;   // Column 3

  // Setup solver once
  LinearSolver<real_t> solver;
  solver.SetOperator(A);

  // Solve with multiple RHS
  for (int k = 0; k < 5; ++k) {
    DVector<real_t> b(this->kMemType, N);
    DVector<real_t> x(this->kMemType, N);

    // Different RHS each time
    real_t* b_data = b.HostWrite();
    for (int i = 0; i < N; ++i) {
      b_data[i] = static_cast<real_t>((k + 1) * (i + 1));
    }

    solver.LinearSolve(b, x);

    // Verify
    real_t residual = ComputeResidual(A, x, b);
    EXPECT_LT(residual, kRealTolerance);
  }
}

// ============================================================================
// Multiple RHS Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, SolveMultipleRHS) {
  const int N = 3;
  const int M = 4;  // Number of RHS vectors
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> B(this->kMemType, N, M);
  DMatrix<real_t> X(this->kMemType, N, M);

  // Setup matrix
  A << 3.0, 1.0, 1.0,  // Column 0
      1.0, 4.0, 1.0,   // Column 1
      1.0, 1.0, 5.0;   // Column 2

  // Setup multiple RHS (column-major)
  B << 5.0, 6.0, 7.0,   // Column 0 (first RHS)
      6.0, 9.0, 7.0,    // Column 1 (second RHS)
      7.0, 7.0, 11.0,   // Column 2 (third RHS)
      10.0, 10.0, 10.0;  // Column 3 (fourth RHS)

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.SolveMultiple(B, X);

  // Verify each column
  const real_t* A_data = A.Read(false);
  const real_t* B_data = B.Read(false);
  const real_t* X_data = X.Read(false);

  for (int k = 0; k < M; ++k) {
    // Compute residual for column k
    real_t residual_norm = 0;
    real_t b_norm = 0;

    for (int i = 0; i < N; ++i) {
      real_t Ax_i = 0;
      for (int j = 0; j < N; ++j) {
        Ax_i += A_data[j * N + i] * X_data[k * N + j];
      }
      real_t b_i = B_data[k * N + i];
      real_t r_i = Ax_i - b_i;
      residual_norm += r_i * r_i;
      b_norm += b_i * b_i;
    }

    real_t rel_residual = std::sqrt(residual_norm) / std::sqrt(b_norm + 1e-30);
    EXPECT_LT(rel_residual, kRealTolerance);
  }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, ConvenienceFunction) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Setup
  A << 2.0, 1.0, 1.0,
      1.0, 3.0, 1.0,
      1.0, 1.0, 4.0;
  b << 4.0, 5.0, 6.0;

  // Use convenience function
  LinearSolve<real_t>(A, b, x);

  // Verify
  real_t residual = ComputeResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

// ============================================================================
// Options Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, ForcePartialPivLUMethod) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // SPD matrix
  A << 4.0, 1.0, 1.0,
      1.0, 4.0, 1.0,
      1.0, 1.0, 4.0;
  b << 6.0, 6.0, 6.0;

  // Force PartialPivLU method
  LinearSolverOptions opts;
  opts.method = SolverMethod::kPartialPivLU;

  LinearSolver<real_t> solver(opts);
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  EXPECT_EQ(solver.GetMethod(), SolverMethod::kPartialPivLU);

  // Verify
  real_t residual = ComputeResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, ClearAndReuse) {
  const int N = 3;
  DMatrix<real_t> A1(this->kMemType, N, N);
  DMatrix<real_t> A2(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // First matrix
  A1 << 2.0, 1.0, 0.0,
       1.0, 3.0, 1.0,
       0.0, 1.0, 4.0;
  b << 3.0, 5.0, 5.0;

  LinearSolver<real_t> solver;
  solver.SetOperator(A1);
  solver.LinearSolve(b, x);

  real_t residual1 = ComputeResidual(A1, x, b);
  EXPECT_LT(residual1, kRealTolerance);

  // Clear and reuse with different matrix
  solver.Clear();
  EXPECT_FALSE(solver.IsReady());

  A2 << 5.0, 1.0, 1.0,
       1.0, 6.0, 1.0,
       1.0, 1.0, 7.0;
  b << 7.0, 8.0, 9.0;

  solver.SetOperator(A2);
  solver.LinearSolve(b, x);

  real_t residual2 = ComputeResidual(A2, x, b);
  EXPECT_LT(residual2, kRealTolerance);
}

// ============================================================================
// Float Type Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, FloatPrecision) {
  const int N = 3;
  DMatrix<float> A(this->kMemType, N, N);
  DVector<float> b(this->kMemType, N);
  DVector<float> x(this->kMemType, N);

  // Setup
  A << 3.0f, 1.0f, 1.0f,
      1.0f, 4.0f, 1.0f,
      1.0f, 1.0f, 5.0f;
  b << 5.0f, 6.0f, 7.0f;

  // Solve
  LinearSolver<float> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Verify (looser tolerance for float)
  const float* A_data = A.Read(false);
  const float* x_data = x.Read(false);
  const float* b_data = b.Read(false);

  float residual_norm = 0;
  float b_norm = 0;

  for (int i = 0; i < N; ++i) {
    float Ax_i = 0;
    for (int j = 0; j < N; ++j) {
      Ax_i += A_data[j * N + i] * x_data[j];
    }
    float r_i = Ax_i - b_data[i];
    residual_norm += r_i * r_i;
    b_norm += b_data[i] * b_data[i];
  }

  float rel_residual = std::sqrt(residual_norm) / std::sqrt(b_norm + kRealTolerance);
  EXPECT_LT(rel_residual, 1e-5f);
}

// ============================================================================
// Larger Matrix Tests
// ============================================================================

TYPED_TEST(LinearSolverTest, LargerMatrix10x10) {
  const int N = 10;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Create diagonally dominant matrix
  real_t* A_data = A.HostWrite();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      if (i == j) {
        A_data[j * N + i] = static_cast<real_t>(N + 1);  // Strong diagonal
      } else {
        A_data[j * N + i] = 1.0;  // Off-diagonal
      }
    }
  }

  // Known solution: x = [1, 2, 3, ..., N]
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    real_t sum = 0;
    for (int j = 0; j < N; ++j) {
      sum += A_data[j * N + i] * static_cast<real_t>(j + 1);
    }
    b_data[i] = sum;
  }

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Verify solution
  const real_t* x_data = x.Read(false);
  for (int i = 0; i < N; ++i) {
    EXPECT_NEAR(x_data[i], static_cast<real_t>(i + 1), kRealTolerance);
  }
}

// ============================================================================
// Rectangular Matrix Tests (Least Squares)
// ============================================================================

TYPED_TEST(LinearSolverTest, RectangularOverdetermined) {
  const int M = 4;  // Rows
  const int N = 2;  // Columns (M > N: overdetermined)
  DMatrix<real_t> A(this->kMemType, M, N);
  DVector<real_t> b(this->kMemType, M);
  DVector<real_t> x(this->kMemType, N);

  // Create overdetermined system (column-major)
  // A = [1 1; 1 2; 1 3; 1 4]
  A << 1.0, 1.0, 1.0, 1.0,  // Column 0
      1.0, 2.0, 3.0, 4.0;   // Column 1

  // b = [2, 4, 5, 4]
  b << 2.0, 4.0, 5.0, 4.0;

  // Solve (least squares)
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should use QR for rectangular
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kColPivHouseholderQR);
  EXPECT_FALSE(solver.IsSquare());

  // Solution should minimize ||Ax - b||
  // Just verify it produces a reasonable result
  EXPECT_EQ(x.GetSize(), N);
}

// ============================================================================
// Sparse Matrix Tests
// ============================================================================

/// @brief Helper to create tridiagonal sparse SPD matrix
SpDMatrix<real_t> CreateTridiagonalSPD(int N) {
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> coo(DShape<2>(N, N));

  // Diagonal: 4.0
  for (int i = 0; i < N; ++i) {
    coo.Insert(i, i, 4.0);
  }
  // Sub-diagonal and super-diagonal: -1.0
  for (int i = 0; i < N - 1; ++i) {
    coo.Insert(i, i + 1, -1.0);
    coo.Insert(i + 1, i, -1.0);
  }

  coo.Finalize();
  return coo.ToLayout<DefaultSparseLayout>();
}

/// @brief Helper to compute sparse residual
real_t ComputeSparseResidual(const SpDMatrix<real_t>& A, const DVector<real_t>& x,
                              const DVector<real_t>& b) {
  const int N = A.GetExtent(0);
  const real_t* x_data = x.HostRead();
  const real_t* b_data = b.HostRead();

  real_t residual_norm = 0;
  real_t b_norm = 0;

  for (int i = 0; i < N; ++i) {
    real_t Ax_i = 0;
    for (int j = 0; j < N; ++j) {
      Ax_i += A.At(i, j) * x_data[j];
    }
    real_t r_i = Ax_i - b_data[i];
    residual_norm += r_i * r_i;
    b_norm += b_data[i] * b_data[i];
  }

  return std::sqrt(residual_norm) / std::sqrt(b_norm + 1e-30);
}

/// @brief Helper to create sparse diagonal matrix
SpDMatrix<real_t> CreateSparseDiagonal(int N) {
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> coo(DShape<2>(N, N));

  for (int i = 0; i < N; ++i) {
    coo.Insert(i, i, static_cast<real_t>(i + 2));
  }

  coo.Finalize();
  return coo.ToLayout<DefaultSparseLayout>();
}

/// @brief Helper to create sparse lower triangular matrix
SpDMatrix<real_t> CreateSparseLowerTriangular(int N) {
  SparseMArray<real_t, DShape<2>, SparseLayoutStride> coo(DShape<2>(N, N));

  // Diagonal
  for (int i = 0; i < N; ++i) {
    coo.Insert(i, i, static_cast<real_t>(i + 2));
  }
  // Sub-diagonal
  for (int i = 1; i < N; ++i) {
    coo.Insert(i, i - 1, 1.0);
  }

  coo.Finalize();
  return coo.ToLayout<DefaultSparseLayout>();
}

TYPED_TEST(LinearSolverTest, SparseDiagonalMatrix) {
  const int N = 10;
  SpDMatrix<real_t> A = CreateSparseDiagonal(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // b = [2, 6, 12, 20, ...]
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = static_cast<real_t>((i + 1) * (i + 2));
  }

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as sparse diagonal
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kSparseDiagonal);
  EXPECT_TRUE(solver.IsSparse());

  // Expected: x[i] = (i+1)
  const real_t* x_data = x.Read(false);
  for (int i = 0; i < N; ++i) {
    EXPECT_NEAR(x_data[i], static_cast<real_t>(i + 1), kRealTolerance);
  }
}

TYPED_TEST(LinearSolverTest, SparseLowerTriangularMatrix) {
  const int N = 10;
  SpDMatrix<real_t> A = CreateSparseLowerTriangular(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // RHS
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = static_cast<real_t>(i + 1);
  }

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  // Should detect as sparse triangular
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kSparseTriangular);

  // Verify residual
  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, SparseLUBasic) {
  const int N = 10;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // RHS: all ones
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = 1.0;
  }

  // Solve with SparseLU
  LinearSolverOptions opts;
  opts.method = SolverMethod::kSparseLU;

  LinearSolver<real_t> solver(opts);
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  EXPECT_TRUE(solver.IsReady());
  EXPECT_TRUE(solver.IsSparse());
  EXPECT_EQ(solver.GetMethod(), SolverMethod::kSparseLU);

  // Verify residual
  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, SparseSimplicialLDLTBasic) {
  const int N = 10;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // RHS
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = static_cast<real_t>(i + 1);
  }

  // Solve with SimplicialLDLT
  LinearSolverOptions opts;
  opts.method = SolverMethod::kSimplicialLDLT;

  LinearSolver<real_t> solver(opts);
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  EXPECT_EQ(solver.GetMethod(), SolverMethod::kSimplicialLDLT);

  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, SparseCGBasic) {
  const int N = 20;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // RHS
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = 1.0;
  }

  // Solve with CG
  LinearSolverOptions opts;
  opts.method = SolverMethod::kCG;
  opts.iterative_tol = kRealTolerance;

  LinearSolver<real_t> solver(opts);
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  EXPECT_EQ(solver.GetMethod(), SolverMethod::kCG);

  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kLooseRealTolerance);  // Iterative solver tolerance
}

TYPED_TEST(LinearSolverTest, SparseBiCGSTABBasic) {
  const int N = 20;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // RHS
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = static_cast<real_t>(i + 1);
  }

  // Solve with BiCGSTAB
  LinearSolverOptions opts;
  opts.method = SolverMethod::kBiCGSTAB;
  opts.iterative_tol = kRealTolerance;

  LinearSolver<real_t> solver(opts);
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  EXPECT_EQ(solver.GetMethod(), SolverMethod::kBiCGSTAB);

  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kLooseRealTolerance);
}

TYPED_TEST(LinearSolverTest, SparseAutoSelectDirect) {
  const int N = 10;  // Small, should select direct method
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = 1.0;
  }

  // Auto select (should pick direct method for small matrix)
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  EXPECT_TRUE(solver.IsSparse());
  // Should select SimplicialLLT or SimplicialLDLT for SPD matrix
  SolverMethod method = solver.GetMethod();
  EXPECT_TRUE(method == SolverMethod::kSimplicialLLT ||
              method == SolverMethod::kSimplicialLDLT ||
              method == SolverMethod::kSparseLU);

  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, SparseConvenienceFunction) {
  const int N = 10;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(N);
  DVector<real_t> x(N);

  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = 2.0;
  }

  // Use convenience function
  LinearSolve(A, b, x);

  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

TYPED_TEST(LinearSolverTest, SparseRepeatedSolves) {
  const int N = 15;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);

  // Setup solver once
  LinearSolver<real_t> solver;
  solver.SetOperator(A);

  // Solve with multiple RHS
  for (int k = 0; k < 5; ++k) {
    DVector<real_t> b(this->kMemType, N);
    DVector<real_t> x(this->kMemType, N);

    real_t* b_data = b.HostWrite();
    for (int i = 0; i < N; ++i) {
      b_data[i] = static_cast<real_t>((k + 1) * (i + 1));
    }

    solver.LinearSolve(b, x);

    real_t residual = ComputeSparseResidual(A, x, b);
    EXPECT_LT(residual, kRealTolerance);
  }
}

TYPED_TEST(LinearSolverTest, SparseLargeMatrix) {
  const int N = 100;
  SpDMatrix<real_t> A = CreateTridiagonalSPD(N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // RHS
  real_t* b_data = b.HostWrite();
  for (int i = 0; i < N; ++i) {
    b_data[i] = 1.0;
  }

  // Solve
  LinearSolver<real_t> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);

  real_t residual = ComputeSparseResidual(A, x, b);
  EXPECT_LT(residual, kRealTolerance);
}

#endif  // ASC_USE_EIGEN

TYPED_TEST(LinearSolverTest, LapackLuFactorSolveWrappers) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  A << 2.0, 4.0, 8.0, 1.0, 3.0, 7.0, 1.0, 3.0, 9.0;
  b << 4.0, 10.0, 24.0;

  DMatrix<real_t> lu = A;
  DVector<int> ipiv(this->kMemType);
  EXPECT_EQ(Getrf(lu, ipiv), 0);

  DVector<real_t> x(this->kMemType);
  EXPECT_EQ(Getrs(lu, ipiv, b, x), 0);
  EXPECT_LT(ComputeResidual(A, x, b), kRealTolerance);

  DMatrix<real_t> invA(this->kMemType);
  EXPECT_EQ(Getri(lu, ipiv, invA), 0);
  const auto& a_map = A.GetMap();
  const auto& inv_map = invA.GetMap();
  const real_t* a_data = A.Read(false);
  const real_t* inv_data = invA.Read(false);
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      real_t value = 0.0_r;
      for (int k = 0; k < N; ++k) {
        value += a_data[a_map(i, k)] * inv_data[inv_map(k, j)];
      }
      EXPECT_NEAR(value, i == j ? 1.0 : 0.0, kRealTolerance);
    }
  }

  DVector<int> ipiv2(this->kMemType);
  DVector<real_t> x2(this->kMemType);
  EXPECT_EQ(Gesv(A, b, x2, ipiv2), 0);
  EXPECT_LT(ComputeResidual(A, x2, b), kRealTolerance);
}

TYPED_TEST(LinearSolverTest, LapackCholeskyWrappers) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  A << 4.0, 1.0, 1.0, 1.0, 4.0, 1.0, 1.0, 1.0, 4.0;
  b << 6.0, 6.0, 6.0;

  DMatrix<real_t> L(this->kMemType);
  EXPECT_EQ(Potrf(A, L), 0);

  DVector<real_t> x(this->kMemType);
  EXPECT_EQ(Potrs(L, b, x), 0);
  const real_t* x_data = x.Read(false);
  EXPECT_NEAR(x_data[0], 1.0, kRealTolerance);
  EXPECT_NEAR(x_data[1], 1.0, kRealTolerance);
  EXPECT_NEAR(x_data[2], 1.0, kRealTolerance);

  DMatrix<real_t> invA(this->kMemType);
  EXPECT_EQ(Potri(L, invA), 0);
  const auto& a_map = A.GetMap();
  const auto& inv_map = invA.GetMap();
  const real_t* a_data = A.Read(false);
  const real_t* inv_data = invA.Read(false);
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      real_t value = 0.0_r;
      for (int k = 0; k < N; ++k) {
        value += a_data[a_map(i, k)] * inv_data[inv_map(k, j)];
      }
      EXPECT_NEAR(value, i == j ? 1.0 : 0.0, kRealTolerance);
    }
  }

  DMatrix<real_t> L2(this->kMemType);
  DVector<real_t> x2(this->kMemType);
  EXPECT_EQ(Posv(A, b, x2, L2), 0);
  EXPECT_LT(ComputeResidual(A, x2, b), kRealTolerance);
}

TYPED_TEST(LinearSolverTest, LapackQrLeastSquaresWrapper) {
  DMatrix<real_t> A(this->kMemType, 3, 2);
  DVector<real_t> b(this->kMemType, 3);
  A << 1.0, 1.0, 1.0, 1.0, 2.0, 3.0;
  b << 3.0, 4.0, 5.0;

  DMatrix<real_t> Q(this->kMemType);
  DMatrix<real_t> R(this->kMemType);
  EXPECT_EQ(Geqrf(A, Q, R), 0);
  EXPECT_EQ(Q.GetExtent(0), 3);
  EXPECT_EQ(Q.GetExtent(1), 3);
  EXPECT_EQ(R.GetExtent(0), 3);
  EXPECT_EQ(R.GetExtent(1), 2);

  DVector<real_t> x(this->kMemType);
  EXPECT_EQ(Gels(A, b, x), 0);
  const real_t* x_data = x.Read(false);
  EXPECT_NEAR(x_data[0], 2.0, kRealTolerance);
  EXPECT_NEAR(x_data[1], 1.0, kRealTolerance);
}

#ifdef ASC_USE_EIGEN

TYPED_TEST(LinearSolverTest, LapackEigenBackedWrappers) {
  DMatrix<real_t> A(this->kMemType, 2, 2);
  A << 2.0, 1.0, 1.0, 2.0;

  DVector<real_t> values(this->kMemType);
  DMatrix<real_t> vectors(this->kMemType);
  EXPECT_EQ(Syev(A, values, vectors), 0);
  const real_t* value_data = values.Read(false);
  EXPECT_NEAR(value_data[0], 1.0, kRealTolerance);
  EXPECT_NEAR(value_data[1], 3.0, kRealTolerance);

  DMatrix<real_t> U(this->kMemType);
  DVector<real_t> s(this->kMemType);
  DMatrix<real_t> Vt(this->kMemType);
  EXPECT_EQ(Gesvd(A, U, s, Vt), 0);
  const real_t* s_data = s.Read(false);
  EXPECT_NEAR(s_data[0], 3.0, kRealTolerance);
  EXPECT_NEAR(s_data[1], 1.0, kRealTolerance);
}

#endif  // ASC_USE_EIGEN

TEST(LapackUtilityTest, SymmetricEigenDecompose) {
  DMatrix<real_t> A(2, 2);
  A << 2.0_r, 1.0_r, 1.0_r, 2.0_r;

  SymmetricEigenResult<real_t> eig = SymmetricEigenDecompose(A);

#ifdef ASC_USE_DOUBLE
  const real_t tol = static_cast<real_t>(kRealTolerance);
#else
  const real_t tol = 1e-5_r;
#endif
  const real_t* values = eig.values.HostRead();
  const real_t* vectors = eig.vectors.HostRead();
  const auto& v_map = eig.vectors.GetMap();
  EXPECT_NEAR(values[0], 1.0_r, tol);
  EXPECT_NEAR(values[1], 3.0_r, tol);

  const auto& a_map = A.GetMap();
  const real_t* a_data = A.HostRead();
  for (int col = 0; col < 2; ++col) {
    for (int row = 0; row < 2; ++row) {
      real_t av = 0.0_r;
      for (int k = 0; k < 2; ++k) {
        av += a_data[a_map(row, k)] * vectors[v_map(k, col)];
      }
      EXPECT_NEAR(av, values[col] * vectors[v_map(row, col)], tol);
    }
  }
}

}  // namespace asc
