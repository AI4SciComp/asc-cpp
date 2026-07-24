// ============================================================================
// Copyright (C) 2025, Xiamen University, School of Mathematical Sciences.
// All rights reserved. See files LICENSE for details.
//
// File: ASC/algebra/eigen.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_EIGEN_H_
#define ASC_EIGEN_H_

#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include "asc/core/globals.h"
#include "asc/core/error.h"
#include "asc/array/marray.h"
#include "asc/array/spmarray.h"

#ifdef ASC_USE_MKL
#define EIGEN_USE_MKL_ALL
#include "mkl.h"
#ifdef ASC_MKL_USE_parallel
#include <Eigen/PardisoSupport>
#endif
#endif
#include <Eigen/Dense>
#include <Eigen/Sparse>

// Eigen Matrix/SparseMatrix IO (binary version)
namespace Eigen {

// https://stackoverflow.com/a/25389481/11927397
template <typename Matrix>
void WriteDenseMatrix(std::string filename, const Matrix& matrix) {
  std::ofstream out(filename,
                    std::ios::out | std::ios::binary | std::ios::trunc);
  if (out.is_open()) {
    typename Matrix::Index rows = matrix.rows(), cols = matrix.cols();
    out.write(reinterpret_cast<char*>(&rows), sizeof(typename Matrix::Index));
    out.write(reinterpret_cast<char*>(&cols), sizeof(typename Matrix::Index));
    out.write(
        reinterpret_cast<const char*>(matrix.data()),
        rows * cols *
            static_cast<typename Matrix::Index>(sizeof(typename Matrix::T)));
    out.close();
  } else {
    ASC_ABORT("Can not write to file: " << filename);
  }
}

template <typename Matrix>
void ReadDenseMatrix(std::string filename, Matrix& matrix) {
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in.is_open()) {
    typename Matrix::Index rows = 0, cols = 0;
    in.read(reinterpret_cast<char*>(&rows), sizeof(typename Matrix::Index));
    in.read(reinterpret_cast<char*>(&cols), sizeof(typename Matrix::Index));
    matrix->resize(rows, cols);
    in.read(
        reinterpret_cast<char*>(matrix.data()),
        rows * cols *
            static_cast<typename Matrix::Index>(sizeof(typename Matrix::T)));
    in.close();
  } else {
    ASC_ABORT("Can not open binary matrix file: " << filename);
  }
}

// https://scicomp.stackexchange.com/a/21438
template <typename SparseMatrix>
void WriteSparseMatrix(std::string filename, const SparseMatrix& matrix) {
  assert(matrix.isCompressed() == true);

  std::ofstream out(filename,
                    std::ios::binary | std::ios::out | std::ios::trunc);
  if (out.is_open()) {
    typename SparseMatrix::Index rows, cols, nnzs, outS, innS;
    typename SparseMatrix::Index size_index =
        static_cast<typename SparseMatrix::Index>(
            sizeof(typename SparseMatrix::Index));
    typename SparseMatrix::Index size_scalar =
        static_cast<typename SparseMatrix::Index>(
            sizeof(typename SparseMatrix::T));
    typename SparseMatrix::Index size_storage_index =
        static_cast<typename SparseMatrix::Index>(
            sizeof(typename SparseMatrix::StorageIndex));

    rows = matrix.rows();
    cols = matrix.cols();
    nnzs = matrix.nonZeros();
    outS = matrix.outerSize();
    innS = matrix.innerSize();

    out.write(reinterpret_cast<const char*>(&rows), size_index);
    out.write(reinterpret_cast<const char*>(&cols), size_index);
    out.write(reinterpret_cast<const char*>(&nnzs), size_index);
    out.write(reinterpret_cast<const char*>(&outS), size_index);
    out.write(reinterpret_cast<const char*>(&innS), size_index);
    out.write(reinterpret_cast<const char*>(matrix.valuePtr()),
              size_scalar * nnzs);
    out.write(reinterpret_cast<const char*>(matrix.outerIndexPtr()),
              size_storage_index * (outS + 1));
    out.write(reinterpret_cast<const char*>(matrix.innerIndexPtr()),
              size_storage_index * nnzs);

    out.close();
  } else {
    ASC_ABORT("Can not write to file: " << filename);
  }
}

template <typename SparseMatrix>
void ReadSparseMatrix(std::string filename, SparseMatrix& matrix) {
  // Bug: When the number of nonzero is 1,
  //      and it locates at the right bottom corner of the matrix, this routine
  //      fails!!!
  std::ifstream in(filename, std::ios::binary | std::ios::in);
  if (in.is_open()) {
    typename SparseMatrix::Index rows, cols, nnz, outS, innS;
    typename SparseMatrix::Index size_index =
        static_cast<typename SparseMatrix::Index>(
            sizeof(typename SparseMatrix::Index));
    typename SparseMatrix::Index size_scalar =
        static_cast<typename SparseMatrix::Index>(
            sizeof(typename SparseMatrix::T));
    typename SparseMatrix::Index size_scalar_index =
        static_cast<typename SparseMatrix::Index>(
            sizeof(typename SparseMatrix::StorageIndex));

    in.read(reinterpret_cast<char*>(&rows), size_index);
    in.read(reinterpret_cast<char*>(&cols), size_index);
    in.read(reinterpret_cast<char*>(&nnz), size_index);
    in.read(reinterpret_cast<char*>(&outS), size_index);
    in.read(reinterpret_cast<char*>(&innS), size_index);

    matrix.resize(rows, cols);
    matrix.makeCompressed();
    matrix.resizeNonZeros(nnz);

    in.read(reinterpret_cast<char*>(matrix.valuePtr()), size_scalar * nnz);
    in.read(reinterpret_cast<char*>(matrix.outerIndexPtr()),
            size_scalar_index * (outS + 1));
    in.read(reinterpret_cast<char*>(matrix.innerIndexPtr()),
            size_scalar_index * nnz);

    matrix.finalize();
    in.close();
  } else {
    ASC_ABORT("Can not open binary sparse matrix file: " << filename);
  }
}

}  // namespace Eigen

namespace asc {

// ============================================================================
// Eigen Type Aliases
// ============================================================================

/// @brief Default Eigen storage order based on ASC's DefaultLayout
/// LayoutLeft (column-major) → Eigen::ColMajor
/// LayoutRight (row-major) → Eigen::RowMajor
constexpr int kDefaultEigenOptions = std::is_same_v<DefaultLayout, LayoutLeft>
                                         ? Eigen::ColMajor
                                         : Eigen::RowMajor;

/// @brief Eigen dynamic-size vector (column vector)
template <typename T, int Options = kDefaultEigenOptions>
using EDVector = Eigen::Matrix<T, Eigen::Dynamic, 1, Options>;

/// @brief Eigen static-size vector (column vector)
template <typename T, int Size, int Options = kDefaultEigenOptions>
using ESVector = Eigen::Matrix<T, Size, 1, Options>;

/// @brief Eigen dynamic-size matrix
template <typename T, int Options = kDefaultEigenOptions>
using EDMatrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Options>;

/// @brief Eigen static-size matrix
template <typename T, int Rows, int Cols, int Options = kDefaultEigenOptions>
using ESMatrix = Eigen::Matrix<T, Rows, Cols, Options>;

template <typename T, int Options = kDefaultEigenOptions>
using ESpMatrix = Eigen::SparseMatrix<T, Options>;

// sparse lapack
template <typename T>
using SpLU = Eigen::SparseLU<ESpMatrix<T>>;

template <typename T>
using SpLLT = Eigen::SimplicialLLT<ESpMatrix<T>>;

template <typename T>
using SpLDLT = Eigen::SimplicialLDLT<ESpMatrix<T>>;

template <typename T>
using SpCG = Eigen::ConjugateGradient<ESpMatrix<T>, Eigen::Upper>;

template <typename T>
using SpBiCGStab = Eigen::BiCGSTAB<ESpMatrix<T>, Eigen::IdentityPreconditioner>;

#ifdef MKL_USE_parallel

template <typename T>
using SpPardisoLU = Eigen::PardisoLU<SpMatrix<T>>;

template <typename T>
using SpPardisoLLT = Eigen::PardisoLDLT<SpMatrix<T>>;

template <typename T>
using SpPardisoLDLT = Eigen::PardisoLDLT<SpMatrix<T>>;

#endif

enum SpLinearSolverType {
  kNone = 0,      ///< empty
  kLU = 1,        ///< Eigen built-in: LU, Square
  kLLT = 2,       ///< Eigen built-in: LLt, SPD
  kLDLT = 3,      ///< Eigen built-in, SPD
  kCG = 4,        ///< Eigen built-in: conjugate gradient, SPD
  kBiCGStab = 5,  ///< Eigen built-in: stabilized bi-conjugate gradient, Square
#ifdef MKL_USE_parallel
  kPardisoLU = 6,    ///< Intel MKL Pardiso: LU, Square
  kPardisoLLT = 7,   ///< Intel MKL Pardiso: LLt, SPD
  kPardisoLDLT = 8,  ///< Intel MKL Pardiso: LDLt, SPD
#endif
};

enum SpLinearSolverStatus : unsigned {
  kInitialized = 1 << 0,  ///< solver is initialized
  kComputed = 1 << 1,     ///< matrix is factorized
};

template <typename T>
class SpLinearSolver {
 public:
  int type_;                  ///< solver type
  int status_;                ///< solver status
  ESpMatrix<T> lhs_;           ///< LHS matrix
  EDVector<T> rhs_;           ///< RHS vector
  EDVector<T> result_;        ///< result vector
  std::string lhs_tmp_file_;  ///< LHS temporary file
  std::string rhs_tmp_file_;  ///< RHS temporary file

  explicit SpLinearSolver(const int size, std::string lhs_tmp_file = "",
                          std::string rhs_tmp_file = "")
      : type_(kNone),
        status_(0),
        lhs_(size, size),
        rhs_(size),
        result_(size),
        lhs_tmp_file_(std::move(lhs_tmp_file)),
        rhs_tmp_file_(std::move(rhs_tmp_file)) {}

  virtual ~SpLinearSolver() = default;

  bool IsComputed() const { return status_ & kComputed; }
  bool IsInitialized() const { return status_ & kInitialized; }
  void SetType(int tp) { type_ = tp; }
  void SaveLHS() { Eigen::WriteSparseMatrix(lhs_tmp_file_.c_str(), &lhs_); }
  void SaveRHS() { Eigen::WriteDenseMatrix(rhs_tmp_file_.c_str(), &rhs_); }

  virtual void SetType() { throw std::runtime_error("undefined"); }
  virtual void Init() { throw std::runtime_error("undefined"); }
  virtual void Compute() { throw std::runtime_error("undefined"); }
  virtual void Solve() { throw std::runtime_error("undefined"); }

 protected:
  bool CheckSymmetric() { return lhs_.isApprox(lhs_.transpose()); }
  bool CheckPostiveDefinite() {
    // check diagonal elements first
    if (!(lhs_.diagonal().array() > 0.).any()) {
      return false;
    }
    // try cholesky decomposition
#ifndef MKL_USE_parallel
    SpLDLT<T> check;
#else
    SpPardisoLDLT<T> check;
#endif
    check.compute(lhs_);
    return (check.info() == Eigen::Success);
  }
};

// direct method
template <typename T>
class SpDirect : public SpLinearSolver<T> {
 public:
  std::unique_ptr<SpLU<T>> lu_;
  std::unique_ptr<SpLLT<T>> llt_;
  std::unique_ptr<SpLDLT<T>> ldlt_;

  explicit SpDirect(const int size) : SpLinearSolver<T>(size) {}

  ~SpDirect() override = default;

  void SetType() override;
  void Init() override;
  void Compute() override;
  void Solve() override;

 private:
  void ComputeLU();
  void ComputeLLT();
  void ComputeLDLT();
};

template <typename T>
void SpDirect<T>::SetType() {
  bool spd = (!this->CheckSymmetric()) ? false : this->CheckPostiveDefinite();
  this->type_ = (spd) ? kLDLT : kLU;
}

template <typename T>
void SpDirect<T>::Init() {
  if (this->IsInitialized()) return;

  switch (this->type_) {
    case kLU:
      lu_ = std::make_unique<SpLU<T>>();
      break;
    case kLLT:
      llt_ = std::make_unique<SpLLT<T>>();
      break;
    case kLDLT:
      ldlt_ = std::make_unique<SpLDLT<T>>();
      break;
    default:
      ASC_ABORT("invalid sparse linear solver type");
  }
  this->status_ |= kInitialized;
}

template <typename T>
void SpDirect<T>::Compute() {
  if (this->IsComputed()) return;

  switch (this->type_) {
    case kLU:
      ComputeLU();
      return;
    case kLLT:
      ComputeLLT();
      return;
    case kLDLT:
      ComputeLDLT();
      return;
  }
}

template <typename T>
void SpDirect<T>::Solve() {
  switch (this->type_) {
    case kLU:
      this->result_ = lu_->solve(this->rhs_);
      return;
    case kLLT:
      this->result_ = llt_->solve(this->rhs_);
      return;
    case kLDLT:
      this->result_ = ldlt_->solve(this->rhs_);
      return;
  }
}

template <typename T>
void SpDirect<T>::ComputeLU() {
  this->status_ |= kComputed;
  lu_->compute(this->lhs_);

  ASC_VERIFY(lu_->info() == Eigen::Success, "precomputation failed");
}

template <typename T>
void SpDirect<T>::ComputeLLT() {
  this->status_ |= kComputed;
  llt_->compute(this->lhs_);
  ASC_VERIFY(llt_->info() == Eigen::Success, "precomputation failed");
}

template <typename T>
void SpDirect<T>::ComputeLDLT() {
  this->status_ |= kComputed;
  ldlt_->compute(this->lhs_);
  ASC_VERIFY(ldlt_->info() == Eigen::Success, "precomputation failed");
}

// Krylov subspace method
template <typename T>
class SpKrylov : public SpLinearSolver<T> {
 public:
  std::unique_ptr<SpCG<T>> cg_;
  std::unique_ptr<SpBiCGStab<T>> bicg_;

  explicit SpKrylov(const int size) : SpLinearSolver<T>(size) {}

  ~SpKrylov() override = default;

  void SetType() override;
  void Init() override;
  void Compute() override;
  void Solve() override;

 private:
  void ComputeCG();
  void ComputeBiCGStab();
};

template <typename T>
void SpKrylov<T>::SetType() {
  bool spd = (!this->CheckSymmetric()) ? false : this->CheckPostiveDefinite();
  this->type_ = (spd) ? kCG : kBiCGStab;
}

template <typename T>
void SpKrylov<T>::Init() {
  if (this->IsInitialized()) return;

  switch (this->type_) {
    case kCG:
      cg_ = std::make_unique<SpCG<T>>();
      break;
    case kBiCGStab:
      bicg_ = std::make_unique<SpBiCGStab<T>>();
      break;
    default:
      ASC_ABORT("invalid sparse linear solver type");
  }
  this->status_ |= kInitialized;
}

template <typename T>
void SpKrylov<T>::Compute() {
  if (this->IsComputed()) return;

  switch (this->type_) {
    case kCG:
      ComputeCG();
      return;
    case kBiCGStab:
      ComputeBiCGStab();
      return;
  }
}

template <typename T>
void SpKrylov<T>::Solve() {
  switch (this->type_) {
    case kCG:
      this->result_ = cg_->solve(this->rhs_);
      return;
    case kBiCGStab:
      this->result_ = bicg_->solve(this->rhs_);
      return;
  }
}

template <typename T>
void SpKrylov<T>::ComputeCG() {
  this->status_ |= kComputed;
  cg_->compute(this->lhs_);
  if (cg_->info() != Eigen::Success) {
    throw std::runtime_error("precomputation failed");
  }
}

template <typename T>
void SpKrylov<T>::ComputeBiCGStab() {
  this->status_ |= kComputed;
  bicg_->compute(this->lhs_);
  ASC_VERIFY(bicg_->info() == Eigen::Success, "precomputation failed");
}

// parallelized direct method
#ifdef MKL_USE_parallel
template <typename T>
class SpPardisoDirect : public SpLinearSolver<T> {
 public:
  std::unique_ptr<SpPardisoLU<T>> plu_;
  std::unique_ptr<SpPardisoLLT<T>> pllt_;
  std::unique_ptr<SpPardisoLDLT<T>> pldlt_;

  explicit SpPardisoDirect(const int size) : SpLinearSolver<T>(size) {}

  ~SpPardisoDirect() override = default;

  void SetType() override;
  void Init() override;
  void Compute() override;
  void Solve() override;

 private:
  void ComputeLU();
  void ComputeLLT();
  void ComputeLDLT();
};

template <typename T>
void SpPardisoDirect<T>::SetType() {
  bool spd = (!this->CheckSymmetric()) ? false : this->CheckPostiveDefinite();
  this->type_ = (spd) ? kPardisoLDLT : kPardisoLU;
}

template <typename T>
void SpPardisoDirect<T>::Init() {
  if (this->IsInitialized()) return;

  this->status_ = 0;
  switch (this->type_) {
    case kPardisoLU:
      plu_ = std::make_unique<SpPardisoLU<T>>();
      break;
    case kPardisoLLT:
      pllt_ = std::make_unique<SpPardisoLLT<T>>();
      break;
    case kPardisoLDLT:
      pldlt_ = std::make_unique<SpPardisoLDLT<T>>();
      break;
    default:
      ASC_ABORT("invalid sparse linear solver type");
  }
  this->status_ |= kInitialized;
}

template <typename T>
void SpPardisoDirect<T>::Compute() {
  if (this->IsComputed()) return;

  switch (this->type_) {
    case kPardisoLU:
      ComputeLU();
      return;
    case kPardisoLLT:
      ComputeLLT();
      return;
    case kPardisoLDLT:
      ComputeLDLT();
      return;
  }
}

template <typename T>
void SpPardisoDirect<T>::Solve() {
  switch (this->type_) {
    case kPardisoLU:
      this->result_ = plu_->solve(this->rhs_);
      return;
    case kPardisoLLT:
      this->result_ = pllt_->solve(this->rhs_);
      return;
    case kPardisoLDLT:
      this->result_ = pldlt_->solve(this->rhs_);
      return;
  }
}

template <typename T>
void SpPardisoDirect<T>::ComputeLU() {
  this->status_ |= kComputed;
  plu_->compute(this->lhs_);
  ASC_VERIFY(plu_->info() == Eigen::Success, "precomputation failed");
}
template <typename T>
void SpPardisoDirect<T>::ComputeLLT() {
  this->status_ |= kComputed;
  pllt_->compute(this->lhs_);
  ASC_VERIFY(pllt_->info() == Eigen::Success, "precomputation failed");
}

template <typename T>
void SpPardisoDirect<T>::ComputeLDLT() {
  this->status_ |= kComputed;
  pldlt_->compute(this->lhs_);
  ASC_VERIFY(pldlt_->info() == Eigen::Success, "precomputation failed");
}

#endif

template <typename T, int Size>
void CVectorToEigen(T (&src)[Size], EDVector<T>& dest) {
  dest.resize(Size);
  dest = Eigen::Map<EDVector<T>>(src, Size);
}

template <typename T, int Size>
void CVectorToEigen(T (&src)[Size], ESVector<T, Size>& dest) {
  dest = Eigen::Map<ESVector<T, Size>>(src, Size);
}

template <typename T, int Rows, int Cols>
void CMatrixToEigen(T (&src)[Rows][Cols], EDMatrix<T>& dest) {
  dest.resize(Rows, Cols);
  for (int i = 0; i < Rows; ++i) {
    dest.row(i) = Eigen::Map<EDVector<T>>(src[i], Cols);
  }
}

template <typename T, int Rows, int Cols>
void CMatrixToEigen(T (&src)[Rows][Cols], ESMatrix<T, Rows, Cols>& dest) {
  for (int i = 0; i < Rows; ++i) {
    dest.row(i) = Eigen::Map<ESVector<T, Cols>>(src[i], Cols);
  }
}

template <typename T, int Size>
void CVectorFromEigen(EDVector<T>& src, T (&dest)[Size]) {
  ASC_VERIFY(src.size() >= Size,
                "invalid copy from eigen vector to plain array");
  Eigen::Map<EDVector<T>>(dest, Size) = src;
}

template <typename T, int Size>
void CVectorFromEigen(ESVector<T, Size>& src, T (&dest)[Size]) {
  Eigen::Map<ESVector<T, Size>>(dest, Size) = src;
}

template <typename T, int Rows, int Cols>
void CMatrixFromEigen(EDMatrix<T>& src, T (&dest)[Rows][Cols]) {
  ASC_VERIFY(src.rows() >= Rows && src.cols() >= Cols,
                "invalid copy from eigen matrix to nested plain array");
  for (int i = 0; i < Rows; ++i) {
    Eigen::Map<EDVector<T>>(dest[i], Cols) = src.row(i);
  }
}

template <typename T, int Rows, int Cols>
void CMatrixFromEigen(ESMatrix<T, Rows, Cols>& src, T (&dest)[Rows][Cols]) {
  for (int i = 0; i < Rows; ++i) {
    Eigen::Map<ESVector<T, Cols>>(dest[i], Cols) = src.row(i);
  }
}

/// @brief Convert ASC DVector to Eigen EDVector (zero-copy via Map)
/// @param src Source ASC DVector
/// @param dest Destination Eigen EDVector (will be resized and mapped)
template <typename T>
void DVectorToEigen(const DVector<T>& src, EDVector<T>& dest) {
  dest.resize(src.GetSize());
  dest = Eigen::Map<const EDVector<T>>(src.Read(), src.GetSize());
}

/// @brief Convert ASC SVector to Eigen ESVector (zero-copy via Map)
/// @param src Source ASC SVector
/// @param dest Destination Eigen ESVector (mapped to source data)
template <typename T, int Size>
void SVectorToEigen(const SVector<T, Size>& src, ESVector<T, Size>& dest) {
  dest = Eigen::Map<const ESVector<T, Size>>(src.Read());
}

/// @brief Convert ASC DMatrix to Eigen EDMatrix (zero-copy via Map)
/// @param src Source ASC DMatrix
/// @param dest Destination Eigen EDMatrix (will be resized and mapped)
template <typename T>
void DMatrixToEigen(const DMatrix<T>& src, EDMatrix<T>& dest) {
  auto shape = src.GetShape();
  dest.resize(shape[0], shape[1]);
  dest = Eigen::Map<const EDMatrix<T>>(src.Read(), shape[0], shape[1]);
}

/// @brief Convert ASC SMatrix to Eigen ESMatrix (zero-copy via Map)
/// @param src Source ASC SMatrix
/// @param dest Destination Eigen ESMatrix (mapped to source data)
template <typename T, int Rows, int Cols>
void SMatrixToEigen(const SMatrix<T, Rows, Cols>& src,
                    ESMatrix<T, Rows, Cols>& dest) {
  dest = Eigen::Map<const ESMatrix<T, Rows, Cols>>(src.Read());
}

/// @brief Convert Eigen EDVector to ASC DVector (copy)
/// @param src Source Eigen EDVector
/// @param dest Destination ASC DVector (will be resized)
template <typename T>
void DVectorFromEigen(const EDVector<T>& src, DVector<T>& dest) {
  dest.SetShape(DShape<1>(src.size()));
  T* dest_ptr = dest.ReadWrite();
  for (int i = 0; i < src.size(); ++i) {
    dest_ptr[i] = src(i);
  }
}

/// @brief Convert Eigen ESVector to ASC SVector (copy)
/// @param src Source Eigen ESVector
/// @param dest Destination ASC SVector
template <typename T, int Size>
void SVectorFromEigen(const ESVector<T, Size>& src, SVector<T, Size>& dest) {
  T* dest_ptr = dest.ReadWrite();
  for (int i = 0; i < Size; ++i) {
    dest_ptr[i] = src(i);
  }
}

/// @brief Convert Eigen EDMatrix to ASC DMatrix (copy)
/// @param src Source Eigen EDMatrix
/// @param dest Destination ASC DMatrix (will be resized)
template <typename T>
void DMatrixFromEigen(const EDMatrix<T>& src, DMatrix<T>& dest) {
  dest.SetShape(DShape<2>(src.rows(), src.cols()));
  T* dest_ptr = dest.ReadWrite();
  // Copy column-major data (both Eigen and ASC use LayoutLeft)
  for (int j = 0; j < src.cols(); ++j) {
    for (int i = 0; i < src.rows(); ++i) {
      dest_ptr[i + j * src.rows()] = src(i, j);
    }
  }
}

/// @brief Convert Eigen ESMatrix to ASC SMatrix (copy)
/// @param src Source Eigen ESMatrix
/// @param dest Destination ASC SMatrix
template <typename T, int Rows, int Cols>
void SMatrixFromEigen(const ESMatrix<T, Rows, Cols>& src,
                      SMatrix<T, Rows, Cols>& dest) {
  T* dest_ptr = dest.ReadWrite();
  // Copy column-major data (both Eigen and ASC use LayoutLeft)
  for (int j = 0; j < Cols; ++j) {
    for (int i = 0; i < Rows; ++i) {
      dest_ptr[i + j * Rows] = src(i, j);
    }
  }
}

// ============================================================================
// Sparse Matrix Conversion Functions (SpDMatrix/SpSMatrix <-> ESpMatrix)
// ============================================================================

// Note: ESpMatrix<T> uses kDefaultEigenOptions which matches ASC's
// DefaultSparseLayout (CSC when column-major, CSR when row-major).
// This ensures zero-copy Eigen::Map is always possible.

/// @brief Create a zero-copy Eigen Map view of ASC SpDMatrix
///
/// Creates an Eigen::Map that directly references the underlying data of a
/// ASC sparse matrix without any data copy.
///
/// @warning The source SpDMatrix must outlive the returned Map.
///
/// @tparam T Element type
/// @param src Source ASC SpDMatrix (must remain valid)
/// @return Eigen::Map wrapping the sparse matrix data (zero-copy)
template <typename T>
Eigen::Map<const ESpMatrix<T>> SpDMatrixMap(const SpDMatrix<T>& src) {
  const int nrows = src.GetExtent(0);
  const int ncols = src.GetExtent(1);
  const int nnz = src.GetNNZ();

  const auto& map = src.GetMap();
  const int* outer_ptr = map.GetOuterPtr().HostRead();
  const int* inner_indices = map.GetInnerIndices(0).HostRead();
  const T* val_data = src.GetValues().HostRead();

  return Eigen::Map<const ESpMatrix<T>>(nrows, ncols, nnz, outer_ptr,
                                        inner_indices, val_data);
}

/// @brief Create a zero-copy Eigen Map view of ASC SpSMatrix
///
/// @tparam T Element type
/// @tparam M Number of rows
/// @tparam N Number of columns
/// @param src Source ASC SpSMatrix (must remain valid)
/// @return Eigen::Map wrapping the sparse matrix data (zero-copy)
template <typename T, int M, int N>
Eigen::Map<const ESpMatrix<T>> SpSMatrixMap(const SpSMatrix<T, M, N>& src) {
  const int nnz = src.GetNNZ();

  const auto& map = src.GetMap();
  const int* outer_ptr = map.GetOuterPtr().HostRead();
  const int* inner_indices = map.GetInnerIndices(0).HostRead();
  const T* val_data = src.GetValues().HostRead();

  return Eigen::Map<const ESpMatrix<T>>(M, N, nnz, outer_ptr, inner_indices,
                                        val_data);
}

/// @brief Convert ASC SpDMatrix to Eigen sparse matrix view (zero-copy)
/// Returns a zero-copy Eigen::Map that directly references the underlying
/// data of the ASC sparse matrix. This is an alias for SpDMatrixMap().
///
/// @warning The source SpDMatrix must outlive the returned Map.
///
/// @tparam T Element type
/// @param src Source ASC SpDMatrix (must remain valid)
/// @return Eigen::Map wrapping the sparse matrix data (zero-copy)
template <typename T>
Eigen::Map<const ESpMatrix<T>> SpDMatrixToEigen(const SpDMatrix<T>& src) {
  return SpDMatrixMap(src);
}

/// @brief Convert ASC SpSMatrix to Eigen sparse matrix view (zero-copy)
///
/// Returns a zero-copy Eigen::Map that directly references the underlying
/// data of the ASC sparse matrix. This is an alias for SpSMatrixMap().
///
/// @warning The source SpSMatrix must outlive the returned Map.
///
/// @tparam T Element type
/// @tparam M Number of rows
/// @tparam N Number of columns
/// @param src Source ASC SpSMatrix (must remain valid)
/// @return Eigen::Map wrapping the sparse matrix data (zero-copy)
template <typename T, int M, int N>
Eigen::Map<const ESpMatrix<T>> SpSMatrixToEigen(const SpSMatrix<T, M, N>& src) {
  return SpSMatrixMap(src);
}

/// @brief Convert Eigen ESpMatrix to ASC SpDMatrix (single copy)
///
/// @tparam T Element type
/// @param src Source Eigen sparse matrix (must be compressed)
/// @param dest Destination ASC SpDMatrix (will be resized)
template <typename T>
void SpDMatrixFromEigen(const ESpMatrix<T>& src, SpDMatrix<T>& dest) {
  const int nrows = static_cast<int>(src.rows());
  const int ncols = static_cast<int>(src.cols());
  const int nnz = static_cast<int>(src.nonZeros());

  dest = SpDMatrix<T>(DShape<2>(nrows, ncols));

  if (nnz == 0) {
    return;
  }

  ASC_VERIFY(src.isCompressed(),
                "Source Eigen sparse matrix must be in compressed format");

  const auto* outer_ptr = src.outerIndexPtr();
  const auto* inner_ptr = src.innerIndexPtr();
  const auto* val_ptr = src.valuePtr();

  // Direct copy - formats match
  auto& map = const_cast<typename SpDMatrix<T>::MapType&>(dest.GetMap());
  auto& values = const_cast<UArray<T>&>(dest.GetValues());

  map.InitializeStorage(nnz);
  values.SetSize(nnz);

  const int outer_size = (kDefaultEigenOptions == Eigen::ColMajor) ? ncols : nrows;

  int* dest_outer = map.GetOuterPtr().HostWrite();
  for (int i = 0; i <= outer_size; ++i) {
    dest_outer[i] = static_cast<int>(outer_ptr[i]);
  }

  int* dest_inner = map.GetInnerIndices(0).HostWrite();
  for (int k = 0; k < nnz; ++k) {
    dest_inner[k] = static_cast<int>(inner_ptr[k]);
  }

  T* dest_values = values.HostWrite();
  for (int k = 0; k < nnz; ++k) {
    dest_values[k] = val_ptr[k];
  }
}

/// @brief Convert Eigen ESpMatrix to ASC SpSMatrix (single copy)
///
/// @tparam T Element type
/// @tparam M Number of rows
/// @tparam N Number of columns
/// @param src Source Eigen sparse matrix (must be compressed)
/// @param dest Destination ASC SpSMatrix
template <typename T, int M, int N>
void SpSMatrixFromEigen(const ESpMatrix<T>& src, SpSMatrix<T, M, N>& dest) {
  ASC_VERIFY(src.rows() == M && src.cols() == N,
                "Eigen matrix dimensions must match SpSMatrix dimensions");

  const int nnz = static_cast<int>(src.nonZeros());

  dest = SpSMatrix<T, M, N>();

  if (nnz == 0) {
    return;
  }

  ASC_VERIFY(src.isCompressed(),
                "Source Eigen sparse matrix must be in compressed format");

  const auto* outer_ptr = src.outerIndexPtr();
  const auto* inner_ptr = src.innerIndexPtr();
  const auto* val_ptr = src.valuePtr();

  auto& map = const_cast<typename SpSMatrix<T, M, N>::MapType&>(dest.GetMap());
  auto& values = const_cast<UArray<T>&>(dest.GetValues());

  map.InitializeStorage(nnz);
  values.SetSize(nnz);

  constexpr int outer_size = (kDefaultEigenOptions == Eigen::ColMajor) ? N : M;

  int* dest_outer = map.GetOuterPtr().HostWrite();
  for (int i = 0; i <= outer_size; ++i) {
    dest_outer[i] = static_cast<int>(outer_ptr[i]);
  }

  int* dest_inner = map.GetInnerIndices(0).HostWrite();
  for (int k = 0; k < nnz; ++k) {
    dest_inner[k] = static_cast<int>(inner_ptr[k]);
  }

  T* dest_values = values.HostWrite();
  for (int k = 0; k < nnz; ++k) {
    dest_values[k] = val_ptr[k];
  }
}

}  // namespace asc

#endif  // ASC_EIGEN_H_
