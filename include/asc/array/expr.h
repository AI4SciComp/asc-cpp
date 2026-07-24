// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/array/expr.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_EXPR_H_
#define ASC_EXPR_H_

/// @file expr.h
/// @brief Expression template system for lazy element-wise object operations
///
/// This module implements a lightweight expression template system that
/// eliminates temporary allocations for compound object expressions. Instead
/// of eagerly evaluating each operation, expressions build an abstract syntax
/// tree (AST) that is evaluated only when assigned to a destination object.
///
/// @par Benefits:
/// - Zero temporaries for compound expressions like `a + b * c - d`
/// - Single-pass evaluation with automatic loop fusion
/// - Type-safe expression building at compile-time
/// - Compatible with heterogeneous computing (CPU/GPU)
/// - Memory-efficient for large element-wise operations
///
/// @par Design:
/// - Expression types hold references (no ownership, lightweight)
/// - Evaluation is deferred until assignment (`operator=`)
/// - Supports arbitrary expression depth (limited only by compiler)
/// - GPU kernels fuse entire expression into single kernel launch
///
/// @par Usage Examples:
/// @code
/// using namespace asc;
///
/// VectorXr a(1000), b(1000), c(1000), d(1000);
/// a = 1.0; b = 2.0; c = 3.0;
///
/// // Traditional: Creates 2 temporaries
/// // VectorXr temp1 = b * c;      // temporary 1
/// // VectorXr temp2 = a + temp1;  // temporary 2
/// // d = temp2 - 1.0;             // temporary 3
///
/// // Expression template: Zero temporaries, single loop
/// d = a + b * c - 1.0;  // Single-pass evaluation
///
/// // Works with any complexity
/// d = 2.0 * a + (b - c) / (a + 1.0);  // Still zero temps
///
/// // In-place evaluation with <<= operator
/// VectorXr f(1000);
/// f <<= a + b * c;  // Evaluates into existing f
/// @endcode
///
/// @par Performance Notes:
/// - Expression building is zero-cost (happens at compile-time)
/// - Loop fusion improves cache locality and reduces memory traffic
/// - GPU kernels benefit from reduced kernel launch overhead
/// - Best for element-wise operations; use specialized routines for BLAS
///
/// @see blas.h for eager evaluation functions and BLAS operations
/// @see marray.h for dense and sparse array object types

#include <type_traits>
#include <utility>

#include "asc/core/globals.h"
#include "asc/core/forall.h"
#include "asc/core/operators.h"
#include "asc/array/concepts.h"
#include "asc/array/mlayout.h"

namespace asc {

template <typename E>
class Expression;

template <typename T>
struct IsExpression
    : std::bool_constant<std::is_base_of_v<Expression<T>, T>> {};

template <typename E>
struct IsExpression<Expression<E>> : std::true_type {};

template <typename T>
concept ExpressionLike = IsExpression<std::decay_t<T>>::value;

namespace internal {

template <typename Object>
constexpr bool supports_device_expression() {
  if constexpr (requires { std::decay_t<Object>::SupportsDeviceExpression; }) {
    return std::decay_t<Object>::SupportsDeviceExpression;
  } else {
    return false;
  }
}

template <typename Object>
concept flat_expression_object =
    requires(const std::decay_t<Object>& obj, bool use_dev) {
      typename std::decay_t<Object>::ValueType;
      { obj.GetSize() } -> std::convertible_to<int>;
      { obj.UseDevice() } -> std::convertible_to<bool>;
      {
        obj.Read(use_dev)
      } -> std::convertible_to<const typename std::decay_t<Object>::ValueType*>;
      {
        obj.HostRead()
      } -> std::convertible_to<const typename std::decay_t<Object>::ValueType*>;
    };

template <typename Object>
concept sparse_expression_object =
    SparseMArrayLike<Object> &&
    requires(const std::decay_t<Object>& obj, const int* index) {
      typename std::decay_t<Object>::ValueType;
      typename std::decay_t<Object>::ShapeType;
      { obj.GetSize() } -> std::convertible_to<int>;
      {
        obj.GetShape()
      } -> std::same_as<const typename std::decay_t<Object>::ShapeType&>;
      {
        obj.At(index)
      } -> std::convertible_to<typename std::decay_t<Object>::ValueType>;
    };

template <typename Object>
concept readable_expression_object =
    flat_expression_object<Object> || sparse_expression_object<Object>;

template <typename Object>
concept writable_expression_object =
    flat_expression_object<Object> &&
    requires(std::decay_t<Object>& obj, bool use_dev) {
      {
        obj.ReadWrite(use_dev)
      } -> std::convertible_to<typename std::decay_t<Object>::ValueType*>;
      {
        obj.HostReadWrite()
      } -> std::convertible_to<typename std::decay_t<Object>::ValueType*>;
    };

}  // namespace internal

// ============================================================================
// Base Expression Class (CRTP)
// ============================================================================

/// @brief Base class for all expression templates using CRTP
/// @tparam E Derived expression type
///
/// Provides the common interface for all expressions. The derived type E
/// must implement:
/// - `operator[](int i) const` - element access
/// - `GetSize() const` - total number of elements
/// - `UseDevice() const` - whether to use GPU
template <typename E>
class Expression {
 public:
  /// @brief Access element at linear index
  /// @param i Linear index
  /// @return Value at index i
  ASC_HOST_DEVICE inline auto operator[](int i) const {
    return static_cast<const E&>(*this)[i];
  }

  /// @brief Get total number of elements
  /// @return Size of expression
  inline int GetSize() const { return static_cast<const E&>(*this).GetSize(); }

  /// @brief Check if expression should use device execution
  /// @return True if GPU execution requested
  inline bool UseDevice() const {
    return static_cast<const E&>(*this).UseDevice();
  }

  /// @brief Cast to derived type
  /// @return Reference to derived expression
  inline const E& self() const { return static_cast<const E&>(*this); }

  /// @brief Evaluate expression into destination object
  /// @tparam Dest Destination object type
  /// @param dest Destination object
  template <internal::writable_expression_object Dest>
  inline void EvalTo(Dest& dest) const {
    const E& derived = self();
    const int N = derived.GetSize();
    ASC_VERIFY(N == dest.GetSize(),
                  "expression and destination size mismatch");

    bool use_dev = false;
    if constexpr (E::SupportsDevice &&
                  internal::supports_device_expression<Dest>()) {
      use_dev = dest.UseDevice() && derived.UseDevice();
    }

    if constexpr (E::SupportsDevice &&
                  internal::supports_device_expression<Dest>()) {
      if (use_dev) {
        auto* dest_data = dest.ReadWrite(true);
        ASC_FORALL_SWITCH(true, i, N, dest_data[i] = derived[i];);
        return;
      }
    }

    auto* dest_data = dest.HostReadWrite();
    if constexpr (DenseMArrayLike<Dest>) {
      using DestShape = typename Dest::ShapeType;
      LayoutLeft::Map<DestShape> canonical_map(dest.GetShape());
      for (int i = 0; i < N; ++i) {
        if constexpr (DestShape::GetRank() == 0) {
          dest_data[0] = derived[i];
        } else {
          int index[DestShape::GetRank()];
          canonical_map.Unfold(i, index);
          dest_data[dest.GetMap().Fold(index)] = derived[i];
        }
      }
    } else {
      for (int i = 0; i < N; ++i) {
        dest_data[i] = derived[i];
      }
    }
  }

  /// @brief In-place evaluation operator (friend function)
  /// @tparam Dest Destination object type
  /// @param dest Destination object
  /// @param expr Expression to evaluate
  /// @return Reference to destination object
  ///
  /// @par Usage:
  /// @code
  /// VectorXr a(10), b(10), c(10);
  /// a = 1.0; b = 2.0;
  /// c <<= a + b * 2.0;  // In-place evaluation
  /// @endcode
  template <internal::writable_expression_object Dest>
  friend inline Dest& operator<<=(Dest& dest, const Expression<E>& expr) {
    ASC_VERIFY(expr.GetSize() == dest.GetSize(),
                  "expression and destination size mismatch");
    expr.self().EvalTo(dest);
    return dest;
  }
};

// ============================================================================
// Terminal Expressions - Objects and Scalars
// ============================================================================

template <typename Object>
class ObjectExpr;

/// @brief Object reference expression for flat readable objects.
/// @tparam Object Object type exposing GetSize(), Read(), and HostRead().
template <typename Object>
  requires(!DenseMArrayLike<Object> &&
           !SparseMArrayLike<Object> &&
           internal::flat_expression_object<Object>)
class ObjectExpr<Object> : public Expression<ObjectExpr<Object>> {
 public:
  using ValueType = typename Object::ValueType;

  static constexpr bool SupportsDevice =
      internal::supports_device_expression<Object>();

  /// @brief Construct from object reference.
  /// @param obj Object to wrap.
  explicit ObjectExpr(const Object& obj)
      : data_(SupportsDevice ? obj.Read(obj.UseDevice()) : obj.HostRead()),
        size_(obj.GetSize()),
        use_dev_(SupportsDevice && obj.UseDevice()) {}

  /// @brief Access element at linear index.
  ASC_HOST_DEVICE inline ValueType operator[](int i) const {
    return data_[i];
  }

  /// @brief Get total size.
  inline int GetSize() const { return size_; }

  /// @brief Check device usage.
  inline bool UseDevice() const { return use_dev_; }

 private:
  const ValueType* data_;
  int size_;
  bool use_dev_;
};

/// @brief Object reference expression for SparseMArray objects.
/// @tparam Object SparseMArray type.
///
/// @note Sparse expressions expose logical dense-order values while preserving
/// sparse storage. Missing sparse entries evaluate as zero.
template <SparseMArrayLike Object>
class ObjectExpr<Object> : public Expression<ObjectExpr<Object>> {
 public:
  using ValueType = typename Object::ValueType;
  using ShapeType = typename Object::ShapeType;

  static constexpr bool SupportsDevice = false;

  /// @brief Construct from sparse object reference.
  /// @param obj Sparse object to wrap.
  explicit ObjectExpr(const Object& obj)
      : obj_(&obj), canonical_map_(obj.GetShape()), size_(obj.GetSize()) {}

  /// @brief Access element at logical dense-order index.
  ASC_HOST_DEVICE inline ValueType operator[](int i) const {
#if defined(__CUDA_ARCH__)
    (void)i;
    return ValueType{};
#else
    int index[ShapeType::GetRank()];
    canonical_map_.Unfold(i, index);
    return obj_->At(index);
#endif
  }

  /// @brief Get total dense size.
  inline int GetSize() const { return size_; }

  /// @brief Check device usage.
  inline bool UseDevice() const { return false; }

 private:
  const Object* obj_;
  LayoutLeft::Map<ShapeType> canonical_map_;
  int size_;
};

/// @brief Object reference expression for DenseMArray objects.
/// @tparam Object DenseMArray type.
///
/// @note This specialization preserves shape-indexed expression order even
/// when the source dense array is not stored in canonical LayoutLeft order.
template <DenseMArrayLike Object>
class ObjectExpr<Object> : public Expression<ObjectExpr<Object>> {
 public:
  using ValueType = typename Object::ValueType;
  using ShapeType = typename Object::ShapeType;
  using LayoutType = typename Object::LayoutType;
  using MapType = typename Object::MapType;

  static constexpr bool SupportsDevice = Object::SupportsDeviceExpression;

  /// @brief Construct from object reference
  /// @param obj Object to wrap
  explicit ObjectExpr(const Object& obj)
      : data_(SupportsDevice ? obj.Read(obj.UseDevice()) : obj.HostRead()),
        map_(obj.GetMap()),
        canonical_map_(obj.GetShape()),
        size_(obj.GetSize()),
        use_dev_(SupportsDevice && obj.UseDevice()) {}

  /// @brief Access element at linear index
  ASC_HOST_DEVICE inline ValueType operator[](int i) const {
    if constexpr (SupportsDevice) {
      return data_[i];
    } else if constexpr (ShapeType::GetRank() == 0) {
      return data_[0];
    } else {
      int index[ShapeType::GetRank()];
      canonical_map_.Unfold(i, index);
      return data_[map_.Fold(index)];
    }
  }

  /// @brief Get total size
  inline int GetSize() const { return size_; }

  /// @brief Check device usage
  inline bool UseDevice() const { return use_dev_; }

 private:
  const ValueType* data_;
  MapType map_;
  LayoutLeft::Map<ShapeType> canonical_map_;
  int size_;
  bool use_dev_;
};

/// @brief Scalar expression (broadcasts scalar to all elements)
/// @tparam T Scalar type
template <ScalarLike T>
class ScalarExpr : public Expression<ScalarExpr<T>> {
 public:
  using ValueType = T;
  static constexpr bool SupportsDevice = true;

  /// @brief Construct from scalar value
  /// @param val Scalar value
  /// @param size Number of elements to broadcast to
  /// @param use_dev Whether to use GPU
  ScalarExpr(T val, int size, bool use_dev = false)
      : val_(val), size_(size), use_dev_(use_dev) {}

  /// @brief Access element (always returns same scalar)
  ASC_HOST_DEVICE inline T operator[](int i) const {
    (void)i;  // Suppress unused warning
    return val_;
  }

  /// @brief Get size
  inline int GetSize() const { return size_; }

  /// @brief Check device usage
  inline bool UseDevice() const { return use_dev_; }

 private:
  T val_;
  int size_;
  bool use_dev_;
};

// ============================================================================
// Binary Expressions
// ============================================================================

/// @brief Binary operation expression
/// @tparam Lhs Left-hand side expression type
/// @tparam Rhs Right-hand side expression type
/// @tparam Op Binary operation functor
///
/// @note This class stores operands BY VALUE to avoid dangling references
/// when building compound expressions from temporaries. This ensures that
/// expressions like `(a + b) * c` work correctly, where `(a + b)` is a
/// temporary BinaryExpr that would otherwise be destroyed.
template <typename Lhs, typename Rhs, typename Op>
class BinaryExpr : public Expression<BinaryExpr<Lhs, Rhs, Op>> {
 public:
  using ValueType = decltype(std::declval<Op>()(
      std::declval<typename Lhs::ValueType>(),
      std::declval<typename Rhs::ValueType>()));
  static constexpr bool SupportsDevice =
      Lhs::SupportsDevice && Rhs::SupportsDevice;

  /// @brief Construct binary expression
  /// @param lhs Left operand (copied/moved)
  /// @param rhs Right operand (copied/moved)
  BinaryExpr(const Expression<Lhs>& lhs, const Expression<Rhs>& rhs)
      : lhs_(lhs.self()), rhs_(rhs.self()), op_() {
    ASC_VERIFY(lhs_.GetSize() == rhs_.GetSize(),
                  "expression operands must have the same size");
  }

  /// @brief Access element at index
  ASC_HOST_DEVICE inline ValueType operator[](int i) const {
    return op_(lhs_[i], rhs_[i]);
  }

  /// @brief Get size (uses Lhs size)
  inline int GetSize() const { return lhs_.GetSize(); }

  /// @brief Check device usage (uses Lhs setting)
  inline bool UseDevice() const { return lhs_.UseDevice() && rhs_.UseDevice(); }

 private:
  Lhs lhs_;  // Store by value, not reference
  Rhs rhs_;  // Store by value, not reference
  Op op_;
};

// ============================================================================
// Unary Expressions
// ============================================================================

/// @brief Unary operation expression
/// @tparam E Operand expression type
/// @tparam Op Unary operation functor
///
/// @note This class stores operand BY VALUE to avoid dangling references
template <typename E, typename Op>
class UnaryExpr : public Expression<UnaryExpr<E, Op>> {
 public:
  using ValueType =
      decltype(std::declval<Op>()(std::declval<typename E::ValueType>()));
  static constexpr bool SupportsDevice = E::SupportsDevice;

  /// @brief Construct unary expression
  /// @param expr Operand expression (copied/moved)
  explicit UnaryExpr(const Expression<E>& expr) : expr_(expr.self()), op_() {}

  /// @brief Access element at index
  ASC_HOST_DEVICE inline ValueType operator[](int i) const {
    return op_(expr_[i]);
  }

  /// @brief Get size
  inline int GetSize() const { return expr_.GetSize(); }

  /// @brief Check device usage
  inline bool UseDevice() const { return expr_.UseDevice(); }

 private:
  E expr_;  // Store by value, not reference
  Op op_;
};

// ============================================================================
// Expression Building - Operator Overloads
// ============================================================================
//
// Note: Operation functors (AddOp, SubOp, MulOp, DivOp, NegOp) are now
// defined in generic/operators.h to eliminate code duplication between
// mtensor.h and expr.h. See generic/operators.h for implementation.
// ============================================================================

/// @brief Addition: Expression + Expression
template <typename Lhs, typename Rhs>
inline auto operator+(const Expression<Lhs>& lhs, const Expression<Rhs>& rhs) {
  using T =
      std::common_type_t<typename Lhs::ValueType, typename Rhs::ValueType>;
  return BinaryExpr<Lhs, Rhs, AddOp<T>>(lhs, rhs);
}

/// @brief Addition: Object + Expression
template <internal::readable_expression_object Object, typename E>
inline auto operator+(const Object& obj, const Expression<E>& expr) {
  return ObjectExpr<Object>(obj) + expr;
}

/// @brief Addition: Expression + Object
template <typename E, internal::readable_expression_object Object>
inline auto operator+(const Expression<E>& expr, const Object& obj) {
  return expr + ObjectExpr<Object>(obj);
}

/// @brief Addition: Expression + Scalar
template <typename E, ScalarLike T>
inline auto operator+(const Expression<E>& expr, T scalar) {
  return expr + ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice());
}

/// @brief Addition: Scalar + Expression
template <ScalarLike T, typename E>
inline auto operator+(T scalar, const Expression<E>& expr) {
  return ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice()) + expr;
}

/// @brief Subtraction: Expression - Expression
template <typename Lhs, typename Rhs>
inline auto operator-(const Expression<Lhs>& lhs, const Expression<Rhs>& rhs) {
  using T =
      std::common_type_t<typename Lhs::ValueType, typename Rhs::ValueType>;
  return BinaryExpr<Lhs, Rhs, SubOp<T>>(lhs, rhs);
}

/// @brief Subtraction: Object - Expression
template <internal::readable_expression_object Object, typename E>
inline auto operator-(const Object& obj, const Expression<E>& expr) {
  return ObjectExpr<Object>(obj) - expr;
}

/// @brief Subtraction: Expression - Object
template <typename E, internal::readable_expression_object Object>
inline auto operator-(const Expression<E>& expr, const Object& obj) {
  return expr - ObjectExpr<Object>(obj);
}

/// @brief Subtraction: Expression - Scalar
template <typename E, ScalarLike T>
inline auto operator-(const Expression<E>& expr, T scalar) {
  return expr - ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice());
}

/// @brief Subtraction: Scalar - Expression
template <ScalarLike T, typename E>
inline auto operator-(T scalar, const Expression<E>& expr) {
  return ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice()) - expr;
}

/// @brief Multiplication: Expression * Expression
template <typename Lhs, typename Rhs>
inline auto operator*(const Expression<Lhs>& lhs, const Expression<Rhs>& rhs) {
  using T =
      std::common_type_t<typename Lhs::ValueType, typename Rhs::ValueType>;
  return BinaryExpr<Lhs, Rhs, MulOp<T>>(lhs, rhs);
}

/// @brief Multiplication: Object * Expression
template <internal::readable_expression_object Object, typename E>
inline auto operator*(const Object& obj, const Expression<E>& expr) {
  return ObjectExpr<Object>(obj) * expr;
}

/// @brief Multiplication: Expression * Object
template <typename E, internal::readable_expression_object Object>
inline auto operator*(const Expression<E>& expr, const Object& obj) {
  return expr * ObjectExpr<Object>(obj);
}

/// @brief Multiplication: Expression * Scalar
template <typename E, ScalarLike T>
inline auto operator*(const Expression<E>& expr, T scalar) {
  return expr * ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice());
}

/// @brief Multiplication: Scalar * Expression
template <ScalarLike T, typename E>
inline auto operator*(T scalar, const Expression<E>& expr) {
  return ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice()) * expr;
}

/// @brief Division: Expression / Expression
template <typename Lhs, typename Rhs>
inline auto operator/(const Expression<Lhs>& lhs, const Expression<Rhs>& rhs) {
  using T =
      std::common_type_t<typename Lhs::ValueType, typename Rhs::ValueType>;
  return BinaryExpr<Lhs, Rhs, DivOp<T>>(lhs, rhs);
}

/// @brief Division: Object / Expression
template <internal::readable_expression_object Object, typename E>
inline auto operator/(const Object& obj, const Expression<E>& expr) {
  return ObjectExpr<Object>(obj) / expr;
}

/// @brief Division: Expression / Object
template <typename E, internal::readable_expression_object Object>
inline auto operator/(const Expression<E>& expr, const Object& obj) {
  return expr / ObjectExpr<Object>(obj);
}

/// @brief Division: Expression / Scalar
template <typename E, ScalarLike T>
inline auto operator/(const Expression<E>& expr, T scalar) {
  return expr / ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice());
}

/// @brief Division: Scalar / Expression
template <ScalarLike T, typename E>
inline auto operator/(T scalar, const Expression<E>& expr) {
  return ScalarExpr<T>(scalar, expr.GetSize(), expr.UseDevice()) / expr;
}

/// @brief Negation: -Expression
template <typename E>
inline auto operator-(const Expression<E>& expr) {
  using T = typename E::ValueType;
  return UnaryExpr<E, NegOp<T>>(expr);
}

// ============================================================================
// Object Bridging Operators
// ============================================================================
//
// These operators bridge readable objects to the expression template system by
// wrapping operands in ObjectExpr. This enables lazy evaluation:
//   VectorXr c = a + b * 2.0;  // Zero temporaries!
//
// Without these, users would need to manually wrap: ObjectExpr<T>(a) + ...
// ============================================================================

/// @brief Addition: Object + Object (lazy evaluation)
template <internal::readable_expression_object Lhs,
          internal::readable_expression_object Rhs>
inline auto operator+(const Lhs& lhs, const Rhs& rhs) {
  return ObjectExpr<Lhs>(lhs) + ObjectExpr<Rhs>(rhs);
}

/// @brief Addition: Object + Scalar (lazy evaluation)
template <internal::readable_expression_object Object, ScalarLike Scalar>
inline auto operator+(const Object& obj, Scalar scalar) {
  return ObjectExpr<Object>(obj) + scalar;
}

/// @brief Addition: Scalar + Object (lazy evaluation)
template <ScalarLike Scalar, internal::readable_expression_object Object>
inline auto operator+(Scalar scalar, const Object& obj) {
  return scalar + ObjectExpr<Object>(obj);
}

/// @brief Subtraction: Object - Object (lazy evaluation)
template <internal::readable_expression_object Lhs,
          internal::readable_expression_object Rhs>
inline auto operator-(const Lhs& lhs, const Rhs& rhs) {
  return ObjectExpr<Lhs>(lhs) - ObjectExpr<Rhs>(rhs);
}

/// @brief Subtraction: Object - Scalar (lazy evaluation)
template <internal::readable_expression_object Object, ScalarLike Scalar>
inline auto operator-(const Object& obj, Scalar scalar) {
  return ObjectExpr<Object>(obj) - scalar;
}

/// @brief Subtraction: Scalar - Object (lazy evaluation)
template <ScalarLike Scalar, internal::readable_expression_object Object>
inline auto operator-(Scalar scalar, const Object& obj) {
  return scalar - ObjectExpr<Object>(obj);
}

/// @brief Multiplication: Object * Object (lazy evaluation)
template <internal::readable_expression_object Lhs,
          internal::readable_expression_object Rhs>
inline auto operator*(const Lhs& lhs, const Rhs& rhs) {
  return ObjectExpr<Lhs>(lhs) * ObjectExpr<Rhs>(rhs);
}

/// @brief Multiplication: Object * Scalar (lazy evaluation)
template <internal::readable_expression_object Object, ScalarLike Scalar>
inline auto operator*(const Object& obj, Scalar scalar) {
  return ObjectExpr<Object>(obj) * scalar;
}

/// @brief Multiplication: Scalar * Object (lazy evaluation)
template <ScalarLike Scalar, internal::readable_expression_object Object>
inline auto operator*(Scalar scalar, const Object& obj) {
  return scalar * ObjectExpr<Object>(obj);
}

/// @brief Division: Object / Object (lazy evaluation)
template <internal::readable_expression_object Lhs,
          internal::readable_expression_object Rhs>
inline auto operator/(const Lhs& lhs, const Rhs& rhs) {
  return ObjectExpr<Lhs>(lhs) / ObjectExpr<Rhs>(rhs);
}

/// @brief Division: Object / Scalar (lazy evaluation)
template <internal::readable_expression_object Object, ScalarLike Scalar>
inline auto operator/(const Object& obj, Scalar scalar) {
  return ObjectExpr<Object>(obj) / scalar;
}

/// @brief Division: Scalar / Object (lazy evaluation)
template <ScalarLike Scalar, internal::readable_expression_object Object>
inline auto operator/(Scalar scalar, const Object& obj) {
  return scalar / ObjectExpr<Object>(obj);
}

/// @brief Unary negation: -Object (lazy evaluation)
template <internal::readable_expression_object Object>
inline auto operator-(const Object& obj) {
  return -ObjectExpr<Object>(obj);
}

}  // namespace asc

#endif  // ASC_EXPR_H_
