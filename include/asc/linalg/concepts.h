// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_LINALG_CONCEPTS_H_
#define ASC_LINALG_CONCEPTS_H_

#include <concepts>
#include <cstddef>
#include <type_traits>

#include "asc/array/tensor_concepts.h"
#include "asc/core/memory_space.h"
#include "asc/core/types.h"

namespace asc {

namespace detail {

template <typename T>
concept SupportedLinalgScalar =
    std::same_as<T, float> || std::same_as<T, double>;

template <typename T>
concept ReadableLinalgOperand =
    ReadableTensor<T> &&
    SupportedLinalgScalar<typename std::remove_cvref_t<T>::ValueType> &&
    !std::is_volatile_v<typename std::remove_cvref_t<T>::ElementType> &&
    std::same_as<
        std::remove_const_t<typename std::remove_cvref_t<T>::ElementType>,
        typename std::remove_cvref_t<T>::ValueType> &&
    std::is_pointer_v<typename std::remove_cvref_t<T>::DataHandle> &&
    std::same_as<
        std::remove_pointer_t<typename std::remove_cvref_t<T>::DataHandle>,
        typename std::remove_cvref_t<T>::ElementType> &&
    requires(const std::remove_cvref_t<T>& operand) {
      { operand.GetRequiredSpan() } -> std::same_as<extent_t>;
      { operand.GetAvailableSpan() } -> std::same_as<extent_t>;
      { operand.GetMemorySpace() } -> std::same_as<MemorySpace>;
      { operand.IsContiguous() } -> std::same_as<bool>;
      { operand.GetMapping().IsUnique() } -> std::same_as<bool>;
    };

template <typename T>
concept WritableLinalgOperand =
    ReadableLinalgOperand<T> && WritableTensor<T>;

}  // namespace detail

/// @brief Readable canonical Linalg operand of exact rank one.
template <typename T>
concept ReadableLinalgVector =
    detail::ReadableLinalgOperand<T> && Vector<T>;

/// @brief Mutable-element canonical Linalg operand of exact rank one.
template <typename T>
concept WritableLinalgVector =
    detail::WritableLinalgOperand<T> && Vector<T>;

/// @brief Readable canonical Linalg operand of exact rank two.
template <typename T>
concept ReadableLinalgMatrix =
    detail::ReadableLinalgOperand<T> && Matrix<T>;

/// @brief Mutable-element canonical Linalg operand of exact rank two.
template <typename T>
concept WritableLinalgMatrix =
    detail::WritableLinalgOperand<T> && Matrix<T>;

}  // namespace asc

#endif  // ASC_LINALG_CONCEPTS_H_
