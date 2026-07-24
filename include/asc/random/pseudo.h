// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/pseudo.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_PSEUDO_H_
#define ASC_PSEUDO_H_

#include "asc/array/concepts.h"
#include "asc/random/sampler.h"

namespace asc {

// Forward declaration
template <FloatingPoint T, typename Generator>
class RandomSampler;

/// @brief Pseudo-random number sampler using uniform distribution
///
/// Generates uniformly distributed pseudo-random numbers in [0, 1).
/// Uses a pseudo-random number generator (default: Splitmix64).
///
/// @tparam T The floating-point type for samples
/// @tparam Generator The random number generator type
template <FloatingPoint T, typename Generator = UniformGenerator<T>>
class PseudoSampler : public RandomSampler<T, Generator> {
 protected:
  using Base = RandomSampler<T, Generator>;

 public:
  /// @brief Default constructor
  PseudoSampler() : Base() {}

  /// @brief Constructor with external generator
  /// @param generator Pointer to external generator
  explicit PseudoSampler(Generator* generator) : Base(generator) {}

  /// @brief Sample into a tensor-like buffer
  /// @tparam Buffer TensorLike type
  /// @param buffer Output buffer for samples
  template <DenseTensorLike Buffer>
  void Sample(Buffer& buffer) {
    const int m = buffer.GetSize();
    for (int i = 0; i < m; ++i) {
      buffer[i] = this->generator_->Next();
    }
  }
};

}  // namespace asc

#endif  // ASC_PSEUDO_H_
