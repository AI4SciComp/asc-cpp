// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: tests/algebra/unit_test_decomp.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include "common/common.h"
#include "asc/linalg/blas.h"
#include "asc/linalg/decomp.h"

namespace asc {

// ============================================================================
// Helper Functions for LUP Testing
// ============================================================================

/// @brief Check if matrix is lower triangular with unit diagonal
template <typename T>
bool IsLowerTriangularUnitDiag(const DMatrix<T>& L) {
  const int N = L.GetExtent(0);
  const T eps = static_cast<T>(kRealTolerance);
  const T* data = L.Read(false);

  // Column-major: L(i,j) = data[j*N + i]
  for (int i = 0; i < N; i++) {
    // Check diagonal is 1
    if (std::abs(data[i * N + i] - static_cast<T>(1)) > eps) {
      return false;
    }
    // Check upper triangle is zero (i < j)
    for (int j = i + 1; j < N; j++) {
      if (std::abs(data[j * N + i]) > eps) {  // L(i,j)
        return false;
      }
    }
  }
  return true;
}

/// @brief Check if matrix is upper triangular
template <typename T>
bool IsUpperTriangular(const DMatrix<T>& U) {
  const int N = U.GetExtent(0);
  const T eps = static_cast<T>(kRealTolerance);
  const T* data = U.Read(false);

  // Column-major: U(i,j) = data[j*N + i]
  for (int i = 0; i < N; i++) {
    // Check lower triangle is zero (i > j)
    for (int j = 0; j < i; j++) {
      if (std::abs(data[j * N + i]) > eps) {  // U(i,j)
        return false;
      }
    }
  }
  return true;
}

/// @brief Apply permutation to matrix rows: PA
template <typename T>
void ApplyPermutation(const DMatrix<T>& A, const DVector<int>& P,
                      DMatrix<T>& PA) {
  const int N = A.GetExtent(0);
  const T* A_data = A.Read(false);
  const int* P_data = P.Read(false);
  T* PA_data = PA.HostWrite();

  // Column-major: PA(i,j) = A(P[i], j) = A_data[j*N + P[i]]
  for (int j = 0; j < N; j++) {
    for (int i = 0; i < N; i++) {
      PA_data[j * N + i] = A_data[j * N + P_data[i]];
    }
  }
}

/// @brief Matrix multiplication: C = A * B
template <typename T>
void MatMul(const DMatrix<T>& A, const DMatrix<T>& B, DMatrix<T>& C) {
  const int N = A.GetExtent(0);
  const T* A_data = A.Read(false);
  const T* B_data = B.Read(false);
  T* C_data = C.HostWrite();

  // Column-major: C(i,j) = sum_k A(i,k) * B(k,j)
  for (int j = 0; j < N; j++) {
    for (int i = 0; i < N; i++) {
      T sum = 0;
      for (int k = 0; k < N; k++) {
        sum += A_data[k * N + i] * B_data[j * N + k];  // A(i,k) * B(k,j)
      }
      C_data[j * N + i] = sum;  // C(i,j)
    }
  }
}

/// @brief Check if two matrices are approximately equal
template <typename T, Arithmetic Tol>
bool MatrixEquals(const DMatrix<T>& A, const DMatrix<T>& B,
                  Tol requested_tol) {
  if (A.GetExtent(0) != B.GetExtent(0) || A.GetExtent(1) != B.GetExtent(1)) {
    return false;
  }

  const int rows = A.GetExtent(0);
  const int cols = A.GetExtent(1);
  const T* A_data = A.Read(false);
  const T* B_data = B.Read(false);
  const T numeric_tol =
      static_cast<T>(100) * std::numeric_limits<T>::epsilon();
  const T tol = std::max(static_cast<T>(requested_tol), numeric_tol);

  for (int i = 0; i < rows * cols; i++) {
    if (std::abs(A_data[i] - B_data[i]) > tol) {
      return false;
    }
  }
  return true;
}

// ============================================================================
// Typed Test Suite Definition
// ============================================================================

/// @brief Test fixture for LUP decomposition tests
template <typename MemType>
using LUPTest = BaseTest<MemType>;

TYPED_TEST_SUITE(LUPTest, AllMemoryTypes);

// ============================================================================
// Test Cases - Out-of-place LUP Decomposition
// ============================================================================

TYPED_TEST(LUPTest, OutOfPlaceDecomposition3x3real_t) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  DMatrix<real_t> U(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize using column-major array
  A << 2.0, 4.0, 8.0,  // Column 0
      1.0, 3.0, 7.0,   // Column 1
      1.0, 3.0, 9.0;   // Column 2

  DMatrix<real_t> A_copy = A;

  // Perform LUP decomposition
  LUP(A, L, U, P);

  // Verify L is lower triangular with unit diagonal
  EXPECT_TRUE(IsLowerTriangularUnitDiag(L));

  // Verify U is upper triangular
  EXPECT_TRUE(IsUpperTriangular(U));

  // Verify PA = LU
  DMatrix<real_t> PA(this->kMemType, N, N);
  ApplyPermutation(A_copy, P, PA);

  DMatrix<real_t> LU(this->kMemType, N, N);
  MatMul(L, U, LU);

  EXPECT_TRUE(MatrixEquals(PA, LU, kRealTolerance));
}

TYPED_TEST(LUPTest, OutOfPlaceDecomposition3x3Float) {
  const int N = 3;
  DMatrix<float> A(this->kMemType, N, N);
  DMatrix<float> L(this->kMemType, N, N);
  DMatrix<float> U(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize test matrix
  A << 3.0f, 6.0f, 9.0f,  // Column 0
      2.0f, 5.0f, 8.0f,   // Column 1
      1.0f, 4.0f, 7.0f;   // Column 2

  DMatrix<float> A_copy = A;

  // Perform LUP decomposition
  LUP(A, L, U, P);

  // Verify L is lower triangular with unit diagonal
  EXPECT_TRUE(IsLowerTriangularUnitDiag(L));

  // Verify U is upper triangular
  EXPECT_TRUE(IsUpperTriangular(U));

  // Verify PA = LU
  DMatrix<float> PA(this->kMemType, N, N);
  ApplyPermutation(A_copy, P, PA);

  DMatrix<float> LU(this->kMemType, N, N);
  MatMul(L, U, LU);

  EXPECT_TRUE(MatrixEquals(PA, LU, 1e-5f));
}

TYPED_TEST(LUPTest, OutOfPlaceDecomposition5x5) {
  const int N = 5;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  DMatrix<real_t> U(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize random-like test matrix (column-major)
  A << 5.0, 1.0, 3.0, 4.0, 2.0,  // Column 0
      2.0, 6.0, 1.0, 3.0, 4.0,   // Column 1
      1.0, 2.0, 7.0, 2.0, 3.0,   // Column 2
      3.0, 4.0, 5.0, 8.0, 1.0,   // Column 3
      4.0, 5.0, 6.0, 7.0, 9.0;   // Column 4

  DMatrix<real_t> A_copy = A;

  // Perform LUP decomposition
  LUP(A, L, U, P);

  // Verify L is lower triangular with unit diagonal
  EXPECT_TRUE(IsLowerTriangularUnitDiag(L));

  // Verify U is upper triangular
  EXPECT_TRUE(IsUpperTriangular(U));

  // Verify PA = LU
  DMatrix<real_t> PA(this->kMemType, N, N);
  ApplyPermutation(A_copy, P, PA);

  DMatrix<real_t> LU(this->kMemType, N, N);
  MatMul(L, U, LU);

  EXPECT_TRUE(MatrixEquals(PA, LU, kRealTolerance));
}

// ============================================================================
// Test Cases - In-place LUP Decomposition
// ============================================================================

TYPED_TEST(LUPTest, InPlaceDecomposition3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize test matrix
  A << 2.0, 4.0, 8.0,  // Column 0
      1.0, 3.0, 7.0,   // Column 1
      1.0, 3.0, 9.0;   // Column 2

  DMatrix<real_t> A_copy = A;

  // Perform in-place LUP decomposition
  bool success = LUP(A, P);

  EXPECT_TRUE(success);

  // Extract L and U from A
  DMatrix<real_t> L(this->kMemType, N, N);
  DMatrix<real_t> U(this->kMemType, N, N);

  real_t* L_data = L.HostWrite();
  real_t* U_data = U.HostWrite();
  const real_t* A_data = A.Read(false);

  // Column-major extraction
  for (int j = 0; j < N; j++) {
    for (int i = 0; i < N; i++) {
      const int idx = j * N + i;
      if (i > j) {
        L_data[idx] = A_data[idx];
        U_data[idx] = 0.0;
      } else if (i == j) {
        L_data[idx] = 1.0;
        U_data[idx] = A_data[idx];
      } else {
        L_data[idx] = 0.0;
        U_data[idx] = A_data[idx];
      }
    }
  }

  // Verify PA = LU
  DMatrix<real_t> PA(this->kMemType, N, N);
  ApplyPermutation(A_copy, P, PA);

  DMatrix<real_t> LU(this->kMemType, N, N);
  MatMul(L, U, LU);

  EXPECT_TRUE(MatrixEquals(PA, LU, kRealTolerance));
}

TYPED_TEST(LUPTest, InPlaceDecompositionSingularMatrix) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize singular matrix (row 2 = 2 * row 1)
  A << 1.0, 2.0, 4.0,  // Column 0
      2.0, 4.0, 5.0,   // Column 1
      3.0, 6.0, 6.0;   // Column 2

  // Should return false for singular matrix
  bool success = LUP(A, P);

  EXPECT_FALSE(success);
}

// ============================================================================
// Test Cases - LUP Solve with Separate L and U
// ============================================================================

TYPED_TEST(LUPTest, SolveWithSeparateLU3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  DMatrix<real_t> U(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Initialize test system
  A << 2.0, 4.0, 8.0,  // Column 0
      1.0, 3.0, 7.0,   // Column 1
      1.0, 3.0, 9.0;   // Column 2

  b << 4.0, 10.0, 24.0;

  DMatrix<real_t> A_copy = A;

  // Decompose
  LUP(A, L, U, P);

  // Solve
  LUPSolve(L, U, P, b, x);

  // Verify Ax = b
  DVector<real_t> Ax(this->kMemType, N);
  const real_t* A_data = A_copy.Read(false);
  const real_t* x_data = x.Read(false);
  real_t* Ax_data = Ax.HostWrite();

  // Matrix-vector multiply (column-major)
  for (int i = 0; i < N; i++) {
    Ax_data[i] = 0.0;
    for (int j = 0; j < N; j++) {
      Ax_data[i] += A_data[j * N + i] * x_data[j];  // A(i,j) * x[j]
    }
  }

  const real_t* b_data = b.Read(false);
  const real_t* Ax_read = Ax.Read(false);
  for (int i = 0; i < N; i++) {
    EXPECT_NEAR(Ax_read[i], b_data[i], kRealTolerance);
  }
}

// ============================================================================
// Test Cases - LUP Solve with In-place Decomposition
// ============================================================================

TYPED_TEST(LUPTest, SolveInPlace3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  // Initialize test system
  A << 2.0, 4.0, 8.0,  // Column 0
      1.0, 3.0, 7.0,   // Column 1
      1.0, 3.0, 9.0;   // Column 2

  b << 4.0, 10.0, 24.0;

  DMatrix<real_t> A_copy = A;

  // Decompose in-place
  bool success = LUP(A, P);
  EXPECT_TRUE(success);

  // Solve
  LUPSolve(A, P, b, x);

  // Verify Ax = b
  DVector<real_t> Ax(this->kMemType, N);
  const real_t* A_data = A_copy.Read(false);
  const real_t* x_data = x.Read(false);
  real_t* Ax_data = Ax.HostWrite();

  // Matrix-vector multiply (column-major)
  for (int i = 0; i < N; i++) {
    Ax_data[i] = 0.0;
    for (int j = 0; j < N; j++) {
      Ax_data[i] += A_data[j * N + i] * x_data[j];  // A(i,j) * x[j]
    }
  }

  const real_t* b_data = b.Read(false);
  const real_t* Ax_read = Ax.Read(false);
  for (int i = 0; i < N; i++) {
    EXPECT_NEAR(Ax_read[i], b_data[i], kRealTolerance);
  }
}

// ============================================================================
// Test Cases - Edge Cases
// ============================================================================

TYPED_TEST(LUPTest, IdentityMatrix) {
  const int N = 4;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  DMatrix<real_t> U(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize identity matrix (column-major)
  A << 1.0, 0.0, 0.0, 0.0,  // Column 0
      0.0, 1.0, 0.0, 0.0,   // Column 1
      0.0, 0.0, 1.0, 0.0,   // Column 2
      0.0, 0.0, 0.0, 1.0;   // Column 3

  // Perform LUP decomposition
  LUP(A, L, U, P);

  // For identity matrix, L should be identity and U should be identity
  EXPECT_TRUE(IsLowerTriangularUnitDiag(L));
  EXPECT_TRUE(IsUpperTriangular(U));

  // U should be identity (diagonal elements = 1)
  const real_t* U_data = U.Read(false);
  for (int i = 0; i < N; i++) {
    EXPECT_NEAR(U_data[i * N + i], 1.0, kRealTolerance);
  }
}

TYPED_TEST(LUPTest, DiagonalMatrix) {
  const int N = 4;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  DMatrix<real_t> U(this->kMemType, N, N);
  DVector<int> P(this->kMemType, N);

  // Initialize diagonal matrix (column-major)
  A << 2.0, 0.0, 0.0, 0.0,  // Column 0
      0.0, 3.0, 0.0, 0.0,   // Column 1
      0.0, 0.0, 4.0, 0.0,   // Column 2
      0.0, 0.0, 0.0, 5.0;   // Column 3

  DMatrix<real_t> A_copy = A;

  // Perform LUP decomposition
  LUP(A, L, U, P);

  // Verify decomposition
  EXPECT_TRUE(IsLowerTriangularUnitDiag(L));
  EXPECT_TRUE(IsUpperTriangular(U));

  // Verify PA = LU
  DMatrix<real_t> PA(this->kMemType, N, N);
  ApplyPermutation(A_copy, P, PA);

  DMatrix<real_t> LU(this->kMemType, N, N);
  MatMul(L, U, LU);

  EXPECT_TRUE(MatrixEquals(PA, LU, kRealTolerance));
}

// ============================================================================
// Cholesky Tests
// ============================================================================

template <typename MemType>
using CholeskyTest = BaseTest<MemType>;
TYPED_TEST_SUITE(CholeskyTest, AllMemoryTypes);

TYPED_TEST(CholeskyTest, SPD3x3) {
  // A = L * L^T where L = [[2,0,0],[3,1,0],[1,2,4]]
  // A = [[4,6,2],[6,10,5],[2,5,21]]
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);

  // Column-major initialisation (column 0, column 1, column 2)
  A << 4.0, 6.0, 2.0,
       6.0, 10.0, 5.0,
       2.0, 5.0, 21.0;

  EXPECT_TRUE(Cholesky(A, L));

  // L must be lower triangular
  const real_t* L_data = L.HostRead();
  const int Ns = N;
  for (int i = 0; i < Ns; ++i)
    for (int j = i + 1; j < Ns; ++j)
      EXPECT_NEAR(L_data[j * N + i], 0.0, kRealTolerance);  // upper triangle zero

  // Reconstruct: L * L^T == A
  DMatrix<real_t> LLT(this->kMemType, N, N);
  DMatrix<real_t> Lt(this->kMemType, N, N);
  real_t* Lt_data = Lt.HostWrite();
  for (int j = 0; j < N; ++j)
    for (int i = 0; i < N; ++i)
      Lt_data[i * N + j] = L_data[j * N + i];  // Lt = L^T (column-major)
  MatMul(L, Lt, LLT);
  EXPECT_TRUE(MatrixEquals(A, LLT, kRealTolerance));
}

TYPED_TEST(CholeskyTest, NotSPD) {
  const int N = 2;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  A << 1.0, 2.0, 2.0, 1.0;  // not positive definite (det < 0)
  EXPECT_FALSE(Cholesky(A, L));
}

TYPED_TEST(CholeskyTest, Solve3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> L(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  A << 4.0, 6.0, 2.0,
       6.0, 10.0, 5.0,
       2.0, 5.0, 21.0;
  b << 12.0, 21.0, 28.0;

  ASSERT_TRUE(Cholesky(A, L));
  CholeskySolve(L, b, x);

  // Verify A * x == b  (use original A values)
  DMatrix<real_t> A2(this->kMemType, N, N);
  A2 << 4.0, 6.0, 2.0,
        6.0, 10.0, 5.0,
        2.0, 5.0, 21.0;
  DVector<real_t> Ax(this->kMemType, N);
  asc::MatMul(A2, x, Ax);
  const real_t* Ax_data = Ax.HostRead();
  const real_t* b_data  = b.HostRead();
  for (int i = 0; i < N; ++i)
    EXPECT_NEAR(Ax_data[i], b_data[i], kLooseRealTolerance);
}

// ============================================================================
// QR Tests
// ============================================================================

template <typename MemType>
using QRTest = BaseTest<MemType>;
TYPED_TEST_SUITE(QRTest, AllMemoryTypes);

TYPED_TEST(QRTest, Square3x3Reconstruction) {
  const int M = 3, N = 3;
  DMatrix<real_t> A(this->kMemType, M, N);
  DMatrix<real_t> Q(this->kMemType, M, M);
  DMatrix<real_t> R(this->kMemType, M, N);

  A << 12.0, -51.0, 4.0,
       6.0, 167.0, -68.0,
      -4.0, 24.0, -41.0;
  DMatrix<real_t> A_orig = A;

  QR(A, Q, R);

  // R must be upper triangular
  const real_t* R_data = R.HostRead();
  for (int i = 0; i < M; ++i)
    for (int j = 0; j < i && j < N; ++j)
      EXPECT_NEAR(R_data[j * M + i], 0.0, kRealTolerance);

  // Q * R must equal A_orig
  DMatrix<real_t> QR_prod(this->kMemType, M, N);
  MatMul(Q, R, QR_prod);
  EXPECT_TRUE(MatrixEquals(A_orig, QR_prod, kLooseRealTolerance));
}

TYPED_TEST(QRTest, QOrthogonal) {
  const int M = 4, N = 3;
  DMatrix<real_t> A(this->kMemType, M, N);
  DMatrix<real_t> Q(this->kMemType, M, M);
  DMatrix<real_t> R(this->kMemType, M, N);

  // Tall matrix
  A << 1.0, 2.0, 3.0, 4.0,
       5.0, 6.0, 7.0, 8.0,
       9.0, 10.0, 11.0, 12.0;
  QR(A, Q, R);

  // Q must be orthogonal: Q^T * Q == I_M
  DMatrix<real_t> Qt(this->kMemType, M, M);
  const real_t* Q_data = Q.HostRead();
  real_t* Qt_data = Qt.HostWrite();
  for (int j = 0; j < M; ++j)
    for (int i = 0; i < M; ++i)
      Qt_data[i * M + j] = Q_data[j * M + i];

  DMatrix<real_t> QtQ(this->kMemType, M, M);
  MatMul(Qt, Q, QtQ);
  const real_t* QtQ_data = QtQ.HostRead();
  for (int j = 0; j < M; ++j)
    for (int i = 0; i < M; ++i) {
      real_t expected = (i == j) ? 1.0 : 0.0;
      EXPECT_NEAR(QtQ_data[j * M + i], expected, kRealTolerance);
    }
}

TYPED_TEST(QRTest, Solve3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> Q(this->kMemType, N, N);
  DMatrix<real_t> R(this->kMemType, N, N);
  DVector<real_t> b(this->kMemType, N);
  DVector<real_t> x(this->kMemType, N);

  A << 2.0, 1.0, 1.0,
       4.0, 3.0, 3.0,
       8.0, 7.0, 9.0;
  b << 1.0, 1.0, 1.0;
  DMatrix<real_t> A_orig = A;

  QR(A, Q, R);
  QRSolve(Q, R, b, x);

  // Verify A * x == b
  DVector<real_t> Ax(this->kMemType, N);
  asc::MatMul(A_orig, x, Ax);
  const real_t* Ax_data = Ax.HostRead();
  const real_t* b_data  = b.HostRead();
  for (int i = 0; i < N; ++i)
    EXPECT_NEAR(Ax_data[i], b_data[i], kLooseRealTolerance);
}

#ifdef ASC_USE_EIGEN

// ============================================================================
// SVD Tests
// ============================================================================

template <typename MemType>
using SVDTest = BaseTest<MemType>;
TYPED_TEST_SUITE(SVDTest, AllMemoryTypes);

TYPED_TEST(SVDTest, Reconstruction3x3) {
  const int M = 3, N = 3, K = 3;
  DMatrix<real_t> A(this->kMemType, M, N);
  DMatrix<real_t> U(this->kMemType, M, K);
  DVector<real_t> s(this->kMemType, K);
  DMatrix<real_t> Vt(this->kMemType, K, N);

  A << 1.0, 0.0, 0.0,
       0.0, 2.0, 0.0,
       0.0, 0.0, 3.0;  // diagonal → singular values are 3, 2, 1

  SVD(A, U, s, Vt);

  // Singular values must be non-negative and descending
  const real_t* s_data = s.HostRead();
  for (int i = 0; i < K - 1; ++i)
    EXPECT_GE(s_data[i], s_data[i + 1] - kRealTolerance);

  // Reconstruct: U * diag(s) * Vt == A
  DMatrix<real_t> US(this->kMemType, M, K);
  real_t* US_data = US.HostWrite();
  const real_t* U_data  = U.HostRead();
  for (int j = 0; j < K; ++j)
    for (int i = 0; i < M; ++i)
      US_data[j * M + i] = U_data[j * M + i] * s_data[j];

  DMatrix<real_t> A_rec(this->kMemType, M, N);
  MatMul(US, Vt, A_rec);
  EXPECT_TRUE(MatrixEquals(A, A_rec, kLooseRealTolerance));
}

TYPED_TEST(SVDTest, TruncRank1) {
  const int M = 3, N = 3, k = 1;
  DMatrix<real_t> A(this->kMemType, M, N);
  DMatrix<real_t> U(this->kMemType, M, k);
  DVector<real_t> s(this->kMemType, k);
  DMatrix<real_t> Vt(this->kMemType, k, N);

  A << 1.0, 0.0, 0.0,
       0.0, 2.0, 0.0,
       0.0, 0.0, 3.0;

  TruncSVD(A, k, U, s, Vt);

  const real_t* s_data = s.HostRead();
  EXPECT_NEAR(s_data[0], 3.0, kLooseRealTolerance);  // largest singular value
}

// ============================================================================
// Eigenvalue Tests
// ============================================================================

template <typename MemType>
using EigenTest = BaseTest<MemType>;
TYPED_TEST_SUITE(EigenTest, AllMemoryTypes);

TYPED_TEST(EigenTest, EigH_3x3) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> vals(this->kMemType, N);
  DMatrix<real_t> vecs(this->kMemType, N, N);

  // Symmetric diagonal → eigenvalues are diagonal entries
  A << 3.0, 0.0, 0.0,
       0.0, 1.0, 0.0,
       0.0, 0.0, 2.0;

  EigH(A, vals, vecs);

  // Eigenvalues must be sorted ascending
  const real_t* v = vals.HostRead();
  EXPECT_NEAR(v[0], 1.0, kLooseRealTolerance);
  EXPECT_NEAR(v[1], 2.0, kLooseRealTolerance);
  EXPECT_NEAR(v[2], 3.0, kLooseRealTolerance);
}

TYPED_TEST(EigenTest, EigH_Reconstruction) {
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> vals(this->kMemType, N);
  DMatrix<real_t> vecs(this->kMemType, N, N);

  // Symmetric SPD matrix A = [[4,2,0],[2,3,0],[0,0,1]]
  A << 4.0, 2.0, 0.0,
       2.0, 3.0, 0.0,
       0.0, 0.0, 1.0;

  EigH(A, vals, vecs);

  // A * v_i == λ_i * v_i  for each eigenvector column
  const real_t* val_data  = vals.HostRead();
  const real_t* vecs_data = vecs.HostRead();
  DMatrix<real_t> A2(this->kMemType, N, N);
  A2 << 4.0, 2.0, 0.0,
        2.0, 3.0, 0.0,
        0.0, 0.0, 1.0;

  for (int col = 0; col < N; ++col) {
    DVector<real_t> ev(this->kMemType, N);
    real_t* ev_data = ev.HostWrite();
    for (int i = 0; i < N; ++i) ev_data[i] = vecs_data[col * N + i];
    DVector<real_t> Av(this->kMemType, N);
    asc::MatMul(A2, ev, Av);
    const real_t* Av_data = Av.HostRead();
    for (int i = 0; i < N; ++i)
      EXPECT_NEAR(Av_data[i], val_data[col] * ev_data[i], kLooseRealTolerance);
  }
}

TYPED_TEST(EigenTest, Eig_RealEigenvalues) {
  // Diagonal matrix → all eigenvalues real
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DVector<real_t> vals_re(this->kMemType, N);
  DVector<real_t> vals_im(this->kMemType, N);
  DMatrix<real_t> vecs_re(this->kMemType, N, N);
  DMatrix<real_t> vecs_im(this->kMemType, N, N);

  A << 5.0, 0.0, 0.0,
       0.0, 2.0, 0.0,
       0.0, 0.0, 4.0;

  Eig(A, vals_re, vals_im, vecs_re, vecs_im);

  // Imaginary parts must all be zero (or negligible)
  const real_t* im = vals_im.HostRead();
  for (int i = 0; i < N; ++i)
    EXPECT_NEAR(im[i], 0.0, kLooseRealTolerance);

  // The set of real parts must match {2, 4, 5}
  const real_t* re = vals_re.HostRead();
  real_t sum = 0.0;
  for (int i = 0; i < N; ++i) sum += re[i];
  EXPECT_NEAR(sum, 11.0, kLooseRealTolerance);  // 2 + 4 + 5
}

TYPED_TEST(EigenTest, GeneralizedEigH) {
  // Av = λBv, A = 2I, B = I → eigenvalues all equal 2
  const int N = 3;
  DMatrix<real_t> A(this->kMemType, N, N);
  DMatrix<real_t> B(this->kMemType, N, N);
  DVector<real_t> vals(this->kMemType, N);
  DMatrix<real_t> vecs(this->kMemType, N, N);

  A << 2.0, 0.0, 0.0,
       0.0, 2.0, 0.0,
       0.0, 0.0, 2.0;
  B << 1.0, 0.0, 0.0,
       0.0, 1.0, 0.0,
       0.0, 0.0, 1.0;

  GeneralizedEigH(A, B, vals, vecs);

  const real_t* v = vals.HostRead();
  for (int i = 0; i < N; ++i)
    EXPECT_NEAR(v[i], 2.0, kLooseRealTolerance);
}

#endif  // ASC_USE_EIGEN

}  // namespace asc
