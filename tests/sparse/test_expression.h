#ifndef ASC_TESTS_SPARSE_TEST_EXPRESSION_H_
#define ASC_TESTS_SPARSE_TEST_EXPRESSION_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

template <typename Element, std::size_t Rank, asc::SparsityEffect Effect>
struct M4TestExpression {
  const Element* values = nullptr;
  std::array<asc::extent_t, Rank> shape{};
  asc::MemorySpace memory_space = asc::MemorySpace::kHost;
  std::size_t alias_bytes = 0;
  bool access_valid = true;
};

namespace asc {

template <typename Element, std::size_t Rank, SparsityEffect Effect>
struct ExpressionAdapter<::M4TestExpression<Element, Rank, Effect>> {
  using value_type = Element;
  static constexpr rank_t rank = static_cast<rank_t>(Rank);
  static constexpr SparsityEffect sparsity_effect = Effect;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kExternal;

  static constexpr std::array<extent_t, Rank> Shape(
      const ::M4TestExpression<Element, Rank, Effect>& expression) noexcept {
    return expression.shape;
  }

  static Element Read(
      const ::M4TestExpression<Element, Rank, Effect>& expression,
      std::span<const index_t, Rank> coordinate) noexcept {
    std::size_t offset = 0;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      offset = offset * static_cast<std::size_t>(expression.shape[dimension]) +
               static_cast<std::size_t>(coordinate[dimension]);
    }
    return expression.values[offset];
  }

  static bool MayAlias(
      const ::M4TestExpression<Element, Rank, Effect>& expression,
      AliasToken token) noexcept {
    if (expression.values == nullptr || expression.alias_bytes == 0 ||
        token.identity() == nullptr) {
      return false;
    }
    const std::uintptr_t begin =
        reinterpret_cast<std::uintptr_t>(expression.values);
    if (expression.alias_bytes > UINTPTR_MAX - begin) {
      return true;
    }
    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(token.identity());
    return address >= begin && address < begin + expression.alias_bytes;
  }

  static Status ValidateAccess(
      const ::M4TestExpression<Element, Rank, Effect>& expression,
      const ExecutionContext& context) {
    if (!expression.access_valid) {
      return Status(ErrorCode::kMemoryAccess,
                    "Injected sparse expression access failure");
    }
    if (context.backend() != Backend::kSerial) {
      return Status(ErrorCode::kUnsupported,
                    "Sparse test expression requires serial execution");
    }
    if (expression.memory_space != MemorySpace::kHost ||
        !context.CanAccess(expression.memory_space)) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse test expression requires host storage");
    }
    return Status::Ok();
  }
};

template <typename Element, std::size_t Rank, SparsityEffect Effect>
struct ExpressionPlacementAdapter<::M4TestExpression<Element, Rank, Effect>> {
  static constexpr MemorySpace Space(
      const ::M4TestExpression<Element, Rank, Effect>& expression) noexcept {
    return expression.memory_space;
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4TestExpression<Element, Rank, Effect>& expression) noexcept {
    return ExpressionAliasMetadata(expression.values, expression.values,
                                   expression.alias_bytes);
  }
};

}  // namespace asc

#endif  // ASC_TESTS_SPARSE_TEST_EXPRESSION_H_
