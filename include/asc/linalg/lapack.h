// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/linalg/lapack.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_LAPACK_H_
#define ASC_LAPACK_H_

/// @file lapack.h
/// @brief Linear algebra solver with MATLAB-style automatic method selection
///
/// This module provides a unified LinearSolver class that automatically
/// selects the optimal solving strategy based on matrix properties,
/// following MATLAB's backslash (\) operator decision tree.
///
/// @par Design Philosophy (MATLAB backslash-inspired):
/// The solver checks matrix properties in precedence order:
/// 1. **Diagonal** → Direct division
/// 2. **Triangular** (upper/lower) → Forward/backward substitution
/// 3. **Permuted triangular** → Permuted substitution
/// 4. **Symmetric positive definite** → Cholesky (LLT) or LDLT
/// 5. **Symmetric indefinite** → LDLT factorization
/// 6. **General square** → LU with partial pivoting
/// 7. **Rectangular** → QR decomposition (least squares)
///
/// @par For Sparse Matrices:
/// 1. **Diagonal** → Direct division
/// 2. **Triangular** → Sparse triangular solve
/// 3. **Symmetric positive definite** → SimplicialLLT or SimplicialLDLT
/// 4. **General square** → SparseLU
///
/// @par Example - Dense matrix:
/// @code
/// DMatrix<real_t> A(N, N);
/// DVector<real_t> b(N), x(N);
/// LinearSolver<real_t> solver;
/// solver.SetOperator(A);
/// solver.LinearSolve(b, x);
/// @endcode
///
/// @par Example - Sparse matrix:
/// @code
/// SpDMatrix<real_t> A(N, N);
/// DVector<real_t> b(N), x(N);
/// LinearSolver<real_t> solver;
/// solver.SetOperator(A);
/// solver.LinearSolve(b, x);
/// @endcode

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include "asc/linalg/decomp.h"
#include "asc/linalg/types.h"
#include "asc/core/error.h"
#include "asc/core/math.h"
#include "asc/array/marray.h"

#ifdef ASC_USE_EIGEN
#include "asc/linalg/eigen.h"
#endif

namespace asc {

// ============================================================================
// Enums and Configuration
// ============================================================================

/// @brief Linear solver method types (MATLAB-style precedence)
enum class SolverMethod {
  kAuto,  ///< Automatic selection based on matrix properties
  // Dense solvers
  kDiagonal,             ///< Diagonal matrix (direct division)
  kLowerTriangular,      ///< Lower triangular (forward substitution)
  kUpperTriangular,      ///< Upper triangular (backward substitution)
  kPermutedTriangular,   ///< Permuted triangular matrix
  kLLT,                  ///< Cholesky LLT (dense, SPD)
  kLDLT,                 ///< LDLT (dense, symmetric)
  kPartialPivLU,         ///< LU with partial pivoting (dense, general)
  kFullPivLU,            ///< LU with full pivoting (dense, general)
  kHouseholderQR,        ///< QR decomposition (rectangular)
  kColPivHouseholderQR,  ///< Column-pivoted QR (rectangular)
  // Sparse solvers
  kSparseDiagonal,    ///< Sparse diagonal
  kSparseTriangular,  ///< Sparse triangular
  kSimplicialLLT,     ///< Sparse Cholesky LLT (SPD)
  kSimplicialLDLT,    ///< Sparse LDLT (symmetric)
  kSparseLU,          ///< Sparse LU (general)
  // Iterative solvers (for very large sparse systems)
  kCG,        ///< Conjugate Gradient (sparse, SPD)
  kBiCGSTAB,  ///< BiCGSTAB (sparse, general)
};

/// @brief Solver status flags
enum class SolverStatus {
  kUninitialized,  ///< No operator set
  kAnalyzed,       ///< Matrix analyzed, method selected
  kFactorized,     ///< Factorization complete, ready to solve
  kSingular,       ///< Matrix detected as singular
  kFailed,         ///< Solver failed
};

/// @brief Linear solver configuration options
struct LinearSolverOptions {
  /// Tolerance for zero detection (triangular, diagonal checks)
  real_t zero_tol = 1e-14;

  /// Tolerance for symmetry detection
  real_t symmetry_tol = 1e-12;

  /// Tolerance for iterative solvers
  real_t iterative_tol = 1e-10;

  /// Maximum iterations for iterative solvers
  int max_iterations = 1000;

  /// NNZ threshold for direct vs iterative methods (sparse)
  int nnz_threshold = 100000;

  /// Whether to force a specific method (default: auto)
  SolverMethod method = SolverMethod::kAuto;

  /// Whether to use iterative solver for large sparse systems
  bool use_iterative = true;
};

// ============================================================================
// LinearSolver Class
// ============================================================================

/// @brief Adaptive linear solver with MATLAB-style automatic method selection
///
/// LinearSolver provides a unified interface for solving linear systems Ax = b.
/// It automatically detects matrix properties and selects the optimal solving
/// strategy following MATLAB's backslash decision tree.
///
/// Supports both dense (DMatrix) and sparse (SpDMatrix) matrices.
/// All implementations use Eigen library backends.
///
/// @tparam T Scalar type (float, double)
///
/// @par Thread Safety:
/// The solver is NOT thread-safe. Each thread should use its own instance.
template <typename T>
class LinearSolver {
 public:
  // --------------------------------------------------------------------------
  // Type Aliases
  // --------------------------------------------------------------------------
  using Vector = DVector<T>;
  using Matrix = DMatrix<T>;
  using SparseMatrix = SpDMatrix<T>;

  // --------------------------------------------------------------------------
  // Constructors and Destructor
  // --------------------------------------------------------------------------

  /// @brief Default constructor with default options
  LinearSolver() : options_(), status_(SolverStatus::kUninitialized) {}

  /// @brief Constructor with custom options
  explicit LinearSolver(const LinearSolverOptions& options)
      : options_(options), status_(SolverStatus::kUninitialized) {}

  /// @brief Destructor
  ~LinearSolver() = default;

  // Disable copy
  LinearSolver(const LinearSolver&) = delete;
  LinearSolver& operator=(const LinearSolver&) = delete;

  // Enable move
  LinearSolver(LinearSolver&&) = default;
  LinearSolver& operator=(LinearSolver&&) = default;

  // --------------------------------------------------------------------------
  // Configuration
  // --------------------------------------------------------------------------

  /// @brief Set solver options
  void SetOptions(const LinearSolverOptions& options) {
    options_ = options;
    Clear();
  }

  /// @brief Get current solver options
  const LinearSolverOptions& GetOptions() const { return options_; }

  /// @brief Force a specific solving method
  void SetMethod(SolverMethod method) {
    options_.method = method;
    Clear();
  }

  // --------------------------------------------------------------------------
  // Operator Setup - Dense Matrix
  // --------------------------------------------------------------------------

  /// @brief Set dense coefficient matrix
  void SetOperator(const Matrix& A) {
#ifndef ASC_USE_EIGEN
    ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");

    nrows_ = A.GetExtent(0);
    ncols_ = A.GetExtent(1);
    is_sparse_ = false;
    is_square_ = (nrows_ == ncols_);
    ASC_VERIFY(is_square_,
                  "Dense fallback LinearSolver requires a square matrix");

    fallback_A_ = Matrix(A);
    selected_method_ = SolverMethod::kPartialPivLU;
    status_ = SolverStatus::kFactorized;
#else
    ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");

    nrows_ = A.GetExtent(0);
    ncols_ = A.GetExtent(1);
    is_sparse_ = false;
    is_square_ = (nrows_ == ncols_);

    // Convert to Eigen matrix
    DMatrixToEigen(A, eigen_A_);

    AnalyzeDense();
    FactorizeDense();
#endif
  }

  /// @brief Set sparse coefficient matrix
  ///
  /// @note The source matrix A must remain valid for the lifetime of the solver
  /// (zero-copy storage is used internally).
  void SetOperator(const SparseMatrix& A) {
#ifndef ASC_USE_EIGEN
    ASC_ABORT("LinearSolver requires ASC_USE_EIGEN");
#else
    ASC_ASSERT(A.GetRank() == 2, "A must be a 2D matrix");

    nrows_ = A.GetExtent(0);
    ncols_ = A.GetExtent(1);
    nnz_ = A.GetNNZ();
    is_sparse_ = true;
    is_square_ = (nrows_ == ncols_);

    // Store raw pointers for zero-copy Map reconstruction
    const auto& map = A.GetMap();
    sparse_outer_ptr_ = map.GetOuterPtr().HostRead();
    sparse_inner_ptr_ = map.GetInnerIndices(0).HostRead();
    sparse_values_ptr_ = A.GetValues().HostRead();

    // Create zero-copy Eigen Map for analysis and factorization
    // Note: Eigen solvers may copy internally, but the ASC->Eigen
    // transfer is zero-copy
    eigen_sparse_A_ = SpDMatrixToEigen(A);

    AnalyzeSparse();
    FactorizeSparse();
#endif
  }

  // --------------------------------------------------------------------------
  // Status Queries
  // --------------------------------------------------------------------------

  /// @brief Check if the solver has a valid factorization
  bool IsReady() const { return status_ == SolverStatus::kFactorized; }

  /// @brief Get current solver status
  SolverStatus GetStatus() const { return status_; }

  /// @brief Get the selected solving method
  SolverMethod GetMethod() const { return selected_method_; }

  /// @brief Get the number of rows
  int GetRows() const { return nrows_; }

  /// @brief Get the number of columns
  int GetCols() const { return ncols_; }

  /// @brief Check if operating on sparse matrix
  bool IsSparse() const { return is_sparse_; }

  /// @brief Check if matrix is square
  bool IsSquare() const { return is_square_; }

  // --------------------------------------------------------------------------
  // Solving
  // --------------------------------------------------------------------------

  /// @brief Solve the linear system Ax = b
  void LinearSolve(const Vector& b, Vector& x) const {
    ASC_ASSERT(status_ == SolverStatus::kFactorized,
                  "Solver not ready. Call SetOperator() first.");
    ASC_ASSERT(b.GetSize() == nrows_, "RHS dimension mismatch");

    if (x.GetSize() != ncols_) {
      x.SetShape(DShape<1>(ncols_));
    }

#ifndef ASC_USE_EIGEN
    ASC_VERIFY(!is_sparse_,
                  "Sparse LinearSolver requires ASC_USE_EIGEN");
    LinearSolveDenseFallback(b, x);
#else
    // Convert b to Eigen vector using eigen.h utility
    EDVector<T> eigen_b;
    DVectorToEigen(b, eigen_b);

    EDVector<T> eigen_x;

    if (is_sparse_) {
      eigen_x = SolveSparseImpl(eigen_b);
    } else {
      eigen_x = SolveDenseImpl(eigen_b);
    }

    // Convert result back using eigen.h utility
    DVectorFromEigen(eigen_x, x);
#endif
  }

  /// @brief Solve and return solution
  Vector LinearSolve(const Vector& b) const {
    Vector x(ncols_);
    LinearSolve(b, x);
    return x;
  }

  /// @brief Solve with multiple right-hand sides: AX = B
  void SolveMultiple(const Matrix& B, Matrix& X) const {
#ifdef ASC_USE_EIGEN
    ASC_ASSERT(status_ == SolverStatus::kFactorized,
                  "Solver not ready. Call SetOperator() first.");
    ASC_ASSERT(B.GetExtent(0) == nrows_, "RHS row dimension mismatch");

    const int num_rhs = B.GetExtent(1);
    X.SetShape(DShape<2>(ncols_, num_rhs));

    // Convert B to Eigen matrix using eigen.h utility
    EDMatrix<T> eigen_B;
    DMatrixToEigen(B, eigen_B);

    // Solve column by column
    for (int k = 0; k < num_rhs; ++k) {
      EDVector<T> eigen_b = eigen_B.col(k);
      EDVector<T> eigen_x;

      if (is_sparse_) {
        eigen_x = SolveSparseImpl(eigen_b);
      } else {
        eigen_x = SolveDenseImpl(eigen_b);
      }

      // Store in column k of X
      T* X_data = X.HostWrite();
      for (int i = 0; i < ncols_; ++i) {
        X_data[k * ncols_ + i] = eigen_x(i);
      }
    }
#endif
  }

  // --------------------------------------------------------------------------
  // Utilities
  // --------------------------------------------------------------------------

  /// @brief Clear cached factorization and reset solver
  void Clear() {
    status_ = SolverStatus::kUninitialized;
    nrows_ = 0;
    ncols_ = 0;
    nnz_ = 0;
    is_sparse_ = false;
    is_square_ = false;

#ifdef ASC_USE_EIGEN
    eigen_A_.resize(0, 0);
    eigen_sparse_A_.resize(0, 0);

    // Dense solvers
    llt_.reset();
    ldlt_.reset();
    partial_piv_lu_.reset();
    full_piv_lu_.reset();
    householder_qr_.reset();
    col_piv_qr_.reset();

    // Sparse solvers
    simplicial_llt_.reset();
    simplicial_ldlt_.reset();
    sparse_lu_.reset();
    cg_solver_.reset();
    bicgstab_solver_.reset();

    // Cached data
    diagonal_.resize(0);
    perm_.resize(0);

    // Reset sparse pointers
    sparse_outer_ptr_ = nullptr;
    sparse_inner_ptr_ = nullptr;
    sparse_values_ptr_ = nullptr;
#else
    fallback_A_ = Matrix();
#endif
  }

 private:
#ifndef ASC_USE_EIGEN
  void LinearSolveDenseFallback(const Vector& b, Vector& x) const {
    ASC_VERIFY(is_square_,
                  "Dense fallback LinearSolver requires a square matrix");

    Matrix A = fallback_A_;
    Vector rhs = b;
    const auto& a_map = A.GetMap();
    T* a_data = A.HostReadWrite();
    T* rhs_data = rhs.HostReadWrite();
    T* x_data = x.HostWrite();

    for (int i = 0; i < ncols_; ++i) x_data[i] = T(0);

    for (int k = 0; k < ncols_; ++k) {
      int pivot = k;
      T pivot_abs = std::abs(a_data[a_map(k, k)]);
      for (int i = k + 1; i < nrows_; ++i) {
        const T value = std::abs(a_data[a_map(i, k)]);
        if (value > pivot_abs) {
          pivot_abs = value;
          pivot = i;
        }
      }

      ASC_VERIFY(pivot_abs > options_.zero_tol,
                    "LinearSolver: matrix is singular");
      if (pivot != k) {
        for (int j = k; j < ncols_; ++j) {
          std::swap(a_data[a_map(k, j)], a_data[a_map(pivot, j)]);
        }
        std::swap(rhs_data[k], rhs_data[pivot]);
      }

      for (int i = k + 1; i < nrows_; ++i) {
        const T factor = a_data[a_map(i, k)] / a_data[a_map(k, k)];
        a_data[a_map(i, k)] = T(0);
        for (int j = k + 1; j < ncols_; ++j) {
          a_data[a_map(i, j)] -= factor * a_data[a_map(k, j)];
        }
        rhs_data[i] -= factor * rhs_data[k];
      }
    }

    for (int i = ncols_ - 1; i >= 0; --i) {
      T rhs_value = rhs_data[i];
      for (int j = i + 1; j < ncols_; ++j) {
        rhs_value -= a_data[a_map(i, j)] * x_data[j];
      }
      ASC_VERIFY(std::abs(a_data[a_map(i, i)]) > options_.zero_tol,
                    "LinearSolver: matrix is singular");
      x_data[i] = rhs_value / a_data[a_map(i, i)];
    }
  }
#endif

#ifdef ASC_USE_EIGEN
  // --------------------------------------------------------------------------
  // Dense Matrix Analysis (MATLAB-style decision tree)
  // --------------------------------------------------------------------------

  void AnalyzeDense() {
    if (options_.method != SolverMethod::kAuto) {
      selected_method_ = options_.method;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Follow MATLAB's decision tree for full matrices:
    // 1. Diagonal?
    // 2. Triangular (upper/lower)?
    // 3. Permuted triangular?
    // 4. Symmetric positive definite → Cholesky
    // 5. Symmetric indefinite → LDLT
    // 6. General square → LU
    // 7. Rectangular → QR

    if (!is_square_) {
      // Rectangular matrix → QR
      selected_method_ = SolverMethod::kColPivHouseholderQR;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Check diagonal
    if (IsDiagonal(eigen_A_)) {
      selected_method_ = SolverMethod::kDiagonal;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Check lower triangular
    if (IsLowerTriangular(eigen_A_)) {
      selected_method_ = SolverMethod::kLowerTriangular;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Check upper triangular
    if (IsUpperTriangular(eigen_A_)) {
      selected_method_ = SolverMethod::kUpperTriangular;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Check symmetric
    bool is_symmetric = IsSymmetric(eigen_A_);

    if (is_symmetric) {
      // Try Cholesky first (for SPD)
      // We'll attempt LLT, if it fails fall back to LDLT
      selected_method_ = SolverMethod::kLLT;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // General square → LU with partial pivoting
    selected_method_ = SolverMethod::kPartialPivLU;
    status_ = SolverStatus::kAnalyzed;
  }

  void FactorizeDense() {
    switch (selected_method_) {
      case SolverMethod::kDiagonal:
        FactorizeDiagonal();
        break;

      case SolverMethod::kLowerTriangular:
      case SolverMethod::kUpperTriangular:
        // No factorization needed, solve directly
        status_ = SolverStatus::kFactorized;
        break;

      case SolverMethod::kLLT:
        FactorizeLLT();
        break;

      case SolverMethod::kLDLT:
        FactorizeLDLT();
        break;

      case SolverMethod::kPartialPivLU:
        FactorizePartialPivLU();
        break;

      case SolverMethod::kFullPivLU:
        FactorizeFullPivLU();
        break;

      case SolverMethod::kHouseholderQR:
        FactorizeHouseholderQR();
        break;

      case SolverMethod::kColPivHouseholderQR:
        FactorizeColPivHouseholderQR();
        break;

      default:
        FactorizePartialPivLU();
        break;
    }
  }

  EDVector<T> SolveDenseImpl(const EDVector<T>& b) const {
    switch (selected_method_) {
      case SolverMethod::kDiagonal:
        return b.cwiseQuotient(diagonal_);

      case SolverMethod::kLowerTriangular:
        return eigen_A_.template triangularView<Eigen::Lower>().solve(b);

      case SolverMethod::kUpperTriangular:
        return eigen_A_.template triangularView<Eigen::Upper>().solve(b);

      case SolverMethod::kLLT:
        return llt_->solve(b);

      case SolverMethod::kLDLT:
        return ldlt_->solve(b);

      case SolverMethod::kPartialPivLU:
        return partial_piv_lu_->solve(b);

      case SolverMethod::kFullPivLU:
        return full_piv_lu_->solve(b);

      case SolverMethod::kHouseholderQR:
        return householder_qr_->solve(b);

      case SolverMethod::kColPivHouseholderQR:
        return col_piv_qr_->solve(b);

      default:
        ASC_ABORT("LinearSolver: invalid dense solver method");
        return EDVector<T>();
    }
  }

  // --------------------------------------------------------------------------
  // Sparse Matrix Analysis (MATLAB-style decision tree)
  // --------------------------------------------------------------------------

  void AnalyzeSparse() {
    if (options_.method != SolverMethod::kAuto) {
      selected_method_ = options_.method;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    if (!is_square_) {
      // Sparse rectangular → use dense QR for now
      ASC_ABORT("LinearSolver: sparse rectangular not yet supported");
    }

    // Follow MATLAB's sparse decision tree:
    // 1. Diagonal?
    // 2. Triangular?
    // 3. Symmetric positive definite → SimplicialLLT/LDLT
    // 4. General → SparseLU
    // (Large systems with use_iterative → CG/BiCGSTAB)

    // Check diagonal
    if (IsSparseDiagonal(eigen_sparse_A_)) {
      selected_method_ = SolverMethod::kSparseDiagonal;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Check triangular
    if (IsSparseTriangular(eigen_sparse_A_)) {
      selected_method_ = SolverMethod::kSparseTriangular;
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Check symmetric
    bool is_symmetric = IsSparseSymmetric(eigen_sparse_A_);

    // For large systems, consider iterative methods
    if (options_.use_iterative && nnz_ > options_.nnz_threshold) {
      if (is_symmetric && HasPositiveDiagonal(eigen_sparse_A_)) {
        selected_method_ = SolverMethod::kCG;
      } else {
        selected_method_ = SolverMethod::kBiCGSTAB;
      }
      status_ = SolverStatus::kAnalyzed;
      return;
    }

    // Direct methods
    if (is_symmetric) {
      // Try SimplicialLLT for SPD, fall back to SimplicialLDLT
      if (HasPositiveDiagonal(eigen_sparse_A_)) {
        selected_method_ = SolverMethod::kSimplicialLLT;
      } else {
        selected_method_ = SolverMethod::kSimplicialLDLT;
      }
    } else {
      selected_method_ = SolverMethod::kSparseLU;
    }

    status_ = SolverStatus::kAnalyzed;
  }

  void FactorizeSparse() {
    switch (selected_method_) {
      case SolverMethod::kSparseDiagonal:
        FactorizeSparseDiagonal();
        break;

      case SolverMethod::kSparseTriangular:
        // No factorization needed
        DetectSparseTriangularType();
        status_ = SolverStatus::kFactorized;
        break;

      case SolverMethod::kSimplicialLLT:
        FactorizeSimplicialLLT();
        break;

      case SolverMethod::kSimplicialLDLT:
        FactorizeSimplicialLDLT();
        break;

      case SolverMethod::kSparseLU:
        FactorizeSparseLU();
        break;

      case SolverMethod::kCG:
        FactorizeCG();
        break;

      case SolverMethod::kBiCGSTAB:
        FactorizeBiCGSTAB();
        break;

      default:
        FactorizeSparseLU();
        break;
    }
  }

  EDVector<T> SolveSparseImpl(const EDVector<T>& b) const {
    switch (selected_method_) {
      case SolverMethod::kSparseDiagonal:
        return b.cwiseQuotient(diagonal_);

      case SolverMethod::kSparseTriangular:
        if (is_lower_triangular_) {
          return eigen_sparse_A_.template triangularView<Eigen::Lower>().solve(
              b);
        } else {
          return eigen_sparse_A_.template triangularView<Eigen::Upper>().solve(
              b);
        }

      case SolverMethod::kSimplicialLLT:
        return simplicial_llt_->solve(b);

      case SolverMethod::kSimplicialLDLT:
        return simplicial_ldlt_->solve(b);

      case SolverMethod::kSparseLU:
        return sparse_lu_->solve(b);

      case SolverMethod::kCG:
        return cg_solver_->solve(b);

      case SolverMethod::kBiCGSTAB:
        return bicgstab_solver_->solve(b);

      default:
        ASC_ABORT("LinearSolver: invalid sparse solver method");
        return EDVector<T>();
    }
  }

  // --------------------------------------------------------------------------
  // Dense Factorization Methods
  // --------------------------------------------------------------------------

  void FactorizeDiagonal() {
    diagonal_ = eigen_A_.diagonal();

    // Check for zeros on diagonal
    for (int i = 0; i < nrows_; ++i) {
      if (std::abs(diagonal_(i)) < options_.zero_tol) {
        status_ = SolverStatus::kSingular;
        ASC_ABORT("LinearSolver: diagonal matrix has zero diagonal element");
      }
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizeLLT() {
    llt_ = std::make_unique<Eigen::LLT<EDMatrix<T>>>(eigen_A_);

    if (llt_->info() != Eigen::Success) {
      // Cholesky failed, fall back to LDLT
      selected_method_ = SolverMethod::kLDLT;
      FactorizeLDLT();
      return;
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizeLDLT() {
    ldlt_ = std::make_unique<Eigen::LDLT<EDMatrix<T>>>(eigen_A_);

    if (ldlt_->info() != Eigen::Success) {
      // LDLT failed, fall back to LU
      selected_method_ = SolverMethod::kPartialPivLU;
      FactorizePartialPivLU();
      return;
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizePartialPivLU() {
    partial_piv_lu_ =
        std::make_unique<Eigen::PartialPivLU<EDMatrix<T>>>(eigen_A_);
    status_ = SolverStatus::kFactorized;
  }

  void FactorizeFullPivLU() {
    full_piv_lu_ = std::make_unique<Eigen::FullPivLU<EDMatrix<T>>>(eigen_A_);
    status_ = SolverStatus::kFactorized;
  }

  void FactorizeHouseholderQR() {
    householder_qr_ =
        std::make_unique<Eigen::HouseholderQR<EDMatrix<T>>>(eigen_A_);
    status_ = SolverStatus::kFactorized;
  }

  void FactorizeColPivHouseholderQR() {
    col_piv_qr_ =
        std::make_unique<Eigen::ColPivHouseholderQR<EDMatrix<T>>>(eigen_A_);
    status_ = SolverStatus::kFactorized;
  }

  // --------------------------------------------------------------------------
  // Sparse Factorization Methods
  // --------------------------------------------------------------------------

  void FactorizeSparseDiagonal() {
    diagonal_.resize(nrows_);
    for (int i = 0; i < nrows_; ++i) {
      diagonal_(i) = eigen_sparse_A_.coeff(i, i);
      if (std::abs(diagonal_(i)) < options_.zero_tol) {
        status_ = SolverStatus::kSingular;
        ASC_ABORT("LinearSolver: sparse diagonal has zero element");
      }
    }
    status_ = SolverStatus::kFactorized;
  }

  void DetectSparseTriangularType() {
    // Determine if lower or upper triangular
    is_lower_triangular_ = true;
    for (int k = 0; k < eigen_sparse_A_.outerSize(); ++k) {
      for (typename ESpMatrix<T>::InnerIterator it(eigen_sparse_A_, k); it;
           ++it) {
        if (it.row() < it.col()) {
          is_lower_triangular_ = false;
          return;
        }
      }
    }
  }

  void FactorizeSimplicialLLT() {
    simplicial_llt_ = std::make_unique<Eigen::SimplicialLLT<ESpMatrix<T>>>();
    simplicial_llt_->compute(eigen_sparse_A_);

    if (simplicial_llt_->info() != Eigen::Success) {
      // Fall back to LDLT
      selected_method_ = SolverMethod::kSimplicialLDLT;
      FactorizeSimplicialLDLT();
      return;
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizeSimplicialLDLT() {
    simplicial_ldlt_ = std::make_unique<SpLDLT<T>>();
    simplicial_ldlt_->compute(eigen_sparse_A_);

    if (simplicial_ldlt_->info() != Eigen::Success) {
      // Fall back to SparseLU
      selected_method_ = SolverMethod::kSparseLU;
      FactorizeSparseLU();
      return;
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizeSparseLU() {
    sparse_lu_ = std::make_unique<SpLU<T>>();
    sparse_lu_->compute(eigen_sparse_A_);

    if (sparse_lu_->info() != Eigen::Success) {
      status_ = SolverStatus::kFailed;
      ASC_ABORT("LinearSolver: SparseLU factorization failed");
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizeCG() {
    cg_solver_ = std::make_unique<SpCG<T>>();
    cg_solver_->setTolerance(options_.iterative_tol);
    cg_solver_->setMaxIterations(options_.max_iterations);
    cg_solver_->compute(eigen_sparse_A_);

    if (cg_solver_->info() != Eigen::Success) {
      status_ = SolverStatus::kFailed;
      ASC_ABORT("LinearSolver: CG setup failed");
    }

    status_ = SolverStatus::kFactorized;
  }

  void FactorizeBiCGSTAB() {
    bicgstab_solver_ = std::make_unique<SpBiCGStab<T>>();
    bicgstab_solver_->setTolerance(options_.iterative_tol);
    bicgstab_solver_->setMaxIterations(options_.max_iterations);
    bicgstab_solver_->compute(eigen_sparse_A_);

    if (bicgstab_solver_->info() != Eigen::Success) {
      status_ = SolverStatus::kFailed;
      ASC_ABORT("LinearSolver: BiCGSTAB setup failed");
    }

    status_ = SolverStatus::kFactorized;
  }

  // --------------------------------------------------------------------------
  // Matrix Property Detection (Dense)
  // --------------------------------------------------------------------------

  bool IsDiagonal(const EDMatrix<T>& A) const {
    const int n = static_cast<int>(A.rows());
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        if (i != j && std::abs(A(i, j)) > options_.zero_tol) {
          return false;
        }
      }
    }
    return true;
  }

  bool IsLowerTriangular(const EDMatrix<T>& A) const {
    const int n = static_cast<int>(A.rows());
    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        if (std::abs(A(i, j)) > options_.zero_tol) {
          return false;
        }
      }
    }
    return true;
  }

  bool IsUpperTriangular(const EDMatrix<T>& A) const {
    const int n = static_cast<int>(A.rows());
    for (int j = 0; j < n; ++j) {
      for (int i = j + 1; i < n; ++i) {
        if (std::abs(A(i, j)) > options_.zero_tol) {
          return false;
        }
      }
    }
    return true;
  }

  bool IsSymmetric(const EDMatrix<T>& A) const {
    const int n = static_cast<int>(A.rows());
    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        T diff = std::abs(A(i, j) - A(j, i));
        T scale = std::abs(A(i, j)) + std::abs(A(j, i)) + 1;
        if (diff > options_.symmetry_tol * scale) {
          return false;
        }
      }
    }
    return true;
  }

  // --------------------------------------------------------------------------
  // Matrix Property Detection (Sparse)
  // --------------------------------------------------------------------------

  bool IsSparseDiagonal(const ESpMatrix<T>& A) const {
    for (int k = 0; k < A.outerSize(); ++k) {
      for (typename ESpMatrix<T>::InnerIterator it(A, k); it; ++it) {
        if (it.row() != it.col() && std::abs(it.value()) > options_.zero_tol) {
          return false;
        }
      }
    }
    return true;
  }

  bool IsSparseTriangular(const ESpMatrix<T>& A) const {
    bool has_upper = false;
    bool has_lower = false;

    for (int k = 0; k < A.outerSize(); ++k) {
      for (typename ESpMatrix<T>::InnerIterator it(A, k); it; ++it) {
        if (std::abs(it.value()) > options_.zero_tol) {
          if (it.row() < it.col()) {
            has_upper = true;
          }
          if (it.row() > it.col()) {
            has_lower = true;
          }
        }
        if (has_upper && has_lower) {
          return false;
        }
      }
    }

    return true;  // Either upper or lower triangular
  }

  bool IsSparseSymmetric(const ESpMatrix<T>& A) const {
    // Sample-based symmetry check for efficiency
    const int n = static_cast<int>(A.rows());
    const int sample_size = std::min(100, n);

    for (int i = 0; i < sample_size; ++i) {
      int row = i;
      int col = (i + n / 2) % n;
      if (row != col) {
        T aij = A.coeff(row, col);
        T aji = A.coeff(col, row);
        T diff = std::abs(aij - aji);
        T scale = std::abs(aij) + std::abs(aji) + 1;
        if (diff > options_.symmetry_tol * scale) {
          return false;
        }
      }
    }
    return true;
  }

  bool HasPositiveDiagonal(const ESpMatrix<T>& A) const {
    const int n = static_cast<int>(A.rows());
    for (int i = 0; i < n; ++i) {
      if (A.coeff(i, i) <= 0) {
        return false;
      }
    }
    return true;
  }

#endif  // ASC_USE_EIGEN

  // --------------------------------------------------------------------------
  // Member Data
  // --------------------------------------------------------------------------

  LinearSolverOptions options_;
  SolverStatus status_ = SolverStatus::kUninitialized;
  SolverMethod selected_method_ = SolverMethod::kAuto;

  int nrows_ = 0;
  int ncols_ = 0;
  int nnz_ = 0;
  bool is_sparse_ = false;
  bool is_square_ = false;
  bool is_lower_triangular_ = true;

#ifndef ASC_USE_EIGEN
  Matrix fallback_A_;
#endif

#ifdef ASC_USE_EIGEN
  // Eigen matrices (stored after conversion)
  EDMatrix<T> eigen_A_;
  // Sparse matrix storage for zero-copy access
  // Stored as ESpMatrix<T> since solvers may copy internally anyway
  ESpMatrix<T> eigen_sparse_A_;
  // Raw pointers for zero-copy Map reconstruction
  const int* sparse_outer_ptr_ = nullptr;
  const int* sparse_inner_ptr_ = nullptr;
  const T* sparse_values_ptr_ = nullptr;

  // Dense solvers
  std::unique_ptr<Eigen::LLT<EDMatrix<T>>> llt_;
  std::unique_ptr<Eigen::LDLT<EDMatrix<T>>> ldlt_;
  std::unique_ptr<Eigen::PartialPivLU<EDMatrix<T>>> partial_piv_lu_;
  std::unique_ptr<Eigen::FullPivLU<EDMatrix<T>>> full_piv_lu_;
  std::unique_ptr<Eigen::HouseholderQR<EDMatrix<T>>> householder_qr_;
  std::unique_ptr<Eigen::ColPivHouseholderQR<EDMatrix<T>>> col_piv_qr_;

  // Sparse solvers
  std::unique_ptr<Eigen::SimplicialLLT<ESpMatrix<T>>> simplicial_llt_;
  std::unique_ptr<SpLDLT<T>> simplicial_ldlt_;
  std::unique_ptr<SpLU<T>> sparse_lu_;
  std::unique_ptr<SpCG<T>> cg_solver_;
  std::unique_ptr<SpBiCGStab<T>> bicgstab_solver_;

  // Cached data for diagonal/triangular solves
  EDVector<T> diagonal_;
  Eigen::PermutationMatrix<Eigen::Dynamic> perm_;
#endif
};

// ============================================================================
// Convenience Functions
// ============================================================================

namespace internal {

template <typename Array>
inline constexpr bool kResizableLapackArray =
    DenseMArrayLike<Array> && Array::ShapeType::IsDynamic() &&
    !std::is_same_v<typename Array::LayoutType, LayoutStride>;

template <DenseTensorLike Vector>
inline void VerifyLapackVectorSize(const Vector& x, int size,
                                   const char* name) {
  if constexpr (Vector::GetRank() != 1) {
    ASC_VERIFY(false, name << " must be a vector");
  } else {
    ASC_VERIFY(x.GetSize() == size,
                  name << " has size " << x.GetSize() << ", expected "
                       << size);
  }
}

template <DenseTensorLike Vector>
inline void EnsureLapackVectorSize(Vector& x, int size, const char* name) {
  if constexpr (Vector::GetRank() != 1) {
    ASC_VERIFY(false, name << " must be a vector");
  } else if (x.GetSize() != size) {
    if constexpr (kResizableLapackArray<Vector>) {
      x.SetShape(DShape<1>(size));
    } else {
      ASC_VERIFY(false,
                    name << " has size " << x.GetSize() << ", expected "
                         << size);
    }
  }
}

template <DenseTensorLike Matrix>
inline void VerifyLapackMatrixSize(const Matrix& A, int rows, int cols,
                                   const char* name) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, name << " must be a matrix");
  } else {
    ASC_VERIFY(A.GetExtent(0) == rows && A.GetExtent(1) == cols,
                  name << " has shape (" << A.GetExtent(0) << ", "
                       << A.GetExtent(1) << "), expected (" << rows << ", "
                       << cols << ")");
  }
}

template <DenseTensorLike Matrix>
inline void EnsureLapackMatrixSize(Matrix& A, int rows, int cols,
                                   const char* name) {
  if constexpr (Matrix::GetRank() != 2) {
    ASC_VERIFY(false, name << " must be a matrix");
  } else if (A.GetExtent(0) != rows || A.GetExtent(1) != cols) {
    if constexpr (kResizableLapackArray<Matrix>) {
      A.SetShape(DShape<2>(rows, cols));
    } else {
      ASC_VERIFY(false,
                    name << " has shape (" << A.GetExtent(0) << ", "
                         << A.GetExtent(1) << "), expected (" << rows << ", "
                         << cols << ")");
    }
  }
}

}  // namespace internal

/// @brief LU factorization with partial pivoting.
///
/// Stores L and U packed in A and writes the row permutation to ipiv.
/// Returns 0 on success, or a positive info value if the matrix is singular.
template <DenseMatrixLike Matrix, DenseVectorLike Pivot>
int Getrf(Matrix& A, Pivot& ipiv) {
  const int n = A.GetExtent(0);
  internal::VerifyLapackMatrixSize(A, n, n, "A");
  internal::EnsureLapackVectorSize(ipiv, n, "ipiv");
  return LUP(A, ipiv) ? 0 : 1;
}

/// @brief Solve a system using packed LU factors from Getrf.
template <DenseMatrixLike Matrix, DenseVectorLike Pivot, DenseTensorLike Rhs,
          DenseTensorLike Sol>
int Getrs(const Matrix& lu, const Pivot& ipiv, const Rhs& B, Sol& X,
          TransposeMode trans = TransposeMode::kNoTranspose) {
  ASC_VERIFY(trans == TransposeMode::kNoTranspose,
                "Getrs currently supports only kNoTranspose");
  const int n = lu.GetExtent(0);
  internal::VerifyLapackMatrixSize(lu, n, n, "lu");
  internal::VerifyLapackVectorSize(ipiv, n, "ipiv");

  using T = typename Matrix::ElementType;
  if constexpr (Rhs::GetRank() == 1 && Sol::GetRank() == 1) {
    internal::VerifyLapackVectorSize(B, n, "B");
    internal::EnsureLapackVectorSize(X, n, "X");
    LUPSolve(lu, ipiv, B, X);
  } else if constexpr (Rhs::GetRank() == 2 && Sol::GetRank() == 2) {
    const int nrhs = B.GetExtent(1);
    internal::VerifyLapackMatrixSize(B, n, nrhs, "B");
    internal::EnsureLapackMatrixSize(X, n, nrhs, "X");

    const auto& b_map = B.GetMap();
    const auto& x_map = X.GetMap();
    const auto* b_data = B.HostRead();
    auto* x_data = X.HostWrite();
    DVector<T> b_col(n);
    DVector<T> x_col(n);
    for (int rhs = 0; rhs < nrhs; ++rhs) {
      auto* b_col_data = b_col.HostWrite();
      for (int i = 0; i < n; ++i) {
        b_col_data[i] = b_data[b_map(i, rhs)];
      }
      LUPSolve(lu, ipiv, b_col, x_col);
      const auto* x_col_data = x_col.HostRead();
      for (int i = 0; i < n; ++i) {
        x_data[x_map(i, rhs)] = x_col_data[i];
      }
    }
  } else {
    ASC_VERIFY(false, "Getrs requires vector or matrix right-hand sides");
  }
  return 0;
}

/// @brief One-shot LU solve.
template <DenseMatrixLike Matrix, DenseTensorLike Rhs, DenseTensorLike Sol,
          DenseVectorLike Pivot>
int Gesv(const Matrix& A, const Rhs& B, Sol& X, Pivot& ipiv) {
  Matrix lu(A);
  const int info = Getrf(lu, ipiv);
  if (info != 0) return info;
  return Getrs(lu, ipiv, B, X);
}

/// @brief Compute matrix inverse from packed LU factors.
template <DenseMatrixLike Matrix, DenseVectorLike Pivot>
int Getri(const Matrix& lu, const Pivot& ipiv, Matrix& inverse) {
  const int n = lu.GetExtent(0);
  internal::VerifyLapackMatrixSize(lu, n, n, "lu");
  internal::VerifyLapackVectorSize(ipiv, n, "ipiv");
  internal::EnsureLapackMatrixSize(inverse, n, n, "inverse");

  using T = typename Matrix::ElementType;
  DMatrix<T> identity(n, n);
  const auto& id_map = identity.GetMap();
  auto* id_data = identity.HostWrite();
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      id_data[id_map(i, j)] = (i == j) ? T(1) : T(0);
    }
  }
  return Getrs(lu, ipiv, identity, inverse);
}

/// @brief Cholesky factorization A = L * L^T.
template <DenseMatrixLike Matrix>
int Potrf(const Matrix& A, Matrix& L,
          TriangleMode uplo = TriangleMode::kLower) {
  ASC_VERIFY(uplo == TriangleMode::kLower,
                "Potrf currently writes the lower Cholesky factor");
  const int n = A.GetExtent(0);
  internal::VerifyLapackMatrixSize(A, n, n, "A");
  internal::EnsureLapackMatrixSize(L, n, n, "L");
  return Cholesky(A, L) ? 0 : 1;
}

/// @brief Solve a SPD system using a Cholesky factor from Potrf.
template <DenseMatrixLike Matrix, DenseTensorLike Rhs, DenseTensorLike Sol>
int Potrs(const Matrix& L, const Rhs& B, Sol& X,
          TriangleMode uplo = TriangleMode::kLower) {
  ASC_VERIFY(uplo == TriangleMode::kLower,
                "Potrs currently expects a lower Cholesky factor");
  const int n = L.GetExtent(0);
  internal::VerifyLapackMatrixSize(L, n, n, "L");

  using T = typename Matrix::ElementType;
  if constexpr (Rhs::GetRank() == 1 && Sol::GetRank() == 1) {
    internal::VerifyLapackVectorSize(B, n, "B");
    internal::EnsureLapackVectorSize(X, n, "X");
    CholeskySolve(L, B, X);
  } else if constexpr (Rhs::GetRank() == 2 && Sol::GetRank() == 2) {
    const int nrhs = B.GetExtent(1);
    internal::VerifyLapackMatrixSize(B, n, nrhs, "B");
    internal::EnsureLapackMatrixSize(X, n, nrhs, "X");

    const auto& b_map = B.GetMap();
    const auto& x_map = X.GetMap();
    const auto* b_data = B.HostRead();
    auto* x_data = X.HostWrite();
    DVector<T> b_col(n);
    DVector<T> x_col(n);
    for (int rhs = 0; rhs < nrhs; ++rhs) {
      auto* b_col_data = b_col.HostWrite();
      for (int i = 0; i < n; ++i) {
        b_col_data[i] = b_data[b_map(i, rhs)];
      }
      CholeskySolve(L, b_col, x_col);
      const auto* x_col_data = x_col.HostRead();
      for (int i = 0; i < n; ++i) {
        x_data[x_map(i, rhs)] = x_col_data[i];
      }
    }
  } else {
    ASC_VERIFY(false, "Potrs requires vector or matrix right-hand sides");
  }
  return 0;
}

/// @brief One-shot Cholesky solve.
template <DenseMatrixLike Matrix, DenseTensorLike Rhs, DenseTensorLike Sol>
int Posv(const Matrix& A, const Rhs& B, Sol& X, Matrix& L,
         TriangleMode uplo = TriangleMode::kLower) {
  const int info = Potrf(A, L, uplo);
  if (info != 0) return info;
  return Potrs(L, B, X, uplo);
}

/// @brief Compute SPD matrix inverse from a Cholesky factor.
template <DenseMatrixLike Matrix>
int Potri(const Matrix& L, Matrix& inverse,
          TriangleMode uplo = TriangleMode::kLower) {
  ASC_VERIFY(uplo == TriangleMode::kLower,
                "Potri currently expects a lower Cholesky factor");
  const int n = L.GetExtent(0);
  internal::VerifyLapackMatrixSize(L, n, n, "L");
  internal::EnsureLapackMatrixSize(inverse, n, n, "inverse");

  using T = typename Matrix::ElementType;
  DMatrix<T> identity(n, n);
  const auto& id_map = identity.GetMap();
  auto* id_data = identity.HostWrite();
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      id_data[id_map(i, j)] = (i == j) ? T(1) : T(0);
    }
  }
  return Potrs(L, identity, inverse, uplo);
}

/// @brief QR factorization A = Q * R.
template <DenseMatrixLike Matrix>
int Geqrf(const Matrix& A, Matrix& Q, Matrix& R) {
  const int m = A.GetExtent(0);
  const int n = A.GetExtent(1);
  ASC_VERIFY(m >= n, "Geqrf currently requires rows >= columns");
  internal::EnsureLapackMatrixSize(Q, m, m, "Q");
  internal::EnsureLapackMatrixSize(R, m, n, "R");
  QR(A, Q, R);
  return 0;
}

/// @brief Least-squares solve using QR factorization.
template <DenseMatrixLike Matrix, DenseTensorLike Rhs, DenseTensorLike Sol>
int Gels(const Matrix& A, const Rhs& B, Sol& X) {
  const int m = A.GetExtent(0);
  const int n = A.GetExtent(1);
  ASC_VERIFY(m >= n, "Gels currently requires rows >= columns");

  using T = typename Matrix::ElementType;
  DMatrix<T> Q(m, m);
  DMatrix<T> R(m, n);
  Geqrf(A, Q, R);

  if constexpr (Rhs::GetRank() == 1 && Sol::GetRank() == 1) {
    internal::VerifyLapackVectorSize(B, m, "B");
    internal::EnsureLapackVectorSize(X, n, "X");
    QRSolve(Q, R, B, X);
  } else if constexpr (Rhs::GetRank() == 2 && Sol::GetRank() == 2) {
    const int nrhs = B.GetExtent(1);
    internal::VerifyLapackMatrixSize(B, m, nrhs, "B");
    internal::EnsureLapackMatrixSize(X, n, nrhs, "X");

    const auto& b_map = B.GetMap();
    const auto& x_map = X.GetMap();
    const auto* b_data = B.HostRead();
    auto* x_data = X.HostWrite();
    DVector<T> b_col(m);
    DVector<T> x_col(n);
    for (int rhs = 0; rhs < nrhs; ++rhs) {
      auto* b_col_data = b_col.HostWrite();
      for (int i = 0; i < m; ++i) {
        b_col_data[i] = b_data[b_map(i, rhs)];
      }
      QRSolve(Q, R, b_col, x_col);
      const auto* x_col_data = x_col.HostRead();
      for (int i = 0; i < n; ++i) {
        x_data[x_map(i, rhs)] = x_col_data[i];
      }
    }
  } else {
    ASC_VERIFY(false, "Gels requires vector or matrix right-hand sides");
  }
  return 0;
}

#ifdef ASC_USE_EIGEN

/// @brief Singular value decomposition A = U * diag(s) * Vt.
template <DenseMatrixLike Matrix, DenseVectorLike Vector>
int Gesvd(const Matrix& A, Matrix& U, Vector& s, Matrix& Vt) {
  const int m = A.GetExtent(0);
  const int n = A.GetExtent(1);
  const int k = std::min(m, n);
  internal::EnsureLapackMatrixSize(U, m, k, "U");
  internal::EnsureLapackVectorSize(s, k, "s");
  internal::EnsureLapackMatrixSize(Vt, k, n, "Vt");
  SVD(A, U, s, Vt);
  return 0;
}

/// @brief Symmetric eigenvalue decomposition.
template <DenseMatrixLike Matrix, DenseVectorLike Vector>
int Syev(const Matrix& A, Vector& values, Matrix& vectors) {
  const int n = A.GetExtent(0);
  internal::VerifyLapackMatrixSize(A, n, n, "A");
  internal::EnsureLapackVectorSize(values, n, "values");
  internal::EnsureLapackMatrixSize(vectors, n, n, "vectors");
  EigH(A, values, vectors);
  return 0;
}

/// @brief General real eigenvalue decomposition.
template <DenseMatrixLike Matrix, DenseVectorLike Vector>
int Geev(const Matrix& A, Vector& values_re, Vector& values_im,
         Matrix& vectors_re, Matrix& vectors_im) {
  const int n = A.GetExtent(0);
  internal::VerifyLapackMatrixSize(A, n, n, "A");
  internal::EnsureLapackVectorSize(values_re, n, "values_re");
  internal::EnsureLapackVectorSize(values_im, n, "values_im");
  internal::EnsureLapackMatrixSize(vectors_re, n, n, "vectors_re");
  internal::EnsureLapackMatrixSize(vectors_im, n, n, "vectors_im");
  Eig(A, values_re, values_im, vectors_re, vectors_im);
  return 0;
}

#endif  // ASC_USE_EIGEN

/// @brief Compute determinant of a dense square matrix.
template <typename T>
T Determinant(DMatrix<T> A,
              T tol = Sqrt(std::numeric_limits<T>::epsilon())) {
  ASC_VERIFY(A.GetRank() == 2 && A.GetExtent(0) == A.GetExtent(1),
                "Determinant requires a square matrix");
  const int n = A.GetExtent(0);
  const auto& a_map = A.GetMap();
  T* a_data = A.HostReadWrite();
  T det = T(1);
  int sign = 1;

  for (int k = 0; k < n; ++k) {
    int pivot = k;
    T pivot_abs = std::abs(a_data[a_map(k, k)]);
    for (int i = k + 1; i < n; ++i) {
      const T value = std::abs(a_data[a_map(i, k)]);
      if (value > pivot_abs) {
        pivot_abs = value;
        pivot = i;
      }
    }
    if (pivot_abs <= tol) return T(0);
    if (pivot != k) {
      for (int j = k; j < n; ++j) {
        std::swap(a_data[a_map(k, j)], a_data[a_map(pivot, j)]);
      }
      sign = -sign;
    }
    const T pivot_value = a_data[a_map(k, k)];
    det *= pivot_value;
    for (int i = k + 1; i < n; ++i) {
      const T factor = a_data[a_map(i, k)] / pivot_value;
      a_data[a_map(i, k)] = T(0);
      for (int j = k + 1; j < n; ++j) {
        a_data[a_map(i, j)] -= factor * a_data[a_map(k, j)];
      }
    }
  }
  return sign > 0 ? det : -det;
}

/// @brief Eigenvalues and eigenvectors of a real symmetric matrix.
///
/// Eigenvectors are stored as columns. Eigenvalues are sorted in ascending
/// order, matching Eigen's SelfAdjointEigenSolver convention.
template <FloatingPoint T>
struct SymmetricEigenResult {
  DVector<T> values;
  DMatrix<T> vectors;
};

/// @brief Compute the eigendecomposition of a real symmetric matrix.
template <FloatingPoint T>
SymmetricEigenResult<T> SymmetricEigenDecompose(
    const DMatrix<T>& A, T tol = T(64) * std::numeric_limits<T>::epsilon()) {
  ASC_VERIFY(A.GetRank() == 2 && A.GetExtent(0) == A.GetExtent(1),
                "SymmetricEigenDecompose requires a square matrix");

  const int n = A.GetExtent(0);
  const auto& a_map = A.GetMap();
  const T* a_data = A.HostRead();
  for (int j = 0; j < n; ++j) {
    for (int i = j + 1; i < n; ++i) {
      ASC_VERIFY(std::abs(a_data[a_map(i, j)] - a_data[a_map(j, i)]) <= tol,
                    "SymmetricEigenDecompose requires a symmetric matrix");
    }
  }

  SymmetricEigenResult<T> result;

#ifdef ASC_USE_EIGEN
  EDMatrix<T> eigen_A;
  DMatrixToEigen(A, eigen_A);
  Eigen::SelfAdjointEigenSolver<EDMatrix<T>> solver(eigen_A);
  ASC_VERIFY(solver.info() == Eigen::Success,
                "Symmetric eigendecomposition failed");

  EDVector<T> eigen_values = solver.eigenvalues();
  EDMatrix<T> eigen_vectors = solver.eigenvectors();
  DVectorFromEigen(eigen_values, result.values);
  DMatrixFromEigen(eigen_vectors, result.vectors);
#else
  DMatrix<T> work(n, n);
  work.CopyFrom(A);
  result.values.SetShape(DShape<1>(n));
  result.vectors.SetShape(DShape<2>(n, n));

  T* work_data = work.HostReadWrite();
  T* vector_data = result.vectors.HostWrite();
  const auto& work_map = work.GetMap();
  const auto& vector_map = result.vectors.GetMap();
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      vector_data[vector_map(i, j)] = (i == j) ? T(1) : T(0);
    }
  }

  const int max_iterations = std::max(1, 64 * n * n);
  for (int iter = 0; iter < max_iterations; ++iter) {
    int p = 0;
    int q = 1;
    T max_offdiag = (n > 1) ? std::abs(work_data[work_map(0, 1)]) : T(0);
    for (int j = 0; j < n; ++j) {
      for (int i = j + 1; i < n; ++i) {
        const T value = std::abs(work_data[work_map(i, j)]);
        if (value > max_offdiag) {
          max_offdiag = value;
          p = j;
          q = i;
        }
      }
    }
    if (max_offdiag <= tol || n <= 1) break;

    const T app = work_data[work_map(p, p)];
    const T aqq = work_data[work_map(q, q)];
    const T apq = work_data[work_map(p, q)];
    const T tau = (aqq - app) / (T(2) * apq);
    const T sign = tau >= T(0) ? T(1) : T(-1);
    const T t = sign / (std::abs(tau) + std::sqrt(T(1) + tau * tau));
    const T c = T(1) / std::sqrt(T(1) + t * t);
    const T s = t * c;

    for (int k = 0; k < n; ++k) {
      if (k == p || k == q) continue;
      const T akp = work_data[work_map(k, p)];
      const T akq = work_data[work_map(k, q)];
      const T new_kp = c * akp - s * akq;
      const T new_kq = s * akp + c * akq;
      work_data[work_map(k, p)] = new_kp;
      work_data[work_map(p, k)] = new_kp;
      work_data[work_map(k, q)] = new_kq;
      work_data[work_map(q, k)] = new_kq;
    }

    work_data[work_map(p, p)] = c * c * app - T(2) * s * c * apq + s * s * aqq;
    work_data[work_map(q, q)] = s * s * app + T(2) * s * c * apq + c * c * aqq;
    work_data[work_map(p, q)] = T(0);
    work_data[work_map(q, p)] = T(0);

    for (int k = 0; k < n; ++k) {
      const T vkp = vector_data[vector_map(k, p)];
      const T vkq = vector_data[vector_map(k, q)];
      vector_data[vector_map(k, p)] = c * vkp - s * vkq;
      vector_data[vector_map(k, q)] = s * vkp + c * vkq;
    }
  }

  T* value_data = result.values.HostWrite();
  for (int i = 0; i < n; ++i) value_data[i] = work_data[work_map(i, i)];
  for (int i = 0; i < n - 1; ++i) {
    int min_index = i;
    for (int j = i + 1; j < n; ++j) {
      if (value_data[j] < value_data[min_index]) min_index = j;
    }
    if (min_index == i) continue;
    std::swap(value_data[i], value_data[min_index]);
    for (int row = 0; row < n; ++row) {
      std::swap(vector_data[vector_map(row, i)],
                vector_data[vector_map(row, min_index)]);
    }
  }
#endif

  return result;
}

/// @brief Solve a dense linear system Ax = b (one-shot)
template <typename T>
void LinearSolve(const DMatrix<T>& A, const DVector<T>& b, DVector<T>& x) {
  LinearSolver<T> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);
}

/// @brief Solve a sparse linear system Ax = b (one-shot)
template <typename T>
void LinearSolve(const SpDMatrix<T>& A, const DVector<T>& b, DVector<T>& x) {
  LinearSolver<T> solver;
  solver.SetOperator(A);
  solver.LinearSolve(b, x);
}

/// @brief Solve and return solution (dense)
template <typename T>
DVector<T> LinearSolve(const DMatrix<T>& A, const DVector<T>& b) {
  LinearSolver<T> solver;
  solver.SetOperator(A);
  return solver.LinearSolve(b);
}

/// @brief Solve and return solution (sparse)
template <typename T>
DVector<T> LinearSolve(const SpDMatrix<T>& A, const DVector<T>& b) {
  LinearSolver<T> solver;
  solver.SetOperator(A);
  return solver.LinearSolve(b);
}

}  // namespace asc

#endif  // ASC_LAPACK_H_
