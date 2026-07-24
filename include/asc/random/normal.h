// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/normal.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_NORMAL_H_
#define ASC_NORMAL_H_

#include <type_traits>

#include "asc/core/error.h"
#include "asc/array/marray.h"
#include "asc/array/mlayout.h"
#include "asc/random/generator.h"
#include "asc/random/sampler.h"
#include "asc/linalg/blas.h"

namespace asc {

// Forward declaration
template <FloatingPoint T, typename Generator>
class RandomSampler;

/// @brief Multivariate normal distribution sampler
///
/// Generates samples from a multivariate normal distribution using
/// Cholesky decomposition of the covariance matrix.
///
/// @tparam T The floating-point type for samples
/// @tparam Generator The random number generator type
template <FloatingPoint T, typename Generator = NormalGenerator<T>>
class NormalSampler : public RandomSampler<T, Generator> {
 protected:
  using Base = RandomSampler<T, Generator>;
  DVector<T> mean_;        ///< Mean vector
  DMatrix<T> covariance_;  ///< Covariance matrix
  DMatrix<T> transform_;   ///< Cholesky decomposition (lower triangular)

 public:
  /// @brief Constructor with mean vector and covariance matrix
  /// @param mean Mean vector (DVector)
  /// @param covariance Covariance matrix (DMatrix)
  NormalSampler(const DVector<T>& mean, const DMatrix<T>& covariance)
      : Base(),
        mean_(mean),
        covariance_(covariance),
        transform_(mean.GetSize(), mean.GetSize()) {
    ASC_VERIFY(covariance.GetExtent(0) == covariance.GetExtent(1),
                  "Covariance matrix must be square");
    ASC_VERIFY(mean.GetSize() == covariance.GetExtent(0),
                  "Mean and covariance dimensions must match");
    ComputeCholesky();
  }

  /// @brief Constructor with external generator
  /// @param generator Pointer to external generator
  /// @param mean Mean vector (DVector)
  /// @param covariance Covariance matrix (DMatrix)
  NormalSampler(Generator* generator, const DVector<T>& mean,
                const DMatrix<T>& covariance)
      : Base(generator),
        mean_(mean),
        covariance_(covariance),
        transform_(mean.GetSize(), mean.GetSize()) {
    ASC_VERIFY(covariance.GetExtent(0) == covariance.GetExtent(1),
                  "Covariance matrix must be square");
    ASC_VERIFY(mean.GetSize() == covariance.GetExtent(0),
                  "Mean and covariance dimensions must match");
    ComputeCholesky();
  }

  /// @brief Sample into a 1D buffer (vector)
  /// @tparam Buffer TensorLike type (must be 1D)
  /// @param buffer Output buffer for samples
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 1)
  void Sample(Buffer& buffer) {
    const int m = buffer.GetSize();
    const int n = mean_.GetSize();

    ASC_VERIFY(n == 1, "Use 2D Sample for multivariate distribution");

    // 1D case: scalar normal
    for (int i = 0; i < m; ++i) {
      T z = this->generator_->Next();
      buffer[i] = mean_[0] + transform_[0] * z;
    }
  }

  /// @brief Sample into a 2D buffer (matrix)
  /// @tparam Buffer TensorLike type (must be 2D)
  /// @param buffer Output buffer for samples (matrix)
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 2)
  void Sample(Buffer& buffer) {
    if constexpr (std::is_same_v<DefaultLayout, LayoutLeft>) {
      const int m = buffer.GetExtent(0);  // Number of samples
      const int n = buffer.GetExtent(1);  // Dimension

      ASC_VERIFY(n <= mean_.GetSize(),
                    "Buffer dimension exceeds distribution dimension");

      // Allocate once outside the loop to avoid m heap allocations
      DVector<T> z(n), row(n);
      for (int i = 0; i < m; ++i) {
        for (int k = 0; k < n; ++k) z[k] = this->generator_->Next();
        // row = L * z  (upper-triangular zeros in transform_ contribute nothing)
        MatMul(transform_, z, row);
        row += mean_;
        for (int j = 0; j < n; ++j) buffer(i, j) = row[j];
      }
    } else {
      const int m = buffer.GetExtent(1);  // Dimension
      const int n = buffer.GetExtent(0);  // Number of samples

      ASC_VERIFY(m <= mean_.GetSize(),
                    "Buffer dimension exceeds distribution dimension");

      // Allocate once outside the loop to avoid n heap allocations
      DVector<T> z(m), row(m);
      for (int i = 0; i < n; ++i) {
        for (int k = 0; k < m; ++k) z[k] = this->generator_->Next();
        // row = L * z  (upper-triangular zeros in transform_ contribute nothing)
        MatMul(transform_, z, row);
        row += mean_;
        for (int j = 0; j < m; ++j) buffer(i, j) = row[j];
      }
    }
  }

 private:
  /// @brief Compute Cholesky decomposition of covariance matrix
  ///
  /// Computes L such that \Sigma = L * L^T where L is lower triangular.
  /// Uses column-major storage for both input and output.
  void ComputeCholesky() {
    if constexpr (std::is_same_v<DefaultLayout, LayoutRight>) {
      const int n = mean_.GetSize();
      transform_.SetZeros();
      // Cholesky-Banachiewicz algorithm (column-major)
      for (int j = 0; j < n; ++j) {
        for (int i = j; i < n; ++i) {
          T sum = 0.0;
          for (int k = 0; k < j; ++k) {
            // L(i,k) * L(j,k) in column-major: L(i,k) * L(j,k)
            sum += transform_(i, k) * transform_(j, k);
          }
          if (i == j) {
            // Diagonal element: L(j,j) = sqrt(\Sigma(j,j) - sum)
            transform_(j, j) = std::sqrt(covariance_(j, j) - sum);
          } else {
            // Off-diagonal: L(i,j) = (\Sigma(i,j) - sum) / L(j,j)
            transform_(i, j) = (covariance_(i, j) - sum) / transform_(j, j);
          }
        }
      }
    } else {
      const int n = mean_.GetSize();
      transform_.SetZeros();
      // Cholesky-Banachiewicz algorithm (row-major)
      for (int j = 0; j < n; ++j) {
        for (int i = j; i < n; ++i) {
          T sum = 0.0;
          for (int k = 0; k < j; ++k) {
            // L(i,k) * L(j,k) in row-major: L(k,i) * L(k,j)
            sum += transform_(k, i) * transform_(k, j);
          }
          if (i == j) {
            // Diagonal element: L(j,j) = sqrt(\Sigma(j,j) - sum)
            transform_(j, j) = std::sqrt(covariance_(j, j) - sum);
          } else {
            // Off-diagonal: L(i,j) = (\Sigma(i,j) - sum) / L(j,j)
            transform_(i, j) = (covariance_(i, j) - sum) / transform_(j, j);
          }
        }
      }
    }
  }
};

}  // namespace asc

#endif  // ASC_NORMAL_H_
