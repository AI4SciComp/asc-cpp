// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/generator.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_GENERATOR_H_
#define ASC_GENERATOR_H_

#include <concepts>
#include <cstdint>
#include <limits>
#include <random>

#include "asc/core/globals.h"

namespace asc {

/// @brief Uses std::random_device as the seed
struct DeviceSeed {
  std::random_device rd;
  uint64_t operator()() { return rd(); }
};

/// @brief Uses current time as the seed
struct TimeSeed {
  uint64_t operator()() { return time(nullptr); }
};

/// @brief SplitMix64 random number generator
///
/// An implementation of splitmix64 random number generator. Original C
/// implementation written in 2015 by Sebastiano Vigna (vigna@acm.org)
/// and released into public domain. Re-licensed under the
/// MIT license for use within mde library.
///
/// This is a fixed-increment version of Java 8's SplittableRandom generator
/// See http://dx.doi.org/10.1145/2714064.2660195 and
/// http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html
///
/// It is a very fast generator passing BigCrush, and it can be useful if
/// for some reason you absolutely want 64 bits of state; otherwise, we
/// rather suggest to use a xoroshiro128+ (for moderately parallel
/// computations) or xorshift1024* (for massively parallel computations)
/// generator.
class Splitmix64 {
 public:
  using result_type = uint64_t;

  /// @brief Default constructor with fixed seed
  Splitmix64() : state_(0x27c6003152ca78dull) {}

  /// @brief Constructor with user-defined seed
  /// @param sd Seed value
  explicit Splitmix64(uint64_t sd) { Seed(sd); }

  /// @brief Seed the generator
  /// @param sd Seed value
  void Seed(uint64_t sd) { state_ = sd; }

  /// @brief Generate next random number
  uint64_t operator()() {
    uint64_t z = (state_ += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
  }

  /// @brief Minimum value returned by the generator
  /// @return Minimum value
  static constexpr uint64_t min() noexcept {
    return std::numeric_limits<uint64_t>::min();
  }

  /// @brief Maximum value returned by the generator
  /// @return Maximum value
  static constexpr uint64_t max() noexcept {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  uint64_t state_;  ///< Internal state
};

/// @brief Pcg32 random number generator
///
/// An implementation of pcg32 random number generator.
/// Original C implementation Copyright (c) 2014 Melissa O'Neill
/// <oneill@pcg-random.org>
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
/// http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// For additional information about the PCG random number generation scheme,
/// including its license and other licensing options, visit
/// http://www.pcg-random.org
class Pcg32 {
 public:
  using result_type = uint32_t;

  /// @brief Default constructor with fixed seed
  Pcg32() : state_(0x853c49e6748fea9bull), inc_(0xda3e39cb94b95bdbull) {}

  /// @brief Constructor with user-defined seed
  explicit Pcg32(uint64_t sd) { Seed(sd); }

  /// @brief Constructor with user-defined state and stream
  Pcg32(uint64_t initstate, uint64_t initstream) {
    Seed(initstate, initstream);
  }

  /// @brief Seed the generator
  /// @param sd Seed value
  void Seed(uint64_t sd) {
    Splitmix64 g(sd);
    uint64_t initstate = g();
    uint64_t initstream = g();
    Seed(initstate, initstream);
  }

  /// @brief Seed the generator with state and stream
  /// @param initstate Initial state
  /// @param initstream Initial stream
  void Seed(uint64_t initstate, uint64_t initstream) {
    state_ = 0;
    inc_ = (initstream << 1u) | 1u;
    (*this)();
    state_ += initstate;
    (*this)();
  }

  /// @brief Generate next random number
  /// @return Next random number
  uint32_t operator()() {
    uint64_t oldstate = state_;
    state_ = oldstate * 6364136223846793005ull + inc_;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }

  /// @brief Minimum value returned by the generator
  /// @return Minimum value
  static constexpr uint32_t min() noexcept {
    return std::numeric_limits<uint32_t>::min();
  }

  /// @brief Maximum value returned by the generator
  /// @return Maximum value
  static constexpr uint32_t max() noexcept {
    return std::numeric_limits<uint32_t>::max();
  }

 private:
  uint64_t state_;  ///< Internal state
  uint64_t inc_;    ///< Stream selector
};

/// @brief Xoroshiro64* random number generator
///
/// An implementation of xoroshiro64* 1.0 random number generator.
/// Original C implementation written in 2016 by David Blackman and
/// Sebastiano Vigna (vigna@acm.org) and released into public domain.
/// Re-licensed under the MIT license for use within mde library.
///
/// This is xoroshiro64* 1.0, our best and fastest 32-bit small-state generator
/// for 32-bit floating-point numbers. We suggest to use its upper bits for
/// floating-point generation, as it is slightly faster than
/// xoroshiro64**. It passes all tests we are aware of except for
/// linearity tests, as the lowest six bits have low linear complexity, so
/// if low linear complexity is not considered an issue (as it is usually
/// the case) it can be used to generate 32-bit outputs, too.
class Xoroshiro64Star {
 public:
  using result_type = uint32_t;

  /// @brief Default constructor with fixed seed
  Xoroshiro64Star() : state_{0xcb308ebeul, 0xd97012dcul} {}

  /// @brief Constructor with user-defined seed
  explicit Xoroshiro64Star(uint64_t sd) { Seed(sd); }

  /// @brief Seed the generator
  /// @param sd Seed value
  void Seed(uint64_t sd) {
    uint64_t state64 = Splitmix64(sd)();
    state_[0] = static_cast<uint32_t>(state64);
    state_[1] = static_cast<uint32_t>(state64 >> 32);
  }

  /// @brief Generate next random number
  uint32_t operator()() {
    uint32_t s0 = state_[0];
    uint32_t s1 = state_[1];
    uint32_t result_star = s0 * 0x9e3779bbul;

    s1 ^= s0;
    state_[0] = Rotate(s0, 26) ^ s1 ^ (s1 << 9);  // a, b
    state_[1] = Rotate(s1, 13);                   // c

    return result_star;
  }

  /// @brief Minimum value returned by the generator
  /// @return Minimum value
  static constexpr uint32_t min() noexcept {
    return std::numeric_limits<uint32_t>::min();
  }

  /// @brief Maximum value returned by the generator
  /// @return Maximum value
  static constexpr uint32_t max() noexcept {
    return std::numeric_limits<uint32_t>::max();
  }

 private:
  uint32_t state_[2];  ///< Internal state

  /// @brief Rotate left operation
  /// @param x Value to rotate
  /// @param k Number of bits to rotate
  /// @return Rotated value
  static inline uint32_t Rotate(uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
  }
};

/// @brief Xoroshiro128+ random number generator
///
/// An implementation of xoroshiro128+ 1.0 random number generator.
/// Original C implementation written in 2016-2018 by David Blackman and
/// Sebastiano Vigna (vigna@acm.org) and released into public domain.
/// Re-licensed under the MIT license for use within mde library.
///
/// This is xoroshiro128+ 1.0, our best and fastest small-state generator
/// for floating-point numbers. We suggest to use its upper bits for
/// floating-point generation, as it is slightly faster than
/// xoroshiro128**. It passes all tests we are aware of except for the four
/// lower bits, which might fail linearity tests (and just those), so if
/// low linear complexity is not considered an issue (as it is usually the
/// case) it can be used to generate 64-bit outputs, too; moreover, this
/// generator has a very mild Hamming-weight dependency making our test
/// (http://prng.di.unimi.it/hwd.php) fail after 8 TB of output; we believe
/// this slight bias cannot affect any application. If you are concerned,
/// use xoroshiro128** or xoshiro256+.
///
/// @note The parameters (a=24, b=16, b=37) of this version give slightly
/// better results in our test than the 2016 version (a=55, b=14, c=36).
class Xoroshiro128Plus {
 public:
  using result_type = uint64_t;

  /// @brief Default constructor with fixed seed
  Xoroshiro128Plus() : state_{0x6ca496188ffdcf87ull, 0x492492d4cd7e9082ull} {}

  /// @brief Constructor with user-defined seed
  /// @param sd Seed value
  explicit Xoroshiro128Plus(uint64_t sd) { Seed(sd); }

  /// @brief Seed the generator
  /// @param sd Seed value
  void Seed(uint64_t sd) {
    Splitmix64 g(sd);
    state_[0] = g();
    state_[1] = g();
  }

  /// @brief Generate next random number
  uint64_t operator()() {
    uint64_t s0 = state_[0];
    uint64_t s1 = state_[1];
    uint64_t result = s0 + s1;

    s1 ^= s0;
    state_[0] = Rotate(s0, 24) ^ s1 ^ (s1 << 16);  // a, b
    state_[1] = Rotate(s1, 37);                    // c

    return result;
  }

  /// @brief Minimum value returned by the generator
  /// @return Minimum value
  static constexpr uint64_t min() noexcept {
    return std::numeric_limits<uint64_t>::min();
  }

  /// @brief Maximum value returned by the generator
  /// @return Maximum value
  static constexpr uint64_t max() noexcept {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  uint64_t state_[2];  ///< Internal state

  /// @brief Rotate left operation
  /// @param x Value to rotate
  /// @param k Number of bits to rotate
  /// @return Rotated value
  static inline uint64_t Rotate(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }
};

/// @brief Uniform distribution type alias
template <Arithmetic T>
using UniformDistribution =
    std::conditional_t<FloatingPoint<T>, std::uniform_real_distribution<T>,
                       std::uniform_int_distribution<T>>;

/// @brief Generic random number generator
///
/// @tparam T The type of generated random numbers
/// @tparam Distribution The distribution type (e.g., uniform, normal)
/// @tparam Seed The seed generator type (default: DeviceSeed)
/// @tparam Engine The random number engine type (default: Splitmix64)
template <Arithmetic T, typename Distribution, typename Seed = DeviceSeed,
          typename Engine = Splitmix64>
class Generator {
 public:
  explicit Generator(T min = T(0), T max = T(1))
      : seed_(), engine_(), distribution_(min, max) {}
  void Reset() { engine_.Seed(seed_()); }
  void SetSeed(uint64_t sd) { engine_.Seed(sd); }
  T Next() { return distribution_(engine_); }

 private:
  Seed seed_;
  Engine engine_;
  Distribution distribution_;
};

/// @brief Uniform random number generator
/// @tparam T Type of generated random numbers
/// @tparam Seed The seed generator type (default: DeviceSeed)
/// @tparam Engine The random number engine type (default: Splitmix64)
template <Arithmetic T, typename Seed = DeviceSeed,
          typename Engine = Splitmix64>
using UniformGenerator = Generator<T, UniformDistribution<T>, Seed, Engine>;

/// @brief Normal (Gaussian) random number generator
/// @tparam T Type of generated random numbers
/// @tparam Seed The seed generator type (default: DeviceSeed)
/// @tparam Engine The random number engine type (default: Splitmix64)
template <Arithmetic T, typename Seed = DeviceSeed,
          typename Engine = Splitmix64>
using NormalGenerator = Generator<T, std::normal_distribution<T>, Seed, Engine>;

}  // namespace asc

#endif  // ASC_GENERATOR_H_
