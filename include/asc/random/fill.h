// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_RANDOM_FILL_H_
#define ASC_RANDOM_FILL_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <array>
#include <type_traits>

#include "asc/core/execution_context.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/counter_engine.h"
#include "asc/random/detail/fill_validation.h"
#include "asc/random/distribution.h"
#include "asc/random/types.h"

namespace asc {

/// @brief Fill a canonical writable tensor from explicit Philox positions.
///
/// Logical coordinates are enumerated lexicographically with the rightmost
/// axis changing fastest. Physical layout and padding do not change values.
template <detail::WritableRandomTensor Destination,
          detail::SupportedRandomScalar T>
  requires std::same_as<
      typename std::remove_cvref_t<Destination>::ValueType, T>
Status FillRandom(const ExecutionContext& context,
                  const Destination& destination,
                  Uniform01<T> distribution, RandomKey key,
                  RandomCounter counter) {
  Status status = detail::ValidateRandomContext(context);
  if (!status.ok()) {
    return status;
  }

  detail::RandomFillMetadata metadata;
  status = detail::ValidateRandomMetadata(destination, &metadata);
  if (!status.ok()) {
    return status;
  }
  if (metadata.logical_size == 0) {
    return Status::Ok();
  }
  status = detail::ValidateRandomCounter(counter, metadata.logical_size);
  if (!status.ok()) {
    return status;
  }
  status = detail::ValidateRandomData(destination);
  if (!status.ok()) {
    return status;
  }

  constexpr std::size_t kRank = std::remove_cvref_t<Destination>::Rank();
  std::array<extent_t, kRank> coordinates{};
  auto* data = destination.Data();
  for (extent_t logical_index = 0;
       logical_index < metadata.logical_size; ++logical_index) {
    extent_t storage_offset = 0;
    for (std::size_t dimension = 0; dimension < kRank; ++dimension) {
      storage_offset +=
          coordinates[dimension] * destination.GetStride(dimension);
    }
    const RandomCounter sample_counter{
        counter.subsequence,
        counter.offset + static_cast<std::uint64_t>(logical_index)};
    data[storage_offset] =
        distribution(Philox4x32_10::Generate64(key, sample_counter));

    if constexpr (kRank > 0) {
      for (std::size_t reverse_dimension = kRank;
           reverse_dimension > 0; --reverse_dimension) {
        const std::size_t dimension = reverse_dimension - 1;
        ++coordinates[dimension];
        if (coordinates[dimension] < destination.GetExtent(dimension)) {
          break;
        }
        coordinates[dimension] = 0;
      }
    }
  }
  return Status::Ok();
}

}  // namespace asc

#endif  // ASC_RANDOM_FILL_H_
