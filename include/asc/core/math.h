// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/core/math.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_CORE_MATH_H_
#define ASC_CORE_MATH_H_

/// @file math.h
/// @brief Numeric utilities and mathematical functions
///
/// This module provides:
/// - Basic arithmetic operations (Add, Subtract, Multiply, Divide)
/// - Numeric limits and constants (Epsilon, Infinity, NaN)
/// - Fuzzy floating-point comparisons for numerical stability
/// - Mathematical functions (Sqrt, Pow, Factorial, Binomial)
/// - Comparison operators (Max, Min, Sign)
/// - Mixed-precision arithmetic support
///
/// @par Example - Fuzzy comparisons:
/// @code
/// double a = 0.1 + 0.2;
/// double b = 0.3;
/// bool equal = asc::FuzzyEQ(a, b);  // true (handles rounding errors)
/// bool realEqual = (a == b);          // false (exact comparison)
/// @endcode
///
/// @par Example - Constexpr sqrt:
/// @code
/// constexpr double x = asc::Sqrt(2.0);  // Compile-time sqrt
/// @endcode
///
/// @par Example - Combinatorics:
/// @code
/// int fact = asc::Factorial(5);     // 120
/// int comb = asc::Binomial(5, 2);   // 10 (5 choose 2)
/// @endcode

#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>
#include "asc/core/globals.h"
#include "asc/core/error.h"
#include "asc/core/casts.h"
#include "asc/core/operators.h"

namespace asc {

// ============================================================================
// Fuzzy Comparison Functions
// ============================================================================
// Note: These functions now use operators from generic/operators.h
// to ensure consistency and avoid code duplication.
// ============================================================================

/// @brief Fuzzy check: x ≈ 0
/// @param x Value to check
/// @param tol Tolerance (default: machine epsilon)
template <FloatingPoint T>
constexpr bool FuzzyZero(T x, T tol = std::numeric_limits<T>::epsilon()) {
  return AbsOp<T>()(x) < tol;
}

/// @brief Fuzzy check: |a - b| <= tol
/// @param a First value
/// @param b Second value
/// @param tol Tolerance (default: machine epsilon)
template <FloatingPoint T>
constexpr bool FuzzyEqual(T a, T b, T tol = std::numeric_limits<T>::epsilon()) {
  return FuzzyEqualOp<T>(tol)(a, b);
}

/// @brief Fuzzy check: |a - b| > tol
/// @param a First value
/// @param b Second value
/// @param tol Tolerance (default: machine epsilon)
template <FloatingPoint T>
constexpr bool FuzzyNotEqual(T a, T b,
                             T tol = std::numeric_limits<T>::epsilon()) {
  return FuzzyNotEqualOp<T>(tol)(a, b);
}

/// @brief Fuzzy check: a < b - tol
/// @param a First value
/// @param b Second value
/// @param tol Tolerance (default: machine epsilon)
template <FloatingPoint T>
constexpr bool FuzzyLess(T a, T b, T tol = std::numeric_limits<T>::epsilon()) {
  return FuzzyLessOp<T>(tol)(a, b);
}

/// @brief Fuzzy check: a > b + tol
/// @param a First value
/// @param b Second value
/// @param tol Tolerance (default: machine epsilon)
template <FloatingPoint T>
constexpr bool FuzzyGreater(T a, T b,
                            T tol = std::numeric_limits<T>::epsilon()) {
  return FuzzyGreaterOp<T>(tol)(a, b);
}

// ============================================================================
// Exact Comparison Functions
// ============================================================================
// Note: These functions now use operators from generic/operators.h
// ============================================================================

/// @brief Exact check: a == b.
/// @param a First value
/// @param b Second value
/// @return True if a equals b exactly
template <Arithmetic T>
constexpr bool Equal(const T& a, const T& b) {
  return EqualOp<T>()(a, b);
}

/// @brief Exact check: a != b.
/// @param a First value
/// @param b Second value
/// @return True if a does not equal b exactly
template <Arithmetic T>
constexpr bool NotEqual(const T& a, const T& b) {
  return NotEqualOp<T>()(a, b);
}

/// @brief Exact check: a <= b.
/// @param a First value
/// @param b Second value
/// @return True if a is less than or equal to b exactly
template <Arithmetic T>
constexpr bool LessEqual(const T& a, const T& b) {
  return LessEqualOp<T>()(a, b);
}

/// @brief Exact check: a >= b.
/// @param a First value
/// @param b Second value
/// @return True if a is greater than or equal to b exactly
template <Arithmetic T>
constexpr bool GreaterEqual(const T& a, const T& b) {
  return GreaterEqualOp<T>()(a, b);
}

/// @brief Exact check: a < b.
/// @param a First value
/// @param b Second value
/// @return True if a is less than b exactly
template <Arithmetic T>
constexpr bool Less(const T& a, const T& b) {
  return LessOp<T>()(a, b);
}

/// @brief Exact check: a > b.
/// @param a First value
/// @param b Second value
/// @return True if a is greater than b exactly
template <Arithmetic T>
constexpr bool Greater(const T& a, const T& b) {
  return GreaterOp<T>()(a, b);
}

// ============================================================================
// Mathematical Functions
// ============================================================================
// Note: These functions now use operators from generic/operators.h
// ============================================================================

/// @brief Calculate a^2.
/// @param a Value to square
/// @return Square of a
template <Arithmetic T>
constexpr T Square(const T a) {
  return SquareOp<T>()(a);
}

/// @brief Calculate max(a,b).
/// @param a First value
/// @param b Second value
template <Arithmetic T>
constexpr T Max(T a, T b) {
  return MaxOp<T>()(a, b);
}

/// @brief Calculate min(a,b).
/// @param a First value
/// @param b Second value
/// @return Minimum of a and b
template <Arithmetic T>
constexpr T Min(T a, T b) {
  return MinOp<T>()(a, b);
}

/// @brief Calculate sign(a).
/// @param a Value to check
/// @return +1 if a >= 0, -1 if a < 0
template <Arithmetic T>
constexpr T Sign(T a) {
  return (a >= 0 ? T(1) : T(-1));
}

/// @brief Calculate signabs(a,b).
/// @param a Value whose magnitude is used
/// @param b Value whose sign is used
/// @return Value with magnitude of a and sign of b
template <Arithmetic T>
constexpr T SignAbs(T a, T b) {
  return (b >= 0 ? (a >= 0 ? a : -a) : (a >= 0 ? -a : a));
}

/// @brief Calculate sqrt(x) by Newton-Raphson algorithm.
/// @param a Value to compute sqrt
/// @param curr Current approximation
/// @param prev Previous approximation
/// @return Refined approximation of sqrt(a)
template <Arithmetic T>
constexpr T SqrtNewtonRaphson(T a, T curr, T prev) {
  return (curr == prev) ? curr
                        : SqrtNewtonRaphson(a, T(0.5) * (curr + a / curr),
                                            curr);
}

/// @brief Calculate sqrt(x), constexpr version.
/// @param x Value to compute sqrt
/// @return Square root of x, or NaN if x < 0
/// @note Uses Newton-Raphson iteration for compile-time evaluation
template <Arithmetic T>
constexpr T Sqrt(T x) {
  return (x >= 0 && x < std::numeric_limits<T>::infinity())
             ? SqrtNewtonRaphson<T>(x, x, 0)
             : std::numeric_limits<T>::signaling_NaN();
}

/// @brief Compute pow(base, exp).
/// @param base Base value
/// @param exp Exponent value
/// @return base raised to the power of exp
template <Arithmetic T>
T Pow(T base, T exp) {
  return std::pow(base, exp);
}

/// @brief Power for integer-type varible.
/// @param base Base value
/// @param exp Exponent value (non-negative integer)
/// @return base raised to the power of exp
template <>
int Pow<int>(int base, int exp);

/// @brief Calculate n!
/// @param n Non-negative integer
/// @return Factorial of n
template <Integral T>
T Factorial(const T n) {
  if constexpr (std::is_signed_v<T>) {
    ASC_VERIFY(n >= 0, "Factorial is undefined for negative values.");
  }
  T result = 1;
  for (T i = 2; i <= n; ++i) {
    ASC_VERIFY(result <= std::numeric_limits<T>::max() / i,
                  "Factorial result overflows the destination type.");
    result *= i;
  }
  return result;
}

/// @brief Calculate binomial number C_n^r.
/// @param n Non-negative integer
/// @param r Non-negative integer
/// @return Binomial coefficient C_n^r
/// @note C_n^r = n! / (r! * (n - r)!)
template <Integral T>
T Binomial(T n, T r) {
  if constexpr (std::is_signed_v<T>) {
    ASC_VERIFY(n >= 0 && r >= 0,
                  "Binomial arguments must be non-negative.");
  }
  if (r > n) return T(0);
  if (r > n - r) r = n - r;

  T res = 1;
  for (T i = 1; i <= r; ++i) {
    const T numerator = n - r + i;
    ASC_VERIFY(res <= std::numeric_limits<T>::max() / numerator,
                  "Binomial result overflows the destination type.");
    res = static_cast<T>((res * numerator) / i);
  }
  return res;
}

}  // namespace asc

#endif  // ASC_CORE_MATH_H_
