// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_ARRAY_ACCESSOR_H_
#define ASC_ARRAY_ACCESSOR_H_

#include <concepts>
#include <type_traits>

#include "asc/core/types.h"

namespace asc {

/// @brief Stateless direct pointer accessor for host-addressable tensor data.
template <typename Element>
class DefaultAccessor {
  static_assert(!std::is_reference_v<Element>,
                "A tensor accessor element cannot be a reference");
  static_assert(!std::is_void_v<Element>,
                "A tensor accessor element cannot be void");

 public:
  using ElementType = Element;
  using ValueType = std::remove_cv_t<Element>;
  using DataHandle = Element*;
  using Reference = Element&;

  constexpr DefaultAccessor() noexcept = default;

  template <typename OtherElement>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<Element>, OtherElement>)
  constexpr DefaultAccessor(
      const DefaultAccessor<OtherElement>& /*other*/) noexcept {}

  constexpr Reference Access(DataHandle data, index_t offset) const noexcept {
    return data[offset];
  }

  constexpr DataHandle Offset(DataHandle data, index_t offset) const noexcept {
    return data + offset;
  }
};

}  // namespace asc

#endif  // ASC_ARRAY_ACCESSOR_H_
