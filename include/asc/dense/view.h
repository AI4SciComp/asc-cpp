#ifndef ASC_DENSE_VIEW_H_
#define ASC_DENSE_VIEW_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/contracts.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

namespace asc {

template <typename T>
concept DenseElement =
    std::same_as<T, std::remove_cv_t<T>> && std::is_arithmetic_v<T> &&
    !std::same_as<T, bool> && std::is_trivially_copyable_v<T> &&
    std::is_trivially_destructible_v<T>;

template <typename T>
concept DenseExtents = requires(const T& extents) {
  { T::kRank } -> std::convertible_to<std::size_t>;
  { extents.values() };
  { extents.logical_size() } -> std::same_as<extent_t>;
};

template <DenseElement Element, DenseExtents ExtentsType>
class DenseArray;

template <typename Element, std::size_t Rank>
class DenseView {
 public:
  using element_type = Element;
  using value_type = std::remove_const_t<Element>;
  static constexpr std::size_t kRank = Rank;

  static Result<DenseView> Create(Element* data, DenseLayout<Rank> mapping,
                                  MemorySpace memory_space) {
    if (mapping.required_span_size() != 0 && data == nullptr) {
      return Status(ErrorCode::kInvalidArgument,
                    "A nonempty DenseView requires a non-null pointer");
    }
    if (!mapping.is_unique()) {
      return Status(ErrorCode::kInvalidArgument,
                    "Milestone 3 DenseView requires a unique mapping");
    }
    auto byte_span =
        CheckedMultiply(mapping.required_span_size(), sizeof(value_type));
    if (!byte_span.ok()) {
      return byte_span.status();
    }
    return DenseView(data, mapping, memory_space);
  }

  template <typename OtherElement>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<OtherElement>, value_type> &&
             !std::is_const_v<OtherElement>)
  constexpr DenseView(const DenseView<OtherElement, Rank>& other) noexcept
      : data_(other.data()),
        mapping_(other.mapping()),
        memory_space_(other.memory_space()) {}

  [[nodiscard]] constexpr Element* data() const noexcept { return data_; }

  [[nodiscard]] constexpr const DenseLayout<Rank>& mapping() const noexcept {
    return mapping_;
  }

  [[nodiscard]] constexpr MemorySpace memory_space() const noexcept {
    return memory_space_;
  }

  [[nodiscard]] constexpr std::span<const extent_t, Rank> extents()
      const noexcept {
    return mapping_.extents();
  }

  [[nodiscard]] constexpr std::span<const stride_t, Rank> strides()
      const noexcept {
    return mapping_.strides();
  }

  [[nodiscard]] constexpr extent_t logical_size() const noexcept {
    return mapping_.logical_size();
  }

  [[nodiscard]] Result<Element*> At(
      std::span<const index_t, Rank> coordinates) const {
    if (memory_space_ != MemorySpace::kHost &&
        memory_space_ != MemorySpace::kPinnedHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "DenseView element access requires host-accessible memory");
    }
    auto offset = mapping_.Offset(coordinates);
    if (!offset.ok()) {
      return offset.status();
    }
    return data_ + *offset;
  }

  [[nodiscard]] Result<DenseView> Subview(
      std::span<const index_t, Rank> offsets,
      std::span<const extent_t, Rank> extents) const {
    bool empty = false;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (offsets[dimension] < 0 || extents[dimension] < 0) {
        return Status(ErrorCode::kInvalidArgument,
                      "Dense subview offsets and extents cannot be negative");
      }
      auto end = CheckedAdd<extent_t>(offsets[dimension], extents[dimension]);
      if (!end.ok()) {
        return end.status();
      }
      if (*end > mapping_.extents()[dimension]) {
        return Status(ErrorCode::kIndex,
                      "Dense subview exceeds its source extent");
      }
      empty = empty || extents[dimension] == 0;
    }

    auto submapping = DenseLayout<Rank>::Create(
        extents, LayoutStride<Rank>{.strides = [&] {
          std::array<stride_t, Rank> result{};
          for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
            result[dimension] = mapping_.strides()[dimension];
          }
          return result;
        }()});
    if (!submapping.ok()) {
      return submapping.status();
    }
    if (!submapping->is_unique()) {
      return Status(ErrorCode::kInvalidArgument,
                    "A DenseView subview must remain unique");
    }

    Element* subdata = data_;
    if (!empty) {
      auto offset = mapping_.Offset(offsets);
      if (!offset.ok()) {
        return offset.status();
      }
      subdata += *offset;
    }
    return DenseView(subdata, *submapping, memory_space_);
  }

  [[nodiscard]] constexpr bool SameDescriptor(
      const DenseView& other) const noexcept {
    if (data_ != other.data_ || memory_space_ != other.memory_space_) {
      return false;
    }
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (mapping_.extents()[dimension] !=
              other.mapping_.extents()[dimension] ||
          mapping_.strides()[dimension] !=
              other.mapping_.strides()[dimension]) {
        return false;
      }
    }
    return true;
  }

 private:
  template <DenseElement, DenseExtents>
  friend class DenseArray;

  constexpr DenseView(Element* data, DenseLayout<Rank> mapping,
                      MemorySpace memory_space) noexcept
      : data_(data), mapping_(mapping), memory_space_(memory_space) {}

  Element* data_;
  DenseLayout<Rank> mapping_;
  MemorySpace memory_space_;
};

template <typename Element, std::size_t Rank>
struct ExpressionAdapter<DenseView<Element, Rank>> {
  using value_type = std::remove_const_t<Element>;
  static constexpr rank_t rank = static_cast<rank_t>(Rank);
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation =
      ExpressionOperation::kTerminal;

  static constexpr std::array<extent_t, Rank> Shape(
      const DenseView<Element, Rank>& view) noexcept {
    std::array<extent_t, Rank> shape{};
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      shape[dimension] = view.extents()[dimension];
    }
    return shape;
  }

  static value_type Read(const DenseView<Element, Rank>& view,
                         std::span<const index_t, Rank> coordinates) {
    auto element = view.At(coordinates);
    ASC_CHECK_MESSAGE(element.ok(),
                      "Dense expression read requires valid host coordinates");
    return **element;
  }

  static bool MayAlias(const DenseView<Element, Rank>& view,
                       AliasToken token) noexcept {
    if (view.mapping().required_span_size() == 0 || view.data() == nullptr ||
        token.identity() == nullptr) {
      return false;
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(view.data());
    const std::size_t span_bytes =
        view.mapping().required_span_size() * sizeof(value_type);
    if (span_bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
      return true;
    }
    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(token.identity());
    return address >= begin && address < begin + span_bytes;
  }

  static Status ValidateAccess(const DenseView<Element, Rank>& view,
                               const ExecutionContext& context) {
    if (context.backend() != Backend::kSerial) {
      return Status(ErrorCode::kUnsupported,
                    "DenseView expression access requires serial execution");
    }
    if (view.memory_space() != MemorySpace::kHost ||
        !context.CanAccess(view.memory_space())) {
      return Status(ErrorCode::kMemoryAccess,
                    "DenseView expression access requires host storage");
    }
    return Status::Ok();
  }
};

template <typename Element, std::size_t Rank>
struct ExpressionPlacementAdapter<DenseView<Element, Rank>> {
  static constexpr MemorySpace Space(
      const DenseView<Element, Rank>& view) noexcept {
    return view.memory_space();
  }

  static constexpr ExpressionAliasMetadata Alias(
      const DenseView<Element, Rank>& view) noexcept {
    const std::size_t bytes =
        view.mapping().required_span_size() *
        sizeof(typename DenseView<Element, Rank>::value_type);
    return ExpressionAliasMetadata(view.data(), view.data(), bytes);
  }
};

template <typename Element, std::size_t Rank>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<DenseView<Element, Rank>> {
  using value_type = typename DenseView<Element, Rank>::value_type;

  static constexpr std::array<extent_t, Rank> Shape(
      const DenseView<Element, Rank>& view) noexcept {
    return ExpressionAdapter<DenseView<Element, Rank>>::Shape(view);
  }

  static constexpr ExpressionAliasMetadata Alias(
      const DenseView<Element, Rank>& view) noexcept {
    return ExpressionPlacementAdapter<DenseView<Element, Rank>>::Alias(view);
  }

  static constexpr bool IsUnique(
      const DenseView<Element, Rank>& view) noexcept {
    return view.mapping().is_unique();
  }

  static void Write(DenseView<Element, Rank>& view,
                    std::span<const index_t, Rank> coordinates,
                    value_type value) {
    auto element = view.At(coordinates);
    ASC_CHECK_MESSAGE(element.ok(),
                      "Dense expression write requires valid host coordinates");
    **element = value;
  }

  static Status ValidateAccess(const DenseView<Element, Rank>& view,
                               const ExecutionContext& context) {
    return ExpressionAdapter<DenseView<Element, Rank>>::ValidateAccess(view,
                                                                       context);
  }
};

static_assert(std::is_trivially_copyable_v<DenseView<float, 1>>);

}  // namespace asc

#endif  // ASC_DENSE_VIEW_H_
