// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/latin.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_LATIN_H_
#define ASC_LATIN_H_

#include <algorithm>
#include <iterator>
#include <type_traits>

#include "asc/array/concepts.h"
#include "asc/array/mlayout.h"
#include "asc/random/sampler.h"

namespace asc {

// Forward declaration
template <FloatingPoint T, typename Generator>
class RandomSampler;

/// @brief Latin Hypercube Sampling (LHS) for stratified quasi-random sequences
///
/// Latin Hypercube Sampling stratifies the sample space by dividing each
/// dimension into equally probable intervals, ensuring better space-filling
/// properties than pure random sampling.
///
/// @tparam T The floating-point type for samples
/// @tparam Generator The random number generator type
template <FloatingPoint T, typename Generator = UniformGenerator<T>>
class LatinSampler : public RandomSampler<T, Generator> {
 protected:
  using Base = RandomSampler<T, Generator>;
  bool jitter_;  ///< Whether to jitter samples within strata

  template <typename RandomAccessIterator>
  void Shuffle(RandomAccessIterator first, RandomAccessIterator last) {
    using Diff =
        typename std::iterator_traits<RandomAccessIterator>::difference_type;
    const Diff n = last - first;
    if (n <= 1) {
      return;
    }

    for (Diff i = n - 1; i > 0; --i) {
      Diff j = static_cast<Diff>(this->generator_->Next() * T(i + 1));
      if (j > i) {
        j = i;
      }
      std::iter_swap(first + i, first + j);
    }
  }

 public:
  /// @brief Constructor
  /// @param jitter Whether to add random jitter within each stratum
  explicit LatinSampler(bool jitter = false) : Base(), jitter_(jitter) {}

  /// @brief Constructor with external generator
  /// @param generator Pointer to external generator
  /// @param jitter Whether to add random jitter within each stratum
  explicit LatinSampler(Generator* generator, bool jitter = false)
      : Base(generator), jitter_(jitter) {}

  /// @brief Sample into a 1D buffer (vector)
  /// @tparam Buffer TensorLike type (must be 1D)
  /// @param buffer Output buffer for samples (length m)
  template <DenseTensorLike Buffer>
    requires(Buffer::GetRank() == 1)
  void Sample(Buffer& buffer) {
    const int m = buffer.GetSize();
    const T delta = T(1) / T(m);
    for (int j = 0; j < m; ++j) {
      T p = jitter_ ? this->generator_->Next() : T(0.5);
      buffer[j] = (j + p) * delta;
    }
    Shuffle(buffer.begin(), buffer.end());
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
      const T delta = T(1) / T(m);

      // Column-major: each column is shuffled independently
      for (int j = 0; j < n; ++j) {
        for (int i = 0; i < m; ++i) {
          T p = jitter_ ? this->generator_->Next() : T(0.5);
          buffer(i, j) = (i + p) * delta;
        }
        Shuffle(buffer.begin() + j * m, buffer.begin() + (j + 1) * m);
      }
    } else {
      const int m = buffer.GetExtent(0);
      const int n = buffer.GetExtent(1);
      const T delta = T(1) / T(m);

      // Row-major: each row is shuffled independently
      for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
          T p = jitter_ ? this->generator_->Next() : T(0.5);
          buffer(i, j) = (j + p) * delta;
        }
        Shuffle(buffer.begin() + i * n, buffer.begin() + (i + 1) * n);
      }
    }
  }
};

}  // namespace asc

#endif  // ASC_LATIN_H_
