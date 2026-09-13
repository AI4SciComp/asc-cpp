#ifndef ASC_DISPLAY_READ_OBSERVATION_H_
#define ASC_DISPLAY_READ_OBSERVATION_H_

#include <cstddef>
#include <cstdint>
#include <span>

namespace asc_display_observation {

// One counter per live backing byte distinguishes omitted, repeated and
// selected loads, including separate real and imaginary complex components.
struct Region {
  std::span<const std::byte> backing;
  std::span<std::uint32_t> reads;
};

void Start(std::span<Region> regions);
void Stop();

}  // namespace asc_display_observation

#endif  // ASC_DISPLAY_READ_OBSERVATION_H_
