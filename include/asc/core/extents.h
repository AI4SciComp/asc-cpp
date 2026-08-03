#ifndef ASC_CORE_EXTENTS_H_
#define ASC_CORE_EXTENTS_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

/**
 * @brief Stores validated rank-fixed static and dynamic extents.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
template <extent_t... StaticExtents>
class Extents {
 public:
  /**
   * @brief Stores the Rank value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  static constexpr std::size_t kRank = sizeof...(StaticExtents);
  /**
   * @brief Stores the DynamicRank value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  static constexpr std::size_t kDynamicRank =
      ((StaticExtents == kDynamicExtent ? 1U : 0U) + ... + 0U);

  static_assert(((StaticExtents == kDynamicExtent || StaticExtents >= 0) &&
                 ...),
                "A static extent must be non-negative or kDynamicExtent");
  static_assert(kRank <= std::numeric_limits<rank_t>::max(),
                "Extents rank does not fit rank_t");

  /**
   * @brief Validates inputs and creates the requested Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] dynamic_extents The dynamic extents value required by this
   * contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
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

  /**
   * @brief Validates inputs and creates the requested Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @tparam DynamicValues Type or non-type argument satisfying the
   * declaration's constraints.
   * @param[in] dynamic_extents The dynamic extents value required by this
   * contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
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

  /**
   * @brief Validates inputs and creates the requested Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static Result<Extents> Create()
    requires(kDynamicRank == 0)
  {
    return Create(std::span<const extent_t>());
  }

  /**
   * @brief Returns the object's rank contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] static constexpr rank_t rank() noexcept {
    return static_cast<rank_t>(kRank);
  }

  /**
   * @brief Performs the public dynamic_rank operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] static constexpr rank_t dynamic_rank() noexcept {
    return static_cast<rank_t>(kDynamicRank);
  }

  /**
   * @brief Performs the public static_extent operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @tparam Dimension Type or non-type argument satisfying the declaration's
   * constraints.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  template <std::size_t Dimension>
  [[nodiscard]] static constexpr extent_t static_extent() noexcept {
    static_assert(Dimension < kRank, "Extent dimension is out of range");
    constexpr std::array<extent_t, kRank> kStaticExtents = {StaticExtents...};
    return kStaticExtents[Dimension];
  }

  /**
   * @brief Performs the public extent operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] dimension The dimension value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] Result<extent_t> extent(rank_t dimension) const {
    if (dimension >= kRank) {
      return Status(ErrorCode::kIndex, "Extent dimension is out of range");
    }
    return values_[dimension];
  }

  /**
   * @brief Returns the object's values contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] std::span<const extent_t, kRank> values() const noexcept {
    return values_;
  }

  /**
   * @brief Performs the public logical_size operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] extent_t logical_size() const noexcept { return logical_size_; }

 private:
  explicit Extents(std::array<extent_t, kRank> values, extent_t logical_size)
      : values_(values), logical_size_(logical_size) {}

  std::array<extent_t, kRank> values_{};
  extent_t logical_size_ = 0;
};

}  // namespace asc

#endif  // ASC_CORE_EXTENTS_H_
