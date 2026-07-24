// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/spherical.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_SPHERICAL_H_
#define ASC_SPHERICAL_H_

#include <memory>
#include <type_traits>

#include "asc/core/error.h"
#include "asc/array/marray.h"
#include "asc/array/mlayout.h"
#include "asc/core/math.h"
#include "asc/random/generator.h"
#include "asc/random/sampler.h"

namespace asc {

// Forward declarations
template <FloatingPoint T, typename Generator>
class RandomSampler;

template <FloatingPoint T, typename Generator>
class NormalSampler;

/// @brief Uniform sampler on unit hypersphere
///
/// Generates uniformly distributed samples on the surface of a unit
/// hypersphere in n-dimensional space using the normalization method.
///
/// @tparam T The floating-point type for samples
/// @tparam Generator The random number generator type
template <FloatingPoint T, typename Generator = NormalGenerator<T>>
class SphericalSampler : public RandomSampler<T, Generator> {
 protected:
  using Base = RandomSampler<T, Generator>;
  int n_dims_;  ///< Dimension of the hypersphere
  std::unique_ptr<NormalSampler<T, Generator>> normal_;

 public:
  /// @brief Constructor
  /// @param n_dims Dimension of the hypersphere (default: 3 for unit sphere)
  explicit SphericalSampler(int n_dims = 3) : Base(), n_dims_(n_dims) {
    Initialize();
  }

  /// @brief Constructor with external generator
  /// @param generator Pointer to external generator
  /// @param n_dims Dimension of the hypersphere
  explicit SphericalSampler(Generator* generator, int n_dims = 3)
      : Base(generator), n_dims_(n_dims) {
    Initialize();
  }

  /// @brief Destructor
  ~SphericalSampler() override = default;

  /// @brief Sample into a 1D buffer (vector)
  /// @tparam Buffer TensorLike type (must be 1D)
  /// @param buffer Output buffer for samples
  /// @note For 1D, samples are ±1 (endpoints of unit circle)
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 1)
  void Sample(Buffer& buffer) {
    const int m = buffer.GetSize();
    // For 1D case, samples are just ±1
    for (int i = 0; i < m; ++i) {
      T z = this->generator_->Next();
      buffer[i] = Sign(z);
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
      ASC_VERIFY(n <= n_dims_, "Buffer dimension exceeds sphere dimension");

      // Sample from standard normal and normalize
      normal_->Sample(buffer);

      // Normalize each sample to unit length
      // Column-major: data[i + j * m] is sample i, dimension j
      for (int i = 0; i < m; ++i) {
        auto slice = buffer.Slice(0, i);
        T norm = slice.Norm(2);
        slice /= norm;
      }
    } else {
      const int m = buffer.GetExtent(1);  // Dimension
      const int n = buffer.GetExtent(0);  // Number of samples
      ASC_VERIFY(m <= n_dims_, "Buffer dimension exceeds sphere dimension");

      // Sample from standard normal and normalize
      normal_->Sample(buffer);

      // Normalize each sample to unit length
      // Row-major: data[i * n + j] is sample i, dimension j
      for (int i = 0; i < n; ++i) {
        auto slice = buffer.Slice(0, i);
        T norm = slice.Norm(2);
        slice /= norm;
      }
    }
  }

 private:
  /// @brief Initialize the internal normal sampler
  void Initialize() {
    // Create zero mean and identity covariance
    DVector<T> mean(n_dims_);
    DMatrix<T> covariance(n_dims_, n_dims_);
    mean.SetZeros();
    covariance.SetIdentity();
    normal_ = std::make_unique<NormalSampler<T, Generator>>(
        this->generator_, mean, covariance);
  }
};

}  // namespace asc

#endif  // ASC_SPHERICAL_H_
