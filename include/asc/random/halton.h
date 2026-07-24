// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/halton.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_HALTON_H_
#define ASC_HALTON_H_

#include <type_traits>

#include "asc/array/concepts.h"
#include "asc/core/error.h"
#include "asc/array/mlayout.h"
#include "asc/core/math.h"
#include "asc/array/uarray.h"
#include "asc/random/generator.h"
#include "asc/random/permutation.h"
#include "asc/random/sampler.h"

namespace asc {

// Forward declaration
template <FloatingPoint T, typename Generator>
class RandomSampler;

template <FloatingPoint T>
T Variate(T x);

/// @brief Halton quasi-random sequence sampler
///
/// Halton sequences are low-discrepancy sequences used in quasi-Monte Carlo
/// methods. They provide better uniform coverage of the sample space compared
/// to pseudo-random sampling.
///
/// @tparam T The floating-point type for samples
/// @tparam Generator The random number generator type
template <FloatingPoint T, typename Generator = UniformGenerator<T>>
class HaltonSampler : public RandomSampler<T, Generator> {
 protected:
  using Base = RandomSampler<T, Generator>;
  static int ValidateMaxDim(int max_dim) {
    ASC_VERIFY(max_dim > 0, "Halton max dimension must be positive");
    return max_dim;
  }

  static int ValidateBaseDim(int base_dim) {
    ASC_VERIFY(base_dim >= 0, "Halton base dimension must be non-negative");
    return base_dim;
  }

  UArray<int> base_;                ///< Prime bases for each dimension
  UArray<int> permutation_offset_;  ///< Offsets into permutation cache
  int base_dim_;                    ///< Base dimension offset

 public:
  /// @brief Constructor
  /// @param max_dim Maximum number of dimensions
  /// @param base_dim Base dimension offset (default: 0)
  explicit HaltonSampler(int max_dim = 10, int base_dim = 0)
      : Base(),
        base_(ValidateMaxDim(max_dim)),
        permutation_offset_(ValidateMaxDim(max_dim)),
        base_dim_(ValidateBaseDim(base_dim)) {
    Init(max_dim);
  }

  /// @brief Constructor with external generator
  /// @param generator Pointer to external generator
  /// @param max_dim Maximum number of dimensions
  /// @param base_dim Base dimension offset
  explicit HaltonSampler(Generator* generator, int max_dim = 10,
                         int base_dim = 0)
      : Base(generator),
        base_(ValidateMaxDim(max_dim)),
        permutation_offset_(ValidateMaxDim(max_dim)),
        base_dim_(ValidateBaseDim(base_dim)) {
    Init(max_dim);
  }

  /// @brief Sample into a 1D buffer (vector)
  /// @tparam Buffer TensorLike type (must be 1D)
  /// @param buffer Output buffer for samples (length m)
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 1)
  void Sample(Buffer& buffer) {
    const int m = buffer.GetSize();
    for (int j = 0; j < m; ++j) {
      buffer[j] = SampleHalton(0, j);
    }
  }

  /// @brief Sample into a 2D buffer (matrix)
  /// @tparam Buffer TensorLike type (must be 2D)
  /// @param buffer Output buffer for samples (matrix)
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 2)
  void Sample(Buffer& buffer) {
    if constexpr (std::is_same_v<DefaultLayout, LayoutLeft>) {
      const int m = buffer.GetExtent(0);
      const int n = buffer.GetExtent(1);
      // Column-major: iterate columns (j), then rows (i)
      for (int j = 0; j < n; ++j) {
        for (int i = 0; i < m; ++i) {
          buffer(i, j) = SampleHalton(j, i);
        }
      }
    } else {
      const int m = buffer.GetExtent(0);
      const int n = buffer.GetExtent(1);
      // Row-major: iterate rows (i), then columns (j)
      for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
          buffer(i, j) = SampleHalton(j, i);
        }
      }
    }
  }

 private:
  void Init(int max_dim) {
    const int prime_count = base_dim_ + max_dim;
    Prime::Get().Find(prime_count, true);
    LowDiscrepancyPermutation::Get().EnsureSize(prime_count);
    const int* primes = Prime::Get().GetData();
    const int* prime_sums = Prime::Get().GetCumsum();
    for (int i = 0; i < max_dim; ++i) {
      const int prime_index = base_dim_ + i + 1;
      base_[i] = primes[prime_index];
      permutation_offset_[i] = prime_sums[prime_index - 1];
    }
  }

  T SampleHalton(int dim, int offset) {
    const int* permutation =
        LowDiscrepancyPermutation::Get().GetData() + permutation_offset_[dim];
    T radical_inverse = RadicalInverse<T>(base_dim_ + dim, base_[dim],
                                          permutation, offset);
    return Variate<T>(radical_inverse);
  }
};

}  // namespace asc

#endif  // ASC_HALTON_H_
