// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_ARRAY_TENSOR_CONCEPTS_H_
#define ASC_ARRAY_TENSOR_CONCEPTS_H_

#include <concepts>
#include <cstddef>
#include <type_traits>

#include "asc/core/types.h"

namespace asc {

/// @brief Structural contract for an observable tensor layout mapping.
template <typename T>
concept TensorMapping = requires(const std::remove_cvref_t<T>& mapping,
                                 std::size_t dimension) {
  typename std::remove_cvref_t<T>::ExtentsType;
  {
    std::remove_cvref_t<T>::Rank()
  } -> std::same_as<std::size_t>;
  {
    mapping.GetExtents()
  } -> std::same_as<
      const typename std::remove_cvref_t<T>::ExtentsType&>;
  { mapping.GetExtent(dimension) } -> std::same_as<extent_t>;
  { mapping.GetStride(dimension) } -> std::same_as<stride_t>;
  { mapping.GetSize() } -> std::same_as<extent_t>;
  { mapping.GetRequiredSpan() } -> std::same_as<extent_t>;
  { mapping.IsUnique() } -> std::same_as<bool>;
  { mapping.IsExhaustive() } -> std::same_as<bool>;
  { mapping.IsContiguous() } -> std::same_as<bool>;
};

/// @brief Structural contract for fixed-rank tensor descriptor observation.
template <typename T>
concept TensorDescriptor =
    TensorMapping<typename std::remove_cvref_t<T>::MappingType> &&
    requires(const std::remove_cvref_t<T>& tensor, std::size_t dimension) {
      typename std::remove_cvref_t<T>::ElementType;
      typename std::remove_cvref_t<T>::ValueType;
      typename std::remove_cvref_t<T>::ExtentsType;
      typename std::remove_cvref_t<T>::MappingType;
      {
        std::remove_cvref_t<T>::Rank()
      } -> std::same_as<std::size_t>;
      {
        tensor.GetExtents()
      } -> std::same_as<
          const typename std::remove_cvref_t<T>::ExtentsType&>;
      {
        tensor.GetMapping()
      } -> std::same_as<
          const typename std::remove_cvref_t<T>::MappingType&>;
      { tensor.GetExtent(dimension) } -> std::same_as<extent_t>;
      { tensor.GetStride(dimension) } -> std::same_as<stride_t>;
      { tensor.GetSize() } -> std::same_as<extent_t>;
    };

/// @brief Descriptor whose data handle provides readable elements.
template <typename T>
concept ReadableTensor =
    TensorDescriptor<T> &&
    requires(const std::remove_cvref_t<T>& tensor) {
      typename std::remove_cvref_t<T>::DataHandle;
      {
        tensor.Data()
      } -> std::same_as<typename std::remove_cvref_t<T>::DataHandle>;
      {
        *tensor.Data()
      } -> std::convertible_to<
          const typename std::remove_cvref_t<T>::ValueType&>;
    };

/// @brief Readable descriptor whose element type carries mutation authority.
template <typename T>
concept WritableTensor =
    ReadableTensor<T> &&
    !std::is_const_v<typename std::remove_cvref_t<T>::ElementType> &&
    requires(const std::remove_cvref_t<T>& tensor) {
      {
        *tensor.Data()
      } -> std::same_as<typename std::remove_cvref_t<T>::ElementType&>;
    };

/// @brief Tensor whose mapping type is unconditionally contiguous.
template <typename T>
concept ContiguousTensor =
    TensorDescriptor<T> &&
    std::remove_cvref_t<T>::MappingType::IsAlwaysContiguous();

/// @brief Tensor descriptor of exactly rank one.
template <typename T>
concept Vector = TensorDescriptor<T> && (std::remove_cvref_t<T>::Rank() == 1);

/// @brief Tensor descriptor of exactly rank two.
template <typename T>
concept Matrix = TensorDescriptor<T> && (std::remove_cvref_t<T>::Rank() == 2);

}  // namespace asc

#endif  // ASC_ARRAY_TENSOR_CONCEPTS_H_
