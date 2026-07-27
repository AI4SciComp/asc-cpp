#ifndef ASC_CORE_EXTENTS_H_
#define ASC_CORE_EXTENTS_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

template <extent_t... StaticExtents>
class Extents {
 public:
  static constexpr std::size_t kRank = sizeof...(StaticExtents);
  static constexpr std::size_t kDynamicRank =
      ((StaticExtents == kDynamicExtent ? 1U : 0U) + ... + 0U);

  static_assert(((StaticExtents == kDynamicExtent || StaticExtents >= 0) &&
                 ...),
                "A static extent must be non-negative or kDynamicExtent");
  static_assert(kRank <= std::numeric_limits<rank_t>::max(),
                "Extents rank does not fit rank_t");

  static Result<Extents> Create(std::span<const extent_t> dynamic_extents) {
    if (dynamic_extents.size() != kDynamicRank) {
      return Status(ErrorCode::kShape,
                    "Dynamic extent count does not match the extents type");
    }

    constexpr std::array<extent_t, kRank> kStaticExtents = {StaticExtents...};
    std::array<extent_t, kRank> values{};
    std::size_t dynamic_index = 0;
    bool has_zero_extent = false;

    for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
      extent_t value = kStaticExtents[dimension];
      if (value == kDynamicExtent) {
        value = dynamic_extents[dynamic_index++];
      }
      if (value < 0) {
        return Status(ErrorCode::kShape, "An extent cannot be negative");
      }
      has_zero_extent = has_zero_extent || value == 0;
      values[dimension] = value;
    }

    extent_t logical_size = has_zero_extent ? 0 : 1;
    if (!has_zero_extent) {
      for (extent_t value : values) {
        auto product = CheckedMultiply(logical_size, value);
        if (!product.ok()) {
          return Status(ErrorCode::kOverflow,
                        "The logical extent product exceeds extent_t");
        }
        logical_size = *product;
      }
    }

    return Extents(values, logical_size);
  }

  template <CheckedInteger... DynamicValues>
    requires(sizeof...(DynamicValues) == kDynamicRank &&
             (sizeof...(DynamicValues) > 0))
  static Result<Extents> Create(DynamicValues... dynamic_extents) {
    std::array<extent_t, kDynamicRank> values{};
    std::size_t index = 0;
    Status conversion_status = Status::Ok();
    const auto convert = [&](auto value) {
      if (!conversion_status.ok()) {
        return;
      }
      auto converted = CheckedCast<extent_t>(value);
      if (!converted.ok()) {
        conversion_status = converted.status();
        return;
      }
      values[index++] = *converted;
    };
    (convert(dynamic_extents), ...);
    if (!conversion_status.ok()) {
      return conversion_status;
    }
    return Create(std::span<const extent_t>(values));
  }

  static Result<Extents> Create()
    requires(kDynamicRank == 0)
  {
    return Create(std::span<const extent_t>());
  }

  [[nodiscard]] static constexpr rank_t rank() noexcept {
    return static_cast<rank_t>(kRank);
  }

  [[nodiscard]] static constexpr rank_t dynamic_rank() noexcept {
    return static_cast<rank_t>(kDynamicRank);
  }

  template <std::size_t Dimension>
  [[nodiscard]] static constexpr extent_t static_extent() noexcept {
    static_assert(Dimension < kRank, "Extent dimension is out of range");
    constexpr std::array<extent_t, kRank> kStaticExtents = {StaticExtents...};
    return kStaticExtents[Dimension];
  }

  [[nodiscard]] Result<extent_t> extent(rank_t dimension) const {
    if (dimension >= kRank) {
      return Status(ErrorCode::kIndex, "Extent dimension is out of range");
    }
    return values_[dimension];
  }

  [[nodiscard]] std::span<const extent_t, kRank> values() const noexcept {
    return values_;
  }

  [[nodiscard]] extent_t logical_size() const noexcept { return logical_size_; }

 private:
  explicit Extents(std::array<extent_t, kRank> values, extent_t logical_size)
      : values_(values), logical_size_(logical_size) {}

  std::array<extent_t, kRank> values_{};
  extent_t logical_size_ = 0;
};

}  // namespace asc

#endif  // ASC_CORE_EXTENTS_H_
