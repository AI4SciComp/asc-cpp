// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/operators.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_OPERATORS_H_
#define ASC_OPERATORS_H_

/// @file operators.h
/// @brief Unified operation functors for element-wise operations
///
/// This module provides generic operation functors used throughout ASC for
/// both eager evaluation (DenseMArray compound assignment operators) and lazy
/// evaluation (expression templates).
///
/// @par Operator Categories:
/// - Arithmetic: AddOp, SubOp, MulOp, DivOp
/// - Comparison: EqualOp, NotEqualOp, LessOp, LessEqualOp, GreaterOp,
/// GreaterEqualOp
/// - Fuzzy Comparison: FuzzyEqualOp, FuzzyNotEqualOp, FuzzyLessEqualOp,
/// FuzzyGreaterEqualOp, FuzzyLessOp, FuzzyGreaterOp
/// - Min/Max: MaxOp, MinOp
/// - Unary: NegOp, AbsOp, SquareOp
/// - Assignment: AssignOp
///
/// @par Key Features:
/// - Host and device support via ASC_HOST_DEVICE
/// - Type-generic functors (templated on element type)
/// - Zero runtime overhead (inlined constexpr operators)
/// - Consistent naming convention throughout
///
/// @par Usage:
/// These functors are primarily used internally by:
/// - DenseMArray broadcast operations (operator+=, operator-=, etc.)
/// - Expression templates (BinaryExpr, UnaryExpr)
/// - Generic algorithms requiring operation policies
///
/// @par Design Rationale:
/// Centralizing operation functors in the foundation layer (Layer 0) ensures:
/// - No code duplication across modules
/// - Single source of truth for all operations
/// - Easier maintenance and consistent behavior
/// - Reusability across different abstractions

#include <cmath>
#include <limits>
#include "asc/core/device.h"

namespace asc {

// ============================================================================
// Binary Operation Functors
// ============================================================================

/// @brief Addition functor: a + b
/// @tparam T Element type
template <typename T>
struct AddOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a, const T& b) const {
    return a + b;
  }
};

/// @brief Subtraction functor: a - b
/// @tparam T Element type
template <typename T>
struct SubOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a, const T& b) const {
    return a - b;
  }
};

/// @brief Multiplication functor: a * b
/// @tparam T Element type
template <typename T>
struct MulOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a, const T& b) const {
    return a * b;
  }
};

/// @brief Division functor: a / b
/// @tparam T Element type
template <typename T>
struct DivOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a, const T& b) const {
    return a / b;
  }
};

/// @brief Assignment functor: a = b (returns b, ignores a)
/// @tparam T Element type
template <typename T>
struct AssignOp {
  ASC_HOST_DEVICE constexpr T operator()(const T&, const T& b) const {
    return b;
  }
};

// ============================================================================
// Unary Operation Functors
// ============================================================================

/// @brief Negation functor: -a
/// @tparam T Element type
template <typename T>
struct NegOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a) const { return -a; }
};

/// @brief Absolute value functor: |a|
/// @tparam T Element type
template <typename T>
struct AbsOp {
  ASC_HOST_DEVICE T operator()(const T& a) const {
    return (a >= T(0)) ? a : -a;
  }
};

/// @brief Square functor: a²
/// @tparam T Element type
template <typename T>
struct SquareOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a) const { return a * a; }
};

// ============================================================================
// Comparison Operation Functors
// ============================================================================

/// @brief Equality functor: a == b
/// @tparam T Element type
template <typename T>
struct EqualOp {
  ASC_HOST_DEVICE bool operator()(const T& a, const T& b) const {
    return a == b;
  }
};

/// @brief Inequality functor: a != b
/// @tparam T Element type
template <typename T>
struct NotEqualOp {
  ASC_HOST_DEVICE bool operator()(const T& a, const T& b) const {
    return a != b;
  }
};

/// @brief Less than functor: a < b
/// @tparam T Element type
template <typename T>
struct LessOp {
  ASC_HOST_DEVICE constexpr bool operator()(const T& a, const T& b) const {
    return a < b;
  }
};

/// @brief Less than or equal functor: a <= b
/// @tparam T Element type
template <typename T>
struct LessEqualOp {
  ASC_HOST_DEVICE constexpr bool operator()(const T& a, const T& b) const {
    return a <= b;
  }
};

/// @brief Greater than functor: a > b
/// @tparam T Element type
template <typename T>
struct GreaterOp {
  ASC_HOST_DEVICE constexpr bool operator()(const T& a, const T& b) const {
    return a > b;
  }
};

/// @brief Greater than or equal functor: a >= b
/// @tparam T Element type
template <typename T>
struct GreaterEqualOp {
  ASC_HOST_DEVICE constexpr bool operator()(const T& a, const T& b) const {
    return a >= b;
  }
};

// ============================================================================
// Fuzzy Comparison Operation Functors
// ============================================================================

/// @brief Fuzzy equality functor: |a - b| <= tol
/// @tparam T Element type
template <typename T>
struct FuzzyEqualOp {
  T tol_;

  /// @brief Construct with tolerance
  /// @param tol Tolerance (default: machine epsilon)
  ASC_HOST_DEVICE explicit FuzzyEqualOp(
      T tol = std::numeric_limits<T>::epsilon())
      : tol_(tol) {}

  ASC_HOST_DEVICE bool operator()(const T& a, const T& b) const {
    const T abs_a = AbsOp<T>()(a);
    const T abs_b = AbsOp<T>()(b);
    const T max_abs = abs_a < abs_b ? abs_b : abs_a;
    const T scale = max_abs < T(1) ? T(1) : max_abs;
    return AbsOp<T>()(a - b) <= tol_ * scale;
  }
};

/// @brief Fuzzy inequality functor: |a - b| > tol
/// @tparam T Element type
template <typename T>
struct FuzzyNotEqualOp {
  T tol_;

  /// @brief Construct with tolerance
  /// @param tol Tolerance (default: machine epsilon)
  ASC_HOST_DEVICE explicit FuzzyNotEqualOp(
      T tol = std::numeric_limits<T>::epsilon())
      : tol_(tol) {}

  ASC_HOST_DEVICE bool operator()(const T& a, const T& b) const {
    const T abs_a = AbsOp<T>()(a);
    const T abs_b = AbsOp<T>()(b);
    const T max_abs = abs_a < abs_b ? abs_b : abs_a;
    const T scale = max_abs < T(1) ? T(1) : max_abs;
    return AbsOp<T>()(a - b) > tol_ * scale;
  }
};

/// @brief Fuzzy less than functor: a < b - tol
/// @tparam T Element type
template <typename T>
struct FuzzyLessOp {
  T tol_;

  ASC_HOST_DEVICE explicit FuzzyLessOp(
      T tol = std::numeric_limits<T>::epsilon())
      : tol_(tol) {}

  ASC_HOST_DEVICE bool operator()(const T& a, const T& b) const {
    const T abs_a = AbsOp<T>()(a);
    const T abs_b = AbsOp<T>()(b);
    const T max_abs = abs_a < abs_b ? abs_b : abs_a;
    const T scale = max_abs < T(1) ? T(1) : max_abs;
    return a < b - tol_ * scale;
  }
};

/// @brief Fuzzy greater than functor: a > b + tol
/// @tparam T Element type
template <typename T>
struct FuzzyGreaterOp {
  T tol_;

  ASC_HOST_DEVICE explicit FuzzyGreaterOp(
      T tol = std::numeric_limits<T>::epsilon())
      : tol_(tol) {}

  ASC_HOST_DEVICE bool operator()(const T& a, const T& b) const {
    const T abs_a = AbsOp<T>()(a);
    const T abs_b = AbsOp<T>()(b);
    const T max_abs = abs_a < abs_b ? abs_b : abs_a;
    const T scale = max_abs < T(1) ? T(1) : max_abs;
    return a > b + tol_ * scale;
  }
};

// ============================================================================
// Min/Max Operation Functors
// ============================================================================

/// @brief Maximum functor: max(a, b)
/// @tparam T Element type
template <typename T>
struct MaxOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a, const T& b) const {
    return (a < b) ? b : a;
  }
};

/// @brief Minimum functor: min(a, b)
/// @tparam T Element type
template <typename T>
struct MinOp {
  ASC_HOST_DEVICE constexpr T operator()(const T& a, const T& b) const {
    return (a < b) ? a : b;
  }
};

}  // namespace asc

#endif  // ASC_OPERATORS_H_
