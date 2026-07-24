// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/mobject.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_GENERIC_MOBJECT_H_
#define ASC_GENERIC_MOBJECT_H_

/// @file mobject.h
/// @brief CRTP base class for all mathematical objects (dense/sparse arrays)
///
/// This module provides the MObject class, which serves as the common base
/// for all mathematical object types in ASC using the Curiously Recurring
/// Template Pattern (CRTP). It implements all common operations (arithmetic,
/// reductions, comparisons) that work uniformly across different storage types.
///
/// @par Architecture:
/// @code
/// MObject<Derived>  (CRTP base - shape queries + operations)
///   ↓
///   ├─ DenseMArray<T, Shape, Layout>       (dense storage with Read/Write)
///   └─ SparseMArray<T, Shape, Layout>      (sparse storage with GetValues/GetMap)
/// @endcode
///
/// @par Design Principles:
/// - Static polymorphism via CRTP (zero runtime overhead)
/// - Operations implemented once, work for all derived types
/// - Shape queries and arithmetic operations unified across storage types
/// - Memory access patterns specific to storage type (dense vs sparse)
/// - Similar to Eigen's DenseBase and xtensor's xexpression
///
/// @par Required Derived Class Interface:
/// Derived classes must implement:
/// - `GetSize() const -> int` - Total number of elements
/// - `GetRank() const -> int` - Number of dimensions
/// - `GetExtent(int) const -> int` - Extent along dimension
///
/// @par Memory Access:
/// Memory access patterns differ by storage type:
/// - DenseMArray: Provides Read/Write/ReadWrite methods for contiguous data
/// - SparseMArray: Provides GetValues(), GetMap() for sparse data structures

#include "asc/core/globals.h"

namespace asc {

/// @brief CRTP base class for all mathematical objects
///
/// MObject provides common operations for all array types through static
/// polymorphism. It defines shape queries and mathematical operations
/// that work across dense and sparse storage.
///
/// @tparam Derived Most derived class (DenseMArray, SparseMArray, etc.)
template <typename Derived>
class MObject {
 public:
  /// @brief Get reference to derived class (CRTP pattern)
  inline Derived& derived() { return static_cast<Derived&>(*this); }

  /// @brief Get const reference to derived class (CRTP pattern)
  inline const Derived& derived() const {
    return static_cast<const Derived&>(*this);
  }

  // ==========================================================================
  // Shape and Size Query Interface
  // ==========================================================================

  /// @brief Get total number of elements
  inline int GetSize() const { return derived().GetSize(); }

  /// @brief Get rank (number of dimensions)
  inline int GetRank() const { return derived().GetRank(); }

  /// @brief Get extent along given dimension
  inline int GetExtent(int dim) const { return derived().GetExtent(dim); }

  /// @brief Check if array is empty
  inline bool IsEmpty() const { return GetSize() == 0; }

  // ==========================================================================
  // Arithmetic Operations (returns expressions)
  // ==========================================================================

  /// @brief Unary negation: -object
  inline auto operator-() const { return derived().operator-(); }

  /// @brief Binary addition: object + object
  template <typename Other>
  inline auto operator+(const MObject<Other>& other) const {
    return derived().operator+(other);
  }

  /// @brief Binary subtraction: object - object
  template <typename Other>
  inline auto operator-(const MObject<Other>& other) const {
    return derived().operator-(other);
  }

  /// @brief Binary multiplication: object * object (element-wise)
  template <typename Other>
  inline auto operator*(const MObject<Other>& other) const {
    return derived().operator*(other);
  }

  /// @brief Binary division: object / object (element-wise)
  template <typename Other>
  inline auto operator/(const MObject<Other>& other) const {
    return derived().operator/(other);
  }

  /// @brief Scalar addition: object + scalar
  template <Arithmetic U>
  inline auto operator+(U scalar) const {
    return derived().operator+(scalar);
  }

  /// @brief Scalar subtraction: object - scalar
  template <Arithmetic U>
  inline auto operator-(U scalar) const {
    return derived().operator-(scalar);
  }

  /// @brief Scalar multiplication: object * scalar
  template <Arithmetic U>
  inline auto operator*(U scalar) const {
    return derived().operator*(scalar);
  }

  /// @brief Scalar division: object / scalar
  template <Arithmetic U>
  inline auto operator/(U scalar) const {
    return derived().operator/(scalar);
  }

  // ==========================================================================
  // Compound Assignment Operations
  // ==========================================================================

  /// @brief Compound assignment: +=
  template <typename Other>
  inline Derived& operator+=(const MObject<Other>& other) {
    return derived().operator+=(other);
  }

  /// @brief Compound assignment: -=
  template <typename Other>
  inline Derived& operator-=(const MObject<Other>& other) {
    return derived().operator-=(other);
  }

  /// @brief Compound assignment: *=
  template <typename Other>
  inline Derived& operator*=(const MObject<Other>& other) {
    return derived().operator*=(other);
  }

  /// @brief Compound assignment: /=
  template <typename Other>
  inline Derived& operator/=(const MObject<Other>& other) {
    return derived().operator/=(other);
  }

  /// @brief Scalar compound assignment: += scalar
  template <Arithmetic U>
  inline Derived& operator+=(U scalar) {
    return derived().operator+=(scalar);
  }

  /// @brief Scalar compound assignment: -= scalar
  template <Arithmetic U>
  inline Derived& operator-=(U scalar) {
    return derived().operator-=(scalar);
  }

  /// @brief Scalar compound assignment: *= scalar
  template <Arithmetic U>
  inline Derived& operator*=(U scalar) {
    return derived().operator*=(scalar);
  }

  /// @brief Scalar compound assignment: /= scalar
  template <Arithmetic U>
  inline Derived& operator/=(U scalar) {
    return derived().operator/=(scalar);
  }

 protected:
  // Protected constructor to prevent direct instantiation
  MObject() = default;
  ~MObject() = default;
};

template <typename T>
concept MObjectLike =
    requires(T obj, const T const_obj, int dim, bool use_dev) {
      { const_obj.GetSize() } -> std::convertible_to<int>;
      { const_obj.GetRank() } -> std::convertible_to<int>;
      { const_obj.GetExtent(dim) } -> std::convertible_to<int>;
      { const_obj.IsEmpty() } -> std::convertible_to<bool>;
      {
        const_obj.Read(use_dev)
      } -> std::convertible_to<const typename T::ValueType*>;
      { obj.Write(use_dev) } -> std::convertible_to<typename T::ValueType*>;
      { obj.ReadWrite(use_dev) } -> std::convertible_to<typename T::ValueType*>;
      {
        const_obj.HostRead()
      } -> std::convertible_to<const typename T::ValueType*>;
      { obj.HostWrite() } -> std::convertible_to<typename T::ValueType*>;
      { obj.HostReadWrite() } -> std::convertible_to<typename T::ValueType*>;
      { const_obj.UseDevice() } -> std::convertible_to<bool>;
      { obj.UseDevice(use_dev) } -> std::same_as<void>;
      typename T::ValueType;
    };

// ============================================================================
// Free Function Operators (scalar on left)
// ============================================================================

/// @brief Scalar addition: scalar + object
template <Arithmetic U, typename Derived>
inline auto operator+(U scalar, const MObject<Derived>& object) {
  return object + scalar;
}

/// @brief Scalar subtraction: scalar - object
template <Arithmetic U, typename Derived>
inline auto operator-(U scalar, const MObject<Derived>& object);

/// @brief Scalar multiplication: scalar * object
template <Arithmetic U, typename Derived>
inline auto operator*(U scalar, const MObject<Derived>& object) {
  return object * scalar;
}

/// @brief Scalar division: scalar / object
template <Arithmetic U, typename Derived>
inline auto operator/(U scalar, const MObject<Derived>& object);

}  // namespace asc

// Include implementation
#include "asc/array/mobject_impl.h"

#endif  // ASC_GENERIC_MOBJECT_H_
