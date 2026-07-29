#ifndef ASC_TESTS_SPARSE_EXTERNAL_VECTOR_H_
#define ASC_TESTS_SPARSE_EXTERNAL_VECTOR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

template <typename Element>
struct M4ExternalVector {
  Element* data = nullptr;
  asc::extent_t size = 0;
  asc::stride_t stride = 1;
  asc::MemorySpace memory_space = asc::MemorySpace::kHost;
  bool unique = true;
};

namespace asc {

template <typename Element>
struct ExpressionAdapter<::M4ExternalVector<Element>> {
  using value_type = std::remove_const_t<Element>;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kExternal;

  static constexpr std::array<extent_t, 1> Shape(
      const ::M4ExternalVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr value_type Read(
      const ::M4ExternalVector<Element>& vector,
      std::span<const index_t, 1> coordinate) noexcept {
    return vector.data[static_cast<std::size_t>(coordinate[0] * vector.stride)];
  }

  static bool MayAlias(const ::M4ExternalVector<Element>& vector,
                       AliasToken token) noexcept {
    if (vector.data == nullptr || vector.size <= 0 ||
        token.identity() == nullptr) {
      return false;
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(vector.data);
    const std::size_t span_bytes =
        static_cast<std::size_t>((vector.size - 1) * vector.stride + 1) *
        sizeof(value_type);
    if (span_bytes > UINTPTR_MAX - begin) {
      return true;
    }
    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(token.identity());
    return address >= begin && address < begin + span_bytes;
  }

  static Status ValidateAccess(const ::M4ExternalVector<Element>& vector,
                               const ExecutionContext& context) {
    if (context.backend() != Backend::kSerial) {
      return Status(ErrorCode::kUnsupported,
                    "M4 external vector requires serial execution");
    }
    if (vector.memory_space != MemorySpace::kHost ||
        !context.CanAccess(vector.memory_space)) {
      return Status(ErrorCode::kMemoryAccess,
                    "M4 external vector requires host storage");
    }
    return Status::Ok();
  }
};

template <typename Element>
struct ExpressionPlacementAdapter<::M4ExternalVector<Element>> {
  static constexpr MemorySpace Space(
      const ::M4ExternalVector<Element>& vector) noexcept {
    return vector.memory_space;
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4ExternalVector<Element>& vector) noexcept {
    const std::size_t bytes =
        vector.size <= 0
            ? 0
            : static_cast<std::size_t>((vector.size - 1) * vector.stride + 1) *
                  sizeof(std::remove_const_t<Element>);
    return ExpressionAliasMetadata(vector.data, vector.data, bytes);
  }
};

template <typename Element>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<::M4ExternalVector<Element>> {
  using value_type = std::remove_const_t<Element>;

  static constexpr std::array<extent_t, 1> Shape(
      const ::M4ExternalVector<Element>& vector) noexcept {
    return {vector.size};
  }

  static constexpr ExpressionAliasMetadata Alias(
      const ::M4ExternalVector<Element>& vector) noexcept {
    return ExpressionPlacementAdapter<::M4ExternalVector<Element>>::Alias(
        vector);
  }

  static constexpr bool IsUnique(
      const ::M4ExternalVector<Element>& vector) noexcept {
    return vector.unique && (vector.size <= 1 || vector.stride != 0);
  }

  static constexpr void Write(::M4ExternalVector<Element>& vector,
                              std::span<const index_t, 1> coordinate,
                              value_type value) noexcept {
    vector.data[static_cast<std::size_t>(coordinate[0] * vector.stride)] =
        value;
  }

  static Status ValidateAccess(const ::M4ExternalVector<Element>& vector,
                               const ExecutionContext& context) {
    return ExpressionAdapter<::M4ExternalVector<Element>>::ValidateAccess(
        vector, context);
  }
};

}  // namespace asc

static_assert(asc::ReadableExpression<M4ExternalVector<const double>>);
static_assert(asc::PlacedReadableExpression<M4ExternalVector<const double>>);
static_assert(!asc::WritableExpression<M4ExternalVector<const double>>);
static_assert(asc::WritableExpression<M4ExternalVector<double>>);

#endif  // ASC_TESTS_SPARSE_EXTERNAL_VECTOR_H_
