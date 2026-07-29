#ifndef ASC_DENSE_LAYOUT_H_
#define ASC_DENSE_LAYOUT_H_

#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

struct LayoutLeft {};
struct LayoutRight {};

template <std::size_t Rank>
struct LayoutStride {
  std::array<stride_t, Rank> strides;
};

enum class DenseLayoutKind {
  kLeft,
  kRight,
  kStride,
};

template <std::size_t Rank>
class DenseLayout {
  static_assert(Rank <= std::numeric_limits<rank_t>::max(),
                "Dense rank does not fit rank_t");

 public:
  static Result<DenseLayout> Create(std::span<const extent_t, Rank> extents,
                                    LayoutLeft = {}) {
    auto validated = ValidateExtents(extents);
    if (!validated.ok()) {
      return validated.status();
    }

    std::array<stride_t, Rank> strides{};
    stride_t stride = 1;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      strides[dimension] = stride;
      auto next = CheckedMultiply(stride, extents[dimension]);
      if (!next.ok()) {
        return Status(ErrorCode::kOverflow,
                      "Dense left-layout stride overflow");
      }
      stride = *next;
    }
    return CreateValidated(extents, strides, DenseLayoutKind::kLeft);
  }

  static Result<DenseLayout> Create(std::span<const extent_t, Rank> extents,
                                    LayoutRight) {
    auto validated = ValidateExtents(extents);
    if (!validated.ok()) {
      return validated.status();
    }

    std::array<stride_t, Rank> strides{};
    stride_t stride = 1;
    for (std::size_t reverse = Rank; reverse > 0; --reverse) {
      const std::size_t dimension = reverse - 1;
      strides[dimension] = stride;
      auto next = CheckedMultiply(stride, extents[dimension]);
      if (!next.ok()) {
        return Status(ErrorCode::kOverflow,
                      "Dense right-layout stride overflow");
      }
      stride = *next;
    }
    return CreateValidated(extents, strides, DenseLayoutKind::kRight);
  }

  static Result<DenseLayout> Create(std::span<const extent_t, Rank> extents,
                                    LayoutStride<Rank> layout_stride) {
    auto validated = ValidateExtents(extents);
    if (!validated.ok()) {
      return validated.status();
    }
    for (stride_t stride : layout_stride.strides) {
      if (stride < 0) {
        return Status(ErrorCode::kInvalidArgument,
                      "Dense layout strides cannot be negative");
      }
    }
    return CreateValidated(extents, layout_stride.strides,
                           DenseLayoutKind::kStride);
  }

  [[nodiscard]] constexpr std::span<const extent_t, Rank> extents()
      const noexcept {
    return extents_;
  }

  [[nodiscard]] constexpr std::span<const stride_t, Rank> strides()
      const noexcept {
    return strides_;
  }

  [[nodiscard]] constexpr extent_t logical_size() const noexcept {
    return logical_size_;
  }

  [[nodiscard]] constexpr std::size_t required_span_size() const noexcept {
    return required_span_size_;
  }

  [[nodiscard]] constexpr bool is_unique() const noexcept { return unique_; }

  [[nodiscard]] constexpr bool is_exhaustive() const noexcept {
    return exhaustive_;
  }

  [[nodiscard]] constexpr DenseLayoutKind kind() const noexcept {
    return kind_;
  }

  [[nodiscard]] Result<std::size_t> Offset(
      std::span<const index_t, Rank> coordinates) const {
    stride_t offset = 0;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (coordinates[dimension] < 0 ||
          coordinates[dimension] >= extents_[dimension]) {
        return Status(ErrorCode::kIndex,
                      "Dense coordinate is outside its extent");
      }
      auto term = CheckedMultiply<stride_t>(coordinates[dimension],
                                            strides_[dimension]);
      if (!term.ok()) {
        return term.status();
      }
      auto next = CheckedAdd(offset, *term);
      if (!next.ok()) {
        return next.status();
      }
      offset = *next;
    }
    return CheckedCast<std::size_t>(offset);
  }

 private:
  static Result<extent_t> ValidateExtents(
      std::span<const extent_t, Rank> extents) {
    bool has_zero_extent = false;
    extent_t logical_size = 1;
    for (extent_t extent : extents) {
      if (extent < 0) {
        return Status(ErrorCode::kShape,
                      "Dense layout extents cannot be negative");
      }
      has_zero_extent = has_zero_extent || extent == 0;
      if (!has_zero_extent) {
        auto next = CheckedMultiply(logical_size, extent);
        if (!next.ok()) {
          return Status(ErrorCode::kOverflow, "Dense logical size overflow");
        }
        logical_size = *next;
      }
    }
    return has_zero_extent ? extent_t{0} : logical_size;
  }

  static Result<DenseLayout> CreateValidated(
      std::span<const extent_t, Rank> extents,
      const std::array<stride_t, Rank>& strides, DenseLayoutKind kind) {
    auto logical_size_result = ValidateExtents(extents);
    if (!logical_size_result.ok()) {
      return logical_size_result.status();
    }
    const extent_t logical_size = *logical_size_result;

    stride_t required_span = 1;
    if (logical_size == 0) {
      required_span = 0;
    } else {
      stride_t maximum_offset = 0;
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        auto term = CheckedMultiply<stride_t>(extents[dimension] - 1,
                                              strides[dimension]);
        if (!term.ok()) {
          return Status(ErrorCode::kOverflow,
                        "Dense layout span term overflow");
        }
        auto next = CheckedAdd(maximum_offset, *term);
        if (!next.ok()) {
          return Status(ErrorCode::kOverflow,
                        "Dense layout maximum offset overflow");
        }
        maximum_offset = *next;
      }
      auto span = CheckedAdd(maximum_offset, stride_t{1});
      if (!span.ok()) {
        return Status(ErrorCode::kOverflow, "Dense layout span overflow");
      }
      required_span = *span;
    }

    auto converted_span = CheckedCast<std::size_t>(required_span);
    if (!converted_span.ok()) {
      return converted_span.status();
    }

    const bool unique = logical_size == 0 || ProveUnique(extents, strides);
    auto converted_logical_size = CheckedCast<std::size_t>(logical_size);
    if (!converted_logical_size.ok()) {
      return converted_logical_size.status();
    }
    const bool exhaustive =
        unique && *converted_span == *converted_logical_size;

    std::array<extent_t, Rank> extent_array{};
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      extent_array[dimension] = extents[dimension];
    }
    return DenseLayout(extent_array, strides, logical_size, *converted_span,
                       unique, exhaustive, kind);
  }

  static bool ProveUnique(std::span<const extent_t, Rank> extents,
                          const std::array<stride_t, Rank>& strides) {
    std::array<std::size_t, Rank> dimensions{};
    std::size_t active_count = 0;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      if (extents[dimension] > 1) {
        dimensions[active_count++] = dimension;
      }
    }
    for (std::size_t index = 1; index < active_count; ++index) {
      const std::size_t value = dimensions[index];
      std::size_t insertion = index;
      while (insertion > 0 &&
             strides[value] < strides[dimensions[insertion - 1]]) {
        dimensions[insertion] = dimensions[insertion - 1];
        --insertion;
      }
      dimensions[insertion] = value;
    }

    stride_t covered_span = 1;
    for (std::size_t index = 0; index < active_count; ++index) {
      const std::size_t dimension = dimensions[index];
      if (strides[dimension] < covered_span) {
        return false;
      }
      auto contribution =
          CheckedMultiply<stride_t>(extents[dimension] - 1, strides[dimension]);
      if (!contribution.ok()) {
        return false;
      }
      auto next = CheckedAdd(covered_span, *contribution);
      if (!next.ok()) {
        return false;
      }
      covered_span = *next;
    }
    return true;
  }

  constexpr DenseLayout(std::array<extent_t, Rank> extents,
                        std::array<stride_t, Rank> strides,
                        extent_t logical_size, std::size_t required_span_size,
                        bool unique, bool exhaustive, DenseLayoutKind kind)
      : extents_(extents),
        strides_(strides),
        logical_size_(logical_size),
        required_span_size_(required_span_size),
        unique_(unique),
        exhaustive_(exhaustive),
        kind_(kind) {}

  std::array<extent_t, Rank> extents_{};
  std::array<stride_t, Rank> strides_{};
  extent_t logical_size_ = 0;
  std::size_t required_span_size_ = 0;
  bool unique_ = false;
  bool exhaustive_ = false;
  DenseLayoutKind kind_ = DenseLayoutKind::kStride;
};

}  // namespace asc

#endif  // ASC_DENSE_LAYOUT_H_
