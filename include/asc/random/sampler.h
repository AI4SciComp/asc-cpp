// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/sampler.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_SAMPLER_H_
#define ASC_SAMPLER_H_

#include <limits>
#include <memory>

#include "asc/core/globals.h"
#include "asc/core/error.h"
#include "asc/core/math.h"
#include "asc/random/generator.h"

namespace asc {

/// @brief Clamp a value to [0, 1) range for quasi-random sequences
/// @tparam T Floating-point type
/// @param x Input value
/// @return Value clamped to [0, 1 - epsilon]
template <FloatingPoint T>
T Variate(T x) {
  return Min(x, T(1) - std::numeric_limits<T>::epsilon());
}

/// @brief Base class for all random samplers
///
/// Provides common interface and generator management for random number
/// sampling strategies.
///
/// @tparam T The floating-point type for samples (float or double)
/// @tparam Generator The random number generator type (default:
/// UniformGenerator<T>)
template <FloatingPoint T, typename Generator = UniformGenerator<T>>
class RandomSampler {
 protected:
  std::unique_ptr<Generator> owned_generator_;
  Generator* generator_;  ///< Pointer to the active random number generator

 public:
  /// @brief Default constructor - creates and owns a new generator
  RandomSampler()
      : owned_generator_(std::make_unique<Generator>()),
        generator_(owned_generator_.get()) {
    Reset();
  }

  /// @brief Constructor with external generator
  /// @param generator Pointer to external generator (not owned)
  explicit RandomSampler(Generator* generator)
      : owned_generator_(nullptr), generator_(generator) {
    ASC_VERIFY(generator_ != nullptr, "random sampler generator is null");
    Reset();
  }

  /// @brief Virtual destructor
  virtual ~RandomSampler() = default;

  /// @brief Reset the generator with a new seed
  void Reset() { generator_->Reset(); }
};

}  // namespace asc

// Include all sampler implementations
#include "asc/random/pseudo.h"
#include "asc/random/latin.h"
#include "asc/random/halton.h"
#include "asc/random/hammersley.h"
#include "asc/random/sobol.h"
#include "asc/random/normal.h"
#include "asc/random/spherical.h"

#endif  // ASC_SAMPLER_H_
