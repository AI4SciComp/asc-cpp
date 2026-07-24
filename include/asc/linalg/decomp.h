// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/linalg/decomp.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_DECOMP_H_
#define ASC_DECOMP_H_

/// @file decomp.h
/// @brief Matrix decomposition algorithms
///
/// This module provides the following factorizations:
///
/// **Always available (pure C++):**
/// - LUP:      P*A = L*U  (partial pivoting, square matrices)
/// - Cholesky: A = L*L^T  (symmetric positive definite)
/// - QR:       A = Q*R    (Householder reflections, M >= N)
///
/// **Requires Eigen (`ASC_USE_EIGEN`):**
/// - SVD:              A = U * diag(s) * Vt
/// - TruncSVD:         rank-k truncated SVD
/// - EigH:             A*v = λ*v  (symmetric/Hermitian)
/// - Eig:              A*v = λ*v  (general, λ may be complex)
/// - GeneralizedEigH:  A*v = λ*B*v  (symmetric pair, B SPD)
///
/// All functions use `GetMap()` for layout-agnostic element access and
/// operate on the host via `HostRead` / `HostWrite`.

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "asc/core/error.h"
#include "asc/array/marray.h"

#ifdef ASC_USE_EIGEN
#include "asc/linalg/eigen.h"
#endif

namespace asc {

// ============================================================================
// LUP Decomposition
// ============================================================================

/// @brief LUP decomposition with partial pivoting: P*A = L*U
///
/// Factors A into a lower triangular L (unit diagonal), an upper triangular U,
/// and a permutation vector P such that P*A = L*U.
///
/// @tparam Matrix 2D MatrixLike type
/// @tparam Vector 1D VectorLike type (integer element type for P)
/// @param[in,out] A  Square input matrix (N × N); destroyed during computation
/// @param[out]    L  Lower triangular factor (N × N, unit diagonal)
/// @param[out]    U  Upper triangular factor (N × N)
/// @param[out]    P  Permutation vector (length N); P[i] = row swapped into i
///
/// @note Aborts if A is numerically singular.
///
/// @par Example:
/// @code
/// DMatrix<real_t> A(N, N), L(N, N), U(N, N);
/// DVector<int> P(N);
/// asc::LUP(A, L, U, P);
/// asc::LUPSolve(L, U, P, b, x);
/// @endcode
template <MatrixLike Matrix, VectorLike Vector>
void LUP(Matrix& A, Matrix& L, Matrix& U, Vector& P);

/// @brief Solve Ax = b using explicit LUP factors
///
/// Forward substitution L*y = P*b, then back substitution U*x = y.
///
/// @tparam Matrix  2D MatrixLike type
/// @tparam Vector  1D VectorLike type (floating-point)
/// @tparam Pivot   1D VectorLike type (integer, permutation)
/// @param[in]  L  Lower triangular factor (N × N)
/// @param[in]  U  Upper triangular factor (N × N)
/// @param[in]  P  Permutation vector (length N)
/// @param[in]  b  Right-hand side (length N)
/// @param[out] x  Solution (length N)
template <MatrixLike Matrix, VectorLike Vector, VectorLike Pivot>
void LUPSolve(const Matrix& L, const Matrix& U, const Pivot& P,
              const Vector& b, Vector& x);

/// @brief In-place LUP decomposition
///
/// Stores L and U packed in A: lower triangle holds L (unit diagonal implicit),
/// upper triangle holds U.
///
/// @param[in,out] A  Input/output matrix (N × N)
/// @param[out]    P  Permutation vector (length N)
/// @return true if successful; false if A is singular
template <MatrixLike Matrix, VectorLike Vector>
bool LUP(Matrix& A, Vector& P);

/// @brief Solve Ax = b using in-place LUP factors
///
/// L and U are packed in A (lower triangle = L with implicit unit diagonal,
/// upper triangle = U).
///
/// @param[in]  A  Packed LU matrix (N × N)
/// @param[in]  P  Permutation vector (length N)
/// @param[in]  b  Right-hand side (length N)
/// @param[out] x  Solution (length N)
template <MatrixLike Matrix, VectorLike Vector, VectorLike Pivot>
void LUPSolve(const Matrix& A, const Pivot& P, const Vector& b, Vector& x);

// ============================================================================
// Cholesky (LLT) Decomposition
// ============================================================================

/// @brief Cholesky decomposition: A = L * L^T
///
/// Factors a symmetric positive definite matrix A into a lower triangular L
/// such that A = L * L^T, using the Cholesky-Banachiewicz algorithm.
///
/// @tparam Matrix  2D MatrixLike type (square, SPD)
/// @param[in]  A  Symmetric positive definite matrix (N × N)
/// @param[out] L  Lower triangular Cholesky factor (N × N)
/// @return true if successful; false if A is not positive definite
///
/// @par Example:
/// @code
/// DMatrix<real_t> A(N, N), L(N, N);
/// if (asc::Cholesky(A, L)) {
///   asc::CholeskySolve(L, b, x);
/// }
/// @endcode
template <MatrixLike Matrix>
bool Cholesky(const Matrix& A, Matrix& L);

/// @brief Solve Ax = b given Cholesky factor L where A = L * L^T
///
/// Forward substitution L*y = b, then back substitution L^T*x = y.
///
/// @param[in]  L  Cholesky factor (N × N, lower triangular)
/// @param[in]  b  Right-hand side (length N)
/// @param[out] x  Solution (length N)
template <MatrixLike Matrix, VectorLike Vector>
void CholeskySolve(const Matrix& L, const Vector& b, Vector& x);

// ============================================================================
// QR Decomposition (Householder)
// ============================================================================

/// @brief QR decomposition: A = Q * R (Householder reflections)
///
/// Factors an M × N matrix A (M >= N) into an orthogonal Q (M × M) and an
/// upper triangular R (M × N) using successive Householder reflections.
///
/// @tparam Matrix  2D MatrixLike type
/// @param[in]  A  Input matrix (M × N, M >= N)
/// @param[out] Q  Orthogonal factor (M × M)
/// @param[out] R  Upper triangular factor (M × N)
///
/// @par Example:
/// @code
/// DMatrix<real_t> A(M, N), Q(M, M), R(M, N);
/// asc::QR(A, Q, R);
/// asc::QRSolve(Q, R, b, x);  // least-squares if M > N
/// @endcode
template <MatrixLike Matrix>
void QR(const Matrix& A, Matrix& Q, Matrix& R);

/// @brief Solve Ax = b (or least-squares min ||Ax-b||) given QR factors
///
/// Computes y = Q^T * b, then solves R[0:N,0:N] * x = y[0:N] by back
/// substitution. For square systems this is exact; for overdetermined (M > N)
/// it gives the least-squares solution.
///
/// @param[in]  Q  Orthogonal factor (M × M)
/// @param[in]  R  Upper triangular factor (M × N)
/// @param[in]  b  Right-hand side (length M)
/// @param[out] x  Solution (length N)
template <MatrixLike Matrix, VectorLike Vector>
void QRSolve(const Matrix& Q, const Matrix& R, const Vector& b, Vector& x);

// ============================================================================
// SVD and Eigenvalue Decompositions (requires Eigen)
// ============================================================================

#ifdef ASC_USE_EIGEN

/// @brief Thin SVD: A = U * diag(s) * Vt
///
/// Computes the thin singular value decomposition of A. Singular values are
/// returned in descending order.
///
/// @tparam Matrix  2D MatrixLike type
/// @tparam Vector  1D VectorLike type (singular values)
/// @param[in]  A   Input matrix (M × N)
/// @param[out] U   Left singular vectors (M × K, K = min(M,N))
/// @param[out] s   Singular values (length K, descending)
/// @param[out] Vt  Right singular vectors transposed (K × N)
///
/// @par Example:
/// @code
/// DMatrix<real_t> A(M, N), U(M, K), Vt(K, N);
/// DVector<real_t> s(K);
/// asc::SVD(A, U, s, Vt);
/// @endcode
template <MatrixLike Matrix, VectorLike Vector>
void SVD(const Matrix& A, Matrix& U, Vector& s, Matrix& Vt);

/// @brief Truncated SVD: top-k singular triplets of A
///
/// Computes the full SVD internally and returns only the k largest singular
/// values and their associated singular vectors.
///
/// @param[in]  A   Input matrix (M × N)
/// @param[in]  k   Number of singular triplets to retain (1 <= k <= min(M,N))
/// @param[out] U   Left singular vectors (M × k)
/// @param[out] s   Top-k singular values (length k, descending)
/// @param[out] Vt  Right singular vectors transposed (k × N)
template <MatrixLike Matrix, VectorLike Vector>
void TruncSVD(const Matrix& A, int k, Matrix& U, Vector& s, Matrix& Vt);

/// @brief Symmetric eigenvalue decomposition: A * v = λ * v
///
/// Computes eigenvalues and eigenvectors of a real symmetric (Hermitian)
/// matrix. Eigenvalues are returned in ascending order; eigenvectors are
/// the corresponding columns of `vecs`.
///
/// @tparam Matrix  2D MatrixLike type (symmetric)
/// @tparam Vector  1D VectorLike type (real eigenvalues)
/// @param[in]  A     Symmetric matrix (N × N)
/// @param[out] vals  Eigenvalues (length N, ascending)
/// @param[out] vecs  Eigenvectors (N × N, columns are unit eigenvectors)
///
/// @par Example:
/// @code
/// DMatrix<real_t> A(N, N), vecs(N, N);
/// DVector<real_t> vals(N);
/// asc::EigH(A, vals, vecs);
/// @endcode
template <MatrixLike Matrix, VectorLike Vector>
void EigH(const Matrix& A, Vector& vals, Matrix& vecs);

/// @brief Generalized symmetric eigenvalue problem: A * v = λ * B * v
///
/// Solves the generalized eigenvalue problem for symmetric A and symmetric
/// positive definite B. Eigenvalues are returned in ascending order.
///
/// @param[in]  A     Symmetric matrix (N × N)
/// @param[in]  B     Symmetric positive definite matrix (N × N)
/// @param[out] vals  Generalized eigenvalues (length N, ascending)
/// @param[out] vecs  Generalized eigenvectors (N × N)
template <MatrixLike Matrix, VectorLike Vector>
void GeneralizedEigH(const Matrix& A, const Matrix& B,
                     Vector& vals, Matrix& vecs);

/// @brief General eigenvalue decomposition: A * v = λ * v
///
/// Computes eigenvalues and right eigenvectors of a general real matrix.
/// Since eigenvalues may be complex, real and imaginary parts are returned
/// separately, following LAPACK convention (dgeev).
///
/// @param[in]  A        Square matrix (N × N)
/// @param[out] vals_re  Real parts of eigenvalues (length N)
/// @param[out] vals_im  Imaginary parts of eigenvalues (length N)
/// @param[out] vecs_re  Real parts of right eigenvectors (N × N)
/// @param[out] vecs_im  Imaginary parts of right eigenvectors (N × N)
///
/// @note For real eigenvalues, vals_im entries are zero and the corresponding
///       vecs_im columns are zero.
template <MatrixLike Matrix, VectorLike Vector>
void Eig(const Matrix& A, Vector& vals_re, Vector& vals_im,
         Matrix& vecs_re, Matrix& vecs_im);

#endif  // ASC_USE_EIGEN

// ============================================================================
// Template implementations
// ============================================================================

template <MatrixLike Matrix, VectorLike Vector>
void LUP(Matrix& A, Matrix& L, Matrix& U, Vector& P) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  ASC_ASSERT(A.GetExtent(0) == A.GetExtent(1), "A must be square");
  const int N = A.GetExtent(0);

  ASC_ASSERT(L.GetExtent(0) == N && L.GetExtent(1) == N,
                "L must have same dimensions as A");
  ASC_ASSERT(U.GetExtent(0) == N && U.GetExtent(1) == N,
                "U must have same dimensions as A");
  ASC_ASSERT(P.GetSize() == N, "P must have length N");

  const auto& A_map = A.GetMap();
  const auto& L_map = L.GetMap();
  const auto& U_map = U.GetMap();

  int* P_data = P.HostWrite();
  T* A_data = A.HostReadWrite();
  T* L_data = L.HostWrite();
  T* U_data = U.HostWrite();

  for (int i = 0; i < N; ++i) P_data[i] = i;

  for (int i = 0; i < N - 1; ++i) {
    // Find pivot in column i below row i
    T p = T(0);
    int row = i;
    for (int j = i; j < N; ++j) {
      const T abs_val = std::abs(A_data[A_map(j, i)]);
      if (abs_val > p) { p = abs_val; row = j; }
    }
    ASC_VERIFY(p != T(0), "LUP: matrix is singular");

    std::swap(P_data[i], P_data[row]);
    for (int j = 0; j < N; ++j)
      std::swap(A_data[A_map(i, j)], A_data[A_map(row, j)]);

    const T u = A_data[A_map(i, i)];
    for (int j = i + 1; j < N; ++j) {
      const T l = A_data[A_map(j, i)] / u;
      A_data[A_map(j, i)] = l;
      for (int k = i + 1; k < N; ++k)
        A_data[A_map(j, k)] -= A_data[A_map(i, k)] * l;
    }
  }

  // Extract L and U
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      if (i > j) {
        L_data[L_map(i, j)] = A_data[A_map(i, j)];
        U_data[U_map(i, j)] = T(0);
      } else if (i == j) {
        L_data[L_map(i, j)] = T(1);
        U_data[U_map(i, j)] = A_data[A_map(i, j)];
      } else {
        L_data[L_map(i, j)] = T(0);
        U_data[U_map(i, j)] = A_data[A_map(i, j)];
      }
    }
  }
}

template <MatrixLike Matrix, VectorLike Vector, VectorLike Pivot>
void LUPSolve(const Matrix& L, const Matrix& U, const Pivot& P,
              const Vector& b, Vector& x) {
  using T = typename Vector::ElementType;

  const int N = L.GetExtent(0);
  ASC_ASSERT(L.GetRank() == 2 && U.GetRank() == 2,
                "L and U must be 2D matrices");
  ASC_ASSERT(L.GetExtent(0) == N && L.GetExtent(1) == N, "L must be square");
  ASC_ASSERT(U.GetExtent(0) == N && U.GetExtent(1) == N, "U must be square");
  ASC_ASSERT(P.GetSize() == N && b.GetSize() == N && x.GetSize() == N,
                "P, b, x must have length N");

  const auto& L_map = L.GetMap();
  const auto& U_map = U.GetMap();
  const T* L_data = L.HostRead();
  const T* U_data = U.HostRead();
  const int* P_data = P.HostRead();
  const T* b_data = b.HostRead();
  T* x_data = x.HostWrite();

  DVector<T> y(N);
  T* y_data = y.HostWrite();

  // Forward substitution: L * y = P * b
  for (int i = 0; i < N; ++i) {
    y_data[i] = b_data[P_data[i]];
    for (int j = 0; j < i; ++j)
      y_data[i] -= L_data[L_map(i, j)] * y_data[j];
  }

  // Back substitution: U * x = y
  for (int i = N - 1; i >= 0; --i) {
    x_data[i] = y_data[i];
    for (int j = i + 1; j < N; ++j)
      x_data[i] -= U_data[U_map(i, j)] * x_data[j];
    x_data[i] /= U_data[U_map(i, i)];
  }
}

template <MatrixLike Matrix, VectorLike Vector>
bool LUP(Matrix& A, Vector& P) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  ASC_ASSERT(A.GetExtent(0) == A.GetExtent(1), "A must be square");
  const int N = A.GetExtent(0);
  ASC_ASSERT(P.GetSize() == N, "P must have length N");

  const auto& A_map = A.GetMap();
  int* P_data = P.HostWrite();
  T* A_data = A.HostReadWrite();

  for (int i = 0; i < N; ++i) P_data[i] = i;

  for (int i = 0; i < N - 1; ++i) {
    T p = T(0);
    int row = i;
    for (int j = i; j < N; ++j) {
      const T abs_val = std::abs(A_data[A_map(j, i)]);
      if (abs_val > p) { p = abs_val; row = j; }
    }
    if (p == T(0)) return false;

    std::swap(P_data[i], P_data[row]);
    for (int j = 0; j < N; ++j)
      std::swap(A_data[A_map(i, j)], A_data[A_map(row, j)]);

    const T u = A_data[A_map(i, i)];
    for (int j = i + 1; j < N; ++j) {
      const T l = A_data[A_map(j, i)] / u;
      A_data[A_map(j, i)] = l;
      for (int k = i + 1; k < N; ++k)
        A_data[A_map(j, k)] -= A_data[A_map(i, k)] * l;
    }
  }

  return std::abs(A_data[A_map(N - 1, N - 1)]) != T(0);
}

template <MatrixLike Matrix, VectorLike Vector, VectorLike Pivot>
void LUPSolve(const Matrix& A, const Pivot& P, const Vector& b, Vector& x) {
  using T = typename Vector::ElementType;

  const int N = A.GetExtent(0);
  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  ASC_ASSERT(A.GetExtent(0) == N && A.GetExtent(1) == N, "A must be square");
  ASC_ASSERT(P.GetSize() == N && b.GetSize() == N && x.GetSize() == N,
                "P, b, x must have length N");

  const auto& A_map = A.GetMap();
  const T* A_data = A.HostRead();
  const int* P_data = P.HostRead();
  const T* b_data = b.HostRead();
  T* x_data = x.HostWrite();

  DVector<T> y(N);
  T* y_data = y.HostWrite();

  // Forward substitution: L * y = P * b  (L packed in lower triangle of A)
  for (int i = 0; i < N; ++i) {
    y_data[i] = b_data[P_data[i]];
    for (int j = 0; j < i; ++j)
      y_data[i] -= A_data[A_map(i, j)] * y_data[j];
  }

  // Back substitution: U * x = y  (U packed in upper triangle of A)
  for (int i = N - 1; i >= 0; --i) {
    x_data[i] = y_data[i];
    for (int j = i + 1; j < N; ++j)
      x_data[i] -= A_data[A_map(i, j)] * x_data[j];
    x_data[i] /= A_data[A_map(i, i)];
  }
}

// ============================================================================
// Cholesky implementations
// ============================================================================

template <MatrixLike Matrix>
bool Cholesky(const Matrix& A, Matrix& L) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  ASC_ASSERT(A.GetExtent(0) == A.GetExtent(1), "A must be square");
  const int N = A.GetExtent(0);
  ASC_ASSERT(L.GetExtent(0) == N && L.GetExtent(1) == N,
                "L must have same dimensions as A");

  const auto& A_map = A.GetMap();
  const auto& L_map = L.GetMap();
  const T* A_data = A.HostRead();
  T* L_data = L.HostWrite();

  // Zero-initialize L
  for (int j = 0; j < N; ++j)
    for (int i = 0; i < N; ++i)
      L_data[L_map(i, j)] = T(0);

  // Cholesky-Banachiewicz: column-by-column
  for (int j = 0; j < N; ++j) {
    // Diagonal entry: L[j,j] = sqrt(A[j,j] - sum_{k<j} L[j,k]^2)
    T diag = A_data[A_map(j, j)];
    for (int k = 0; k < j; ++k)
      diag -= L_data[L_map(j, k)] * L_data[L_map(j, k)];
    if (diag <= T(0)) return false;  // Not positive definite
    L_data[L_map(j, j)] = std::sqrt(diag);

    // Off-diagonal: L[i,j] = (A[i,j] - sum_{k<j} L[i,k]*L[j,k]) / L[j,j]
    const T inv_diag = T(1) / L_data[L_map(j, j)];
    for (int i = j + 1; i < N; ++i) {
      T val = A_data[A_map(i, j)];
      for (int k = 0; k < j; ++k)
        val -= L_data[L_map(i, k)] * L_data[L_map(j, k)];
      L_data[L_map(i, j)] = val * inv_diag;
    }
  }
  return true;
}

template <MatrixLike Matrix, VectorLike Vector>
void CholeskySolve(const Matrix& L, const Vector& b, Vector& x) {
  using T = typename Vector::ElementType;

  const int N = L.GetExtent(0);
  ASC_ASSERT(L.GetRank() == 2, "L must be a 2D matrix");
  ASC_ASSERT(L.GetExtent(0) == N && L.GetExtent(1) == N, "L must be square");
  ASC_ASSERT(b.GetSize() == N && x.GetSize() == N, "b, x must have length N");

  const auto& L_map = L.GetMap();
  const T* L_data = L.HostRead();
  const T* b_data = b.HostRead();
  T* x_data = x.HostWrite();

  DVector<T> y(N);
  T* y_data = y.HostWrite();

  // Forward substitution: L * y = b
  for (int i = 0; i < N; ++i) {
    T val = b_data[i];
    for (int j = 0; j < i; ++j)
      val -= L_data[L_map(i, j)] * y_data[j];
    y_data[i] = val / L_data[L_map(i, i)];
  }

  // Back substitution: L^T * x = y  (L^T[i,j] = L[j,i])
  for (int i = N - 1; i >= 0; --i) {
    T val = y_data[i];
    for (int j = i + 1; j < N; ++j)
      val -= L_data[L_map(j, i)] * x_data[j];
    x_data[i] = val / L_data[L_map(i, i)];
  }
}

// ============================================================================
// QR implementations
// ============================================================================

template <MatrixLike Matrix>
void QR(const Matrix& A, Matrix& Q, Matrix& R) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  const int M = A.GetExtent(0);
  const int N = A.GetExtent(1);
  ASC_ASSERT(M >= N, "QR requires M >= N");
  ASC_ASSERT(Q.GetExtent(0) == M && Q.GetExtent(1) == M, "Q must be M × M");
  ASC_ASSERT(R.GetExtent(0) == M && R.GetExtent(1) == N, "R must be M × N");

  const auto& A_map = A.GetMap();
  const auto& Q_map = Q.GetMap();
  const auto& R_map = R.GetMap();
  const T* A_data = A.HostRead();
  T* Q_data = Q.HostWrite();
  T* R_data = R.HostWrite();

  // R = A (copy), Q = I_M
  for (int j = 0; j < N; ++j)
    for (int i = 0; i < M; ++i)
      R_data[R_map(i, j)] = A_data[A_map(i, j)];
  for (int j = 0; j < M; ++j)
    for (int i = 0; i < M; ++i)
      Q_data[Q_map(i, j)] = (i == j) ? T(1) : T(0);

  DVector<T> v(M);
  T* v_data = v.HostReadWrite();
  const T eps = std::numeric_limits<T>::epsilon();

  // Apply N Householder reflectors
  for (int k = 0; k < N; ++k) {
    // x = R[k:M, k];  norm_x = ||x||
    T norm_x = T(0);
    for (int i = k; i < M; ++i)
      norm_x += R_data[R_map(i, k)] * R_data[R_map(i, k)];
    norm_x = std::sqrt(norm_x);

    if (norm_x < eps) continue;

    // v = x;  v[k] -= alpha = -sign(R[k,k]) * norm_x
    for (int i = 0; i < M; ++i) v_data[i] = T(0);
    for (int i = k; i < M; ++i) v_data[i] = R_data[R_map(i, k)];
    const T alpha = (R_data[R_map(k, k)] >= T(0)) ? -norm_x : norm_x;
    v_data[k] -= alpha;

    T v_sq = T(0);
    for (int i = k; i < M; ++i) v_sq += v_data[i] * v_data[i];
    if (v_sq < eps) continue;

    const T beta = T(2) / v_sq;

    // Apply H from left to R:  R[k:M, k:N] -= beta * v * (v^T * R[k:M, k:N])
    for (int j = k; j < N; ++j) {
      T dot = T(0);
      for (int i = k; i < M; ++i) dot += v_data[i] * R_data[R_map(i, j)];
      for (int i = k; i < M; ++i) R_data[R_map(i, j)] -= beta * v_data[i] * dot;
    }

    // Apply H from right to Q:  Q[0:M, k:M] -= beta * (Q[0:M, k:M] * v) * v^T
    // This builds Q = H_0 * H_1 * ... * H_{N-1} column by column
    for (int i = 0; i < M; ++i) {
      T dot = T(0);
      for (int r = k; r < M; ++r) dot += Q_data[Q_map(i, r)] * v_data[r];
      for (int r = k; r < M; ++r) Q_data[Q_map(i, r)] -= beta * dot * v_data[r];
    }
  }
}

template <MatrixLike Matrix, VectorLike Vector>
void QRSolve(const Matrix& Q, const Matrix& R, const Vector& b, Vector& x) {
  using T = typename Vector::ElementType;

  const int M = Q.GetExtent(0);
  const int N = R.GetExtent(1);
  ASC_ASSERT(Q.GetRank() == 2 && R.GetRank() == 2, "Q and R must be 2D");
  ASC_ASSERT(Q.GetExtent(0) == M && Q.GetExtent(1) == M, "Q must be M × M");
  ASC_ASSERT(R.GetExtent(0) == M, "R must have M rows");
  ASC_ASSERT(b.GetSize() == M, "b must have length M");
  ASC_ASSERT(x.GetSize() == N, "x must have length N");

  const auto& Q_map = Q.GetMap();
  const auto& R_map = R.GetMap();
  const T* Q_data = Q.HostRead();
  const T* R_data = R.HostRead();
  const T* b_data = b.HostRead();
  T* x_data = x.HostWrite();

  // y = Q^T * b
  DVector<T> y(M);
  T* y_data = y.HostWrite();
  for (int i = 0; i < M; ++i) {
    T dot = T(0);
    for (int r = 0; r < M; ++r) dot += Q_data[Q_map(r, i)] * b_data[r];
    y_data[i] = dot;
  }

  // Back substitution: R[0:N, 0:N] * x = y[0:N]
  const T eps = std::numeric_limits<T>::epsilon();
  for (int i = N - 1; i >= 0; --i) {
    T val = y_data[i];
    for (int j = i + 1; j < N; ++j)
      val -= R_data[R_map(i, j)] * x_data[j];
    ASC_VERIFY(std::abs(R_data[R_map(i, i)]) > eps, "QRSolve: R is singular");
    x_data[i] = val / R_data[R_map(i, i)];
  }
}

// ============================================================================
// SVD and eigenvalue implementations (requires Eigen)
// ============================================================================

#ifdef ASC_USE_EIGEN

template <MatrixLike Matrix, VectorLike Vector>
void SVD(const Matrix& A, Matrix& U, Vector& s, Matrix& Vt) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  const int M = A.GetExtent(0);
  const int N = A.GetExtent(1);
  const int K = std::min(M, N);
  ASC_ASSERT(U.GetExtent(0) == M && U.GetExtent(1) == K,
                "U must be M × K where K = min(M,N)");
  ASC_ASSERT(s.GetSize() == K, "s must have length K = min(M,N)");
  ASC_ASSERT(Vt.GetExtent(0) == K && Vt.GetExtent(1) == N,
                "Vt must be K × N where K = min(M,N)");

  EDMatrix<T> eigen_A;
  DMatrixToEigen(A, eigen_A);

  Eigen::JacobiSVD<EDMatrix<T>> svd(eigen_A,
      Eigen::ComputeThinU | Eigen::ComputeThinV);

  const auto& U_map  = U.GetMap();
  const auto& Vt_map = Vt.GetMap();
  T* U_data  = U.HostWrite();
  T* s_data  = s.HostWrite();
  T* Vt_data = Vt.HostWrite();

  for (int j = 0; j < K; ++j)
    for (int i = 0; i < M; ++i)
      U_data[U_map(i, j)] = svd.matrixU()(i, j);

  for (int i = 0; i < K; ++i)
    s_data[i] = svd.singularValues()(i);

  // Vt = V^T
  for (int j = 0; j < N; ++j)
    for (int i = 0; i < K; ++i)
      Vt_data[Vt_map(i, j)] = svd.matrixV()(j, i);
}

template <MatrixLike Matrix, VectorLike Vector>
void TruncSVD(const Matrix& A, int k, Matrix& U, Vector& s, Matrix& Vt) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  const int M = A.GetExtent(0);
  const int N = A.GetExtent(1);
  const int K = std::min(M, N);
  ASC_VERIFY(k >= 1 && k <= K, "TruncSVD: k must satisfy 1 <= k <= min(M,N)");
  ASC_ASSERT(U.GetExtent(0) == M && U.GetExtent(1) == k,
                "U must be M × k");
  ASC_ASSERT(s.GetSize() == k, "s must have length k");
  ASC_ASSERT(Vt.GetExtent(0) == k && Vt.GetExtent(1) == N,
                "Vt must be k × N");

  // Compute full thin SVD then truncate
  DMatrix<T> U_full(M, K);
  DVector<T> s_full(K);
  DMatrix<T> Vt_full(K, N);
  SVD(A, U_full, s_full, Vt_full);

  const auto& Uf_map  = U_full.GetMap();
  const auto& Vtf_map = Vt_full.GetMap();
  const auto& U_map   = U.GetMap();
  const auto& Vt_map  = Vt.GetMap();
  const T* Uf_data  = U_full.HostRead();
  const T* sf_data  = s_full.HostRead();
  const T* Vtf_data = Vt_full.HostRead();
  T* U_data  = U.HostWrite();
  T* s_data  = s.HostWrite();
  T* Vt_data = Vt.HostWrite();

  for (int j = 0; j < k; ++j)
    for (int i = 0; i < M; ++i)
      U_data[U_map(i, j)] = Uf_data[Uf_map(i, j)];

  for (int i = 0; i < k; ++i)
    s_data[i] = sf_data[i];

  for (int j = 0; j < N; ++j)
    for (int i = 0; i < k; ++i)
      Vt_data[Vt_map(i, j)] = Vtf_data[Vtf_map(i, j)];
}

template <MatrixLike Matrix, VectorLike Vector>
void EigH(const Matrix& A, Vector& vals, Matrix& vecs) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  ASC_ASSERT(A.GetExtent(0) == A.GetExtent(1), "A must be square");
  const int N = A.GetExtent(0);
  ASC_ASSERT(vals.GetSize() == N, "vals must have length N");
  ASC_ASSERT(vecs.GetExtent(0) == N && vecs.GetExtent(1) == N,
                "vecs must be N × N");

  EDMatrix<T> eigen_A;
  DMatrixToEigen(A, eigen_A);

  Eigen::SelfAdjointEigenSolver<EDMatrix<T>> solver(eigen_A);
  ASC_VERIFY(solver.info() == Eigen::Success,
                "EigH: eigenvalue computation failed");

  DVectorFromEigen(solver.eigenvalues(), vals);
  DMatrixFromEigen(solver.eigenvectors(), vecs);
}

template <MatrixLike Matrix, VectorLike Vector>
void GeneralizedEigH(const Matrix& A, const Matrix& B,
                     Vector& vals, Matrix& vecs) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2 && B.GetRank() == 2,
                "A and B must be 2D matrices");
  ASC_ASSERT(A.GetExtent(0) == A.GetExtent(1), "A must be square");
  const int N = A.GetExtent(0);
  ASC_ASSERT(B.GetExtent(0) == N && B.GetExtent(1) == N,
                "B must have same dimensions as A");
  ASC_ASSERT(vals.GetSize() == N, "vals must have length N");
  ASC_ASSERT(vecs.GetExtent(0) == N && vecs.GetExtent(1) == N,
                "vecs must be N × N");

  EDMatrix<T> eigen_A, eigen_B;
  DMatrixToEigen(A, eigen_A);
  DMatrixToEigen(B, eigen_B);

  Eigen::GeneralizedSelfAdjointEigenSolver<EDMatrix<T>> solver(eigen_A, eigen_B);
  ASC_VERIFY(solver.info() == Eigen::Success,
                "GeneralizedEigH: eigenvalue computation failed");

  DVectorFromEigen(solver.eigenvalues(), vals);
  DMatrixFromEigen(solver.eigenvectors(), vecs);
}

template <MatrixLike Matrix, VectorLike Vector>
void Eig(const Matrix& A, Vector& vals_re, Vector& vals_im,
         Matrix& vecs_re, Matrix& vecs_im) {
  using T = typename Matrix::ElementType;

  ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");
  ASC_ASSERT(A.GetExtent(0) == A.GetExtent(1), "A must be square");
  const int N = A.GetExtent(0);
  ASC_ASSERT(vals_re.GetSize() == N && vals_im.GetSize() == N,
                "vals_re, vals_im must have length N");
  ASC_ASSERT(vecs_re.GetExtent(0) == N && vecs_re.GetExtent(1) == N,
                "vecs_re must be N × N");
  ASC_ASSERT(vecs_im.GetExtent(0) == N && vecs_im.GetExtent(1) == N,
                "vecs_im must be N × N");

  EDMatrix<T> eigen_A;
  DMatrixToEigen(A, eigen_A);

  Eigen::EigenSolver<EDMatrix<T>> solver(eigen_A);
  ASC_VERIFY(solver.info() == Eigen::Success,
                "Eig: eigenvalue computation failed");

  T* re_data = vals_re.HostWrite();
  T* im_data = vals_im.HostWrite();
  for (int i = 0; i < N; ++i) {
    re_data[i] = solver.eigenvalues()(i).real();
    im_data[i] = solver.eigenvalues()(i).imag();
  }

  const auto& re_map = vecs_re.GetMap();
  const auto& im_map = vecs_im.GetMap();
  T* vr_data = vecs_re.HostWrite();
  T* vi_data = vecs_im.HostWrite();
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      vr_data[re_map(i, j)] = solver.eigenvectors()(i, j).real();
      vi_data[im_map(i, j)] = solver.eigenvectors()(i, j).imag();
    }
  }
}

#endif  // ASC_USE_EIGEN

}  // namespace asc

#endif  // ASC_DECOMP_H_
