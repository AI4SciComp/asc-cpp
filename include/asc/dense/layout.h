#ifndef ASC_DENSE_LAYOUT_H_
#define ASC_DENSE_LAYOUT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

struct LayoutLeft {};
struct LayoutRight {};
struct LayoutStride {};

enum class DenseLayoutKind : std::uint8_t {
  kLeft = 0,
  kRight = 1,
  kStride = 2,
};

// A validated compile-time-rank affine layout mapping. Dimension zero is the
// fastest logical traversal dimension regardless of the physical layout.
template <std::size_t Rank>
class DenseLayoutMapping {
 public:
  using ShapeType = std::array<extent_t, Rank>;
  using StrideType = std::array<stride_t, Rank>;

  static Result<DenseLayoutMapping> Create(
      LayoutLeft, std::span<const extent_t, Rank> shape) {
    const Status shape_status = ValidateShape(shape);
    if (!shape_status.ok()) {
      return shape_status;
    }
    StrideType strides{};
    stride_t next_stride = 1;
    bool propagation_stopped = false;
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        if (propagation_stopped) {
          strides[dimension] = 0;
          continue;
        }
        strides[dimension] = next_stride;
        if (shape[dimension] == 0) {
          propagation_stopped = true;
          continue;
        }
        auto product = CheckedMultiply(next_stride, shape[dimension]);
        if (!product.ok()) {
          if (HasZeroExtent(shape.subspan(dimension + 1))) {
            propagation_stopped = true;
            continue;
          }
          return Status(ErrorCode::kOverflow,
                        "A LayoutLeft stride exceeds stride_t");
        }
        next_stride = *product;
      }
    }
    return CreateValidated(shape, strides, DenseLayoutKind::kLeft);
  }

  static Result<DenseLayoutMapping> Create(
      LayoutRight, std::span<const extent_t, Rank> shape) {
    const Status shape_status = ValidateShape(shape);
    if (!shape_status.ok()) {
      return shape_status;
    }
    StrideType strides{};
    stride_t next_stride = 1;
    bool propagation_stopped = false;
    for (std::size_t reverse = Rank; reverse > 0; --reverse) {
      const std::size_t dimension = reverse - 1;
      if (propagation_stopped) {
        strides[dimension] = 0;
        continue;
      }
      strides[dimension] = next_stride;
      if (shape[dimension] == 0) {
        propagation_stopped = true;
        continue;
      }
      auto product = CheckedMultiply(next_stride, shape[dimension]);
      if (!product.ok()) {
        if (HasZeroExtent(shape.first(dimension))) {
          propagation_stopped = true;
          continue;
        }
        return Status(ErrorCode::kOverflow,
                      "A LayoutRight stride exceeds stride_t");
      }
      next_stride = *product;
    }
    return CreateValidated(shape, strides, DenseLayoutKind::kRight);
  }

  static Result<DenseLayoutMapping> Create(
      LayoutStride, std::span<const extent_t, Rank> shape,
      std::span<const stride_t, Rank> strides) {
    StrideType stride_values{};
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        stride_values[dimension] = strides[dimension];
      }
    }
    return CreateValidated(shape, stride_values, DenseLayoutKind::kStride);
  }

  [[nodiscard]] static constexpr rank_t rank() noexcept {
    static_assert(Rank <= std::numeric_limits<rank_t>::max());
    return static_cast<rank_t>(Rank);
  }

  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return shape_;
  }
  [[nodiscard]] constexpr const StrideType& strides() const noexcept {
    return strides_;
  }
  [[nodiscard]] constexpr extent_t logical_size() const noexcept {
    return logical_size_;
  }
  [[nodiscard]] constexpr extent_t required_span_size() const noexcept {
    return required_span_size_;
  }
  [[nodiscard]] constexpr bool is_unique() const noexcept { return is_unique_; }
  [[nodiscard]] constexpr bool is_exhaustive() const noexcept {
    return is_exhaustive_;
  }
  [[nodiscard]] constexpr DenseLayoutKind kind() const noexcept {
    return kind_;
  }

  [[nodiscard]] Result<extent_t> Offset(
      std::span<const index_t, Rank> indices) const {
    extent_t offset = 0;
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        if (indices[dimension] < 0 || indices[dimension] >= shape_[dimension]) {
          return Status(ErrorCode::kIndex,
                        "A dense coordinate is outside its extent");
        }
        auto term = CheckedMultiply(indices[dimension], strides_[dimension]);
        if (!term.ok()) {
          return Status(ErrorCode::kOverflow,
                        "A dense coordinate offset exceeds extent_t");
        }
        auto sum = CheckedAdd(offset, *term);
        if (!sum.ok()) {
          return Status(ErrorCode::kOverflow,
                        "A dense coordinate offset exceeds extent_t");
        }
        offset = *sum;
      }
    }
    return offset;
  }

  friend constexpr bool operator==(const DenseLayoutMapping&,
                                   const DenseLayoutMapping&) = default;

 private:
  struct ActiveDimension {
    stride_t stride;
    extent_t extent;
  };

  static Status ValidateShape(std::span<const extent_t, Rank> shape) {
    for (extent_t extent : shape) {
      if (extent < 0) {
        return Status(ErrorCode::kShape, "A dense extent cannot be negative");
      }
    }
    return Status::Ok();
  }

  static bool HasZeroExtent(std::span<const extent_t> shape) noexcept {
    for (extent_t extent : shape) {
      if (extent == 0) {
        return true;
      }
    }
    return false;
  }

  static Result<DenseLayoutMapping> CreateValidated(
      std::span<const extent_t, Rank> shape, const StrideType& strides,
      DenseLayoutKind kind) {
    ShapeType shape_values{};
    bool has_zero_extent = false;
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        if (shape[dimension] < 0) {
          return Status(ErrorCode::kShape, "A dense extent cannot be negative");
        }
        if (strides[dimension] < 0) {
          return Status(ErrorCode::kInvalidArgument,
                        "A dense stride cannot be negative");
        }
        shape_values[dimension] = shape[dimension];
        has_zero_extent = has_zero_extent || shape[dimension] == 0;
      }
    }

    extent_t logical_size = has_zero_extent ? 0 : 1;
    if (!has_zero_extent) {
      for (extent_t extent : shape_values) {
        auto product = CheckedMultiply(logical_size, extent);
        if (!product.ok()) {
          return Status(ErrorCode::kOverflow,
                        "The dense logical size exceeds extent_t");
        }
        logical_size = *product;
      }
    }

    extent_t required_span_size = has_zero_extent ? 0 : 1;
    if (!has_zero_extent) {
      extent_t maximum_offset = 0;
      if constexpr (Rank > 0) {
        for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
          auto term =
              CheckedMultiply(shape_values[dimension] - 1, strides[dimension]);
          if (!term.ok()) {
            return Status(ErrorCode::kOverflow,
                          "The dense mapping span exceeds extent_t");
          }
          auto sum = CheckedAdd(maximum_offset, *term);
          if (!sum.ok()) {
            return Status(ErrorCode::kOverflow,
                          "The dense mapping span exceeds extent_t");
          }
          maximum_offset = *sum;
        }
      }
      auto span = CheckedAdd(maximum_offset, static_cast<extent_t>(1));
      if (!span.ok()) {
        return Status(ErrorCode::kOverflow,
                      "The dense mapping span exceeds extent_t");
      }
      required_span_size = *span;
    }

    std::array<ActiveDimension, Rank> active{};
    std::size_t active_count = 0;
    if constexpr (Rank > 0) {
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        if (shape_values[dimension] > 1) {
          active[active_count++] =
              ActiveDimension{strides[dimension], shape_values[dimension]};
        }
      }
    }
    for (std::size_t index = 1; index < active_count; ++index) {
      const ActiveDimension value = active[index];
      std::size_t position = index;
      while (position > 0 && active[position - 1].stride > value.stride) {
        active[position] = active[position - 1];
        --position;
      }
      active[position] = value;
    }

    bool is_unique = true;
    if (!has_zero_extent) {
      extent_t covered_span = 1;
      for (std::size_t index = 0; index < active_count; ++index) {
        if (active[index].stride < covered_span) {
          is_unique = false;
          break;
        }
        auto additional =
            CheckedMultiply(active[index].extent - 1, active[index].stride);
        if (!additional.ok()) {
          return Status(ErrorCode::kOverflow,
                        "The dense uniqueness proof exceeds extent_t");
        }
        auto span = CheckedAdd(covered_span, *additional);
        if (!span.ok()) {
          return Status(ErrorCode::kOverflow,
                        "The dense uniqueness proof exceeds extent_t");
        }
        covered_span = *span;
      }
    }
    const bool is_exhaustive = is_unique && required_span_size == logical_size;

    return DenseLayoutMapping(shape_values, strides, logical_size,
                              required_span_size, is_unique, is_exhaustive,
                              kind);
  }

  constexpr DenseLayoutMapping(ShapeType shape, StrideType strides,
                               extent_t logical_size,
                               extent_t required_span_size, bool is_unique,
                               bool is_exhaustive,
                               DenseLayoutKind kind) noexcept
      : shape_(shape),
        strides_(strides),
        logical_size_(logical_size),
        required_span_size_(required_span_size),
        is_unique_(is_unique),
        is_exhaustive_(is_exhaustive),
        kind_(kind) {}

  ShapeType shape_;
  StrideType strides_;
  extent_t logical_size_;
  extent_t required_span_size_;
  bool is_unique_;
  bool is_exhaustive_;
  DenseLayoutKind kind_;
};

}  // namespace asc

#endif  // ASC_DENSE_LAYOUT_H_
