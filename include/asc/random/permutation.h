// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/random/perm.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_PERMUTATION_H_
#define ASC_PERMUTATION_H_

#include <cstddef>
#include <vector>

#include "asc/core/error.h"
#include "asc/core/globals.h"

namespace asc {

// ============================================================================
// Prime Number Generator
// ============================================================================

/// @brief Singleton class for prime number generation and caching
///
/// This class implements the Sieve of Eratosthenes algorithm to efficiently
/// find prime numbers and cache them for repeated use. It uses lazy evaluation
/// and automatic cache expansion to handle arbitrarily large prime searches.
///
/// The Prime class maintains an internal cache of found primes and their
/// cumulative sums, which is particularly useful for quasi-random sequence
/// generation (e.g., Halton, Hammersley sequences).
///
/// @note This is a singleton class - use Prime::Get() to access the instance.
/// @note Cache uses 1-based indexing: cache_[n] stores the nth prime,
///       cache_sum_[n] stores the sum of the first n primes.
///
/// ## Example Usage
/// @code
/// // Get the 100th prime number
/// int p = Prime::Get().Find(100);
///
/// // Access cached primes (1-indexed)
/// const int* primes = Prime::Get().Cache();
/// int count = Prime::Get().CacheSize();
/// int first_prime = primes[1];   // Returns 2
/// int second_prime = primes[2];  // Returns 3
/// @endcode
class Prime {
 public:
  /// @brief Get pointer to cached prime numbers (1-indexed array)
  /// @return Pointer to array of cached primes
  int* GetData() noexcept { return data_.data(); }

  /// @brief Get const pointer to cached prime numbers (1-indexed array)
  /// @return Const pointer to array of cached primes
  const int* GetData() const noexcept { return data_.data(); }

  /// @brief Get pointer to cumulative sum of cached primes (1-indexed array)
  /// @return Pointer to array of cumulative sums
  int* GetCumsum() noexcept { return cumsum_.data(); }

  /// @brief Get const pointer to cumulative sum of cached primes (1-indexed)
  /// @return Const pointer to array of cumulative sums
  const int* GetCumsum() const noexcept { return cumsum_.data(); }

  /// @brief Get number of primes currently cached
  /// @return Number of cached primes
  int GetSize() const noexcept { return size_; }

  /// @brief Get current cache capacity
  /// @return Current capacity of cache arrays
  int GetCapcity() const noexcept {
    return static_cast<int>(data_.capacity());
  }

  /// @brief Get current cache capacity
  /// @return Current capacity of cache arrays
  int GetCapacity() const noexcept {
    return static_cast<int>(data_.capacity());
  }

  /**
   * @brief Access the singleton instance
   * @return Reference to the Prime singleton
   */
  static Prime& Get();

  /**
   * @brief Find the nth prime number
   *
   * Uses the Sieve of Eratosthenes algorithm to find prime numbers.
   * The found primes are optionally cached for future queries.
   * If the requested prime is already in cache, returns immediately.
   *
   * The algorithm starts with an estimated upper bound based on the
   * prime number theorem, and doubles the search range if needed.
   *
   * @param n The index of the prime to find (1-indexed: n=1 returns 2)
   * @param enable_cache Whether to cache the found primes (default: true)
   * @return The nth prime number
   *
   * ## Time Complexity
   * - If cached: O(1)
   * - If not cached: O(m log log m) where m is the estimated upper bound
   *
   * ## Example
   * @code
   * int p1 = Prime::Get().Find(1);    // Returns 2
   * int p10 = Prime::Get().Find(10);  // Returns 29
   * int p100 = Prime::Get().Find(100); // Returns 541
   * @endcode
   */
  int Find(int n, bool enable_cache = true);

 private:
  /**
   * @brief Private constructor for singleton pattern
   *
   * Initializes cache arrays with capacity of 32 primes.
   * The cache is expanded dynamically as needed.
   */
  Prime();

  /// @brief Private destructor to clean up cache arrays
  ~Prime() = default;

  // Delete copy and move constructors/operators
  Prime(const Prime&) = delete;
  Prime& operator=(const Prime&) = delete;
  Prime(Prime&&) = delete;
  Prime& operator=(Prime&&) = delete;

  std::vector<int> data_;    ///< Cache to store found primes
  std::vector<int> cumsum_;  ///< Cache to store cumulative sums
  int size_;                 ///< Current number of cached primes

  /// @brief Estimate upper bound for nth prime using prime number theorem
  ///
  /// Uses the approximation: p_n ≈ n * (ln(n) + ln(ln(n)))
  /// For small n (< 6), returns a conservative estimate of 15.
  ///
  /// @param n The index of the prime
  /// @return Estimated upper bound for the nth prime
  static int EstimateUpperBound(int n);
};

// ============================================================================
// Radical Inverse Functions for Low-Discrepancy Sequences
// ============================================================================

/// @brief Compute the radical inverse of a value in a given base
///
/// The radical inverse function reverses the base-b representation of an
/// integer. It is a fundamental component of quasi-random sequences like
/// Halton and Hammersley sequences.
///
/// For example, in base 2:
/// - RadicalInverse(2, 5) reverses 101₂ to get 0.101₂ = 0.625
///
/// @tparam T The floating-point type for the result
/// @param base The base for digit expansion (must be >= 2)
/// @param value The integer value to convert
/// @return The radical inverse as a floating-point number in [0, 1)
///
/// ## Example
/// @code
/// double r = RadicalInverse<double>(2, 5);  // Returns 0.625
/// @endcode
template <FloatingPoint T>
T RadicalInverse(int base, int value) {
  ASC_VERIFY(base >= 2, "base must be >= 2");
  const T inv_base = 1 / T(base);
  T inv_base_n = 1;
  int inverse = 0;
  for (int n; value > 0; value = n) {
    n = value / base;
    int d = static_cast<int>(value - n * base);
    inverse = inverse * base + d;
    inv_base_n *= inv_base;
  }
  return T(inverse) * inv_base_n;
}

/// @brief Compute scrambled radical inverse with permutation
///
/// Similar to RadicalInverse, but applies a digit-wise permutation to
/// improve the distribution quality of quasi-random sequences. This is
/// particularly important for higher-dimensional sequences.
///
/// @tparam T The floating-point type for the result
/// @param base The base for digit expansion (must be >= 2)
/// @param perm Permutation array of length base
/// @param value The integer value to convert
/// @return The scrambled radical inverse in [0, 1)
///
/// ## Example
/// @code
/// int perm[3] = {0, 2, 1};  // Permutation for base 3
/// double r = RadicalInverseScrambled<double>(3, perm, 4);
/// @endcode
template <FloatingPoint T>
T RadicalInverseScrambled(int base, const int* perm, int value) {
  ASC_VERIFY(base >= 2, "base must be >= 2");
  const T inv_base = 1. / T(base);
  T inv_base_n = 1.;
  int inverse = 0;
  for (int n, d; value > 0; value = n) {
    n = value / base;
    d = value % base;
    inverse = inverse * base + perm[d];
    inv_base_n *= inv_base;
  }
  return T(inverse) * inv_base_n;
}

/// @brief Compute radical inverse for multi-dimensional sequences
///
/// This function automatically selects the appropriate base and applies
/// scrambling for dimensions > 1. The first two dimensions (0, 1) use
/// bases 2 and 3 without scrambling, as they naturally have good
/// distribution. Higher dimensions use prime bases with scrambling.
///
/// @tparam T The floating-point type for the result
/// @param dim The dimension index (0-based)
/// @param base The base for this dimension (from prime cache)
/// @param perm The permutation array for scrambling
/// @param value The integer value to convert
/// @return The radical inverse value in [0, 1)
///
/// ## Usage in Halton Sequences
/// @code
/// // Generate 2D Halton point
/// double x = RadicalInverse<double>(0, 2, nullptr, i);  // Base 2
/// double y = RadicalInverse<double>(1, 3, nullptr, i);  // Base 3
/// @endcode
template <FloatingPoint T>
T RadicalInverse(int dim, int base, const int* perm, int value) {
  // Halton & Hammersley sequences for bases 2 and 3 exhibit reasonably good
  // distribution and don't need to be scrambled.
  if (dim == 0) {
    return RadicalInverse<T>(2, value);
  } else if (dim == 1) {
    return RadicalInverse<T>(3, value);
  } else {
    return RadicalInverseScrambled<T>(base, perm, value);
  }
}

// ============================================================================
// Low-Discrepancy Permutation Generator
// ============================================================================

/// @brief Singleton class for generating and caching permutations
///
/// Generates random permutations for each prime base used in scrambled
/// radical inverse calculations. The permutations are generated once and
/// cached for repeated use across quasi-random sequence generation.
///
/// For each prime p, this class maintains a random permutation of {0, 1, ...,
/// p-1}. The first two primes (2 and 3) are not scrambled as noted in the
/// literature.
///
/// @note This is a singleton class - use LowDiscrepancyPermutation::Get()
///       to access the instance.
///
/// ## Implementation Details
/// - Uses std::shuffle for permutation generation
/// - Permutations are stored contiguously in memory
/// - Cache is initialized lazily on first access
///
/// ## Example Usage
/// @code
/// const int* perm = LowDiscrepancyPermutation::Get().Cache();
/// // Access permutation for specific prime using offset from Prime cache
/// @endcode
class LowDiscrepancyPermutation {
 public:
  /// @brief Get pointer to cached permutations
  /// @return Pointer to permutation cache
  int* GetData();

  /// @brief Get const pointer to cached permutations
  /// @return Const pointer to permutation cache
  const int* GetData() const noexcept { return data_.data(); }

  /// @brief Ensure permutations are available for all cached primes up to n
  /// @param prime_count Number of prime bases that must be represented
  void EnsureSize(int prime_count);

  /// @brief Access the singleton instance
  /// @return Reference to the LowDiscrepancyPermutation singleton
  static LowDiscrepancyPermutation& Get();

 private:
  std::vector<int> data_;  ///< Flattened array of all permutations
  int prime_count_ = 0;    ///< Number of cached prime permutations

  /// @brief Private constructor for singleton pattern
  ///
  /// Generates random permutations for all cached primes.
  /// The permutations are stored contiguously in a single array,
  /// with offsets tracked by the Prime class's cache_sum array.
  ///
  /// Algorithm:
  /// 1. Get all currently cached primes
  /// 2. Allocate memory for sum of all prime values
  /// 3. For each prime p:
  ///    - Initialize permutation as identity {0, 1, ..., p-1}
  ///    - Shuffle if p >= 5 (skip bases 2 and 3)
  ///    - Store in contiguous memory
  ///
  /// @note Prime cache uses 1-based indexing: primes[i] is the ith prime
  LowDiscrepancyPermutation() = default;

  /// @brief Private destructor to clean up cache
  ~LowDiscrepancyPermutation() = default;

  // Delete copy and move constructors/operators
  LowDiscrepancyPermutation(const LowDiscrepancyPermutation&) = delete;
  LowDiscrepancyPermutation& operator=(const LowDiscrepancyPermutation&) =
      delete;
};

}  // namespace asc

#endif  // ASC_PERM_H_
