#ifndef ASC_TESTS_COMPILE_M4_MULTI_TU_H_
#define ASC_TESTS_COMPILE_M4_MULTI_TU_H_

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

template <typename Element>
struct M4CompileVector {
  Element* data;
  asc::extent_t size;
};

namespace asc {

template <typename Element>
struct ExpressionAdapter<::M4CompileVector<Element>> {
  using value_type = std::remove_const_t<Element>;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static constexpr std::array<extent_t, 1> Shape(
      const ::M4CompileVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr value_type Read(
      const ::M4CompileVector<Element>& vector,
      std::span<const index_t, 1> coordinate) noexcept {
    return vector.data[static_cast<std::size_t>(coordinate[0])];
  }

  static constexpr bool MayAlias(const ::M4CompileVector<Element>& vector,
                                 AliasToken token) noexcept {
    return vector.data == token.identity();
  }
};

template <typename Element>
struct ExpressionPlacementAdapter<::M4CompileVector<Element>> {
  static constexpr MemorySpace Space(
      const ::M4CompileVector<Element>&) noexcept {
    return MemorySpace::kHost;
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4CompileVector<Element>& vector) noexcept {
    return ExpressionAliasMetadata(vector.data, vector.data,
                                   static_cast<std::size_t>(vector.size) *
                                       sizeof(std::remove_const_t<Element>));
  }
};

template <typename Element>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<::M4CompileVector<Element>> {
  using value_type = std::remove_const_t<Element>;

  static constexpr std::array<extent_t, 1> Shape(
      const ::M4CompileVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4CompileVector<Element>& vector) noexcept {
    return ExpressionPlacementAdapter<::M4CompileVector<Element>>::Alias(
        vector);
  }

  static constexpr bool IsUnique(const ::M4CompileVector<Element>&) noexcept {
    return true;
  }

  static constexpr void Write(::M4CompileVector<Element>& vector,
                              std::span<const index_t, 1> coordinate,
                              value_type value) noexcept {
    vector.data[static_cast<std::size_t>(coordinate[0])] = value;
  }
};

}  // namespace asc

double M4CoordinateFromFirstTranslationUnit();
double M4SpmvFromSecondTranslationUnit();

#endif  // ASC_TESTS_COMPILE_M4_MULTI_TU_H_
