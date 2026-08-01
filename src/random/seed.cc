#include "asc/random/seed.h"

#include <cstdint>
#include <limits>
#include <random>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

Result<std::uint64_t> AcquireNondeterministicSeed(std::random_device& source) {
  if (source.min() != 0 ||
      source.max() != std::numeric_limits<std::uint32_t>::max()) {
    return Status(ErrorCode::kUnsupported,
                  "Random device must expose the full uint32 range");
  }

  std::uint32_t high = 0;
  std::uint32_t low = 0;
  try {
    high = static_cast<std::uint32_t>(source());
    low = static_cast<std::uint32_t>(source());
  } catch (...) {
    return Status(ErrorCode::kUnavailable,
                  "Random device seed acquisition failed");
  }
  return (static_cast<std::uint64_t>(high) << 32U) |
         static_cast<std::uint64_t>(low);
}

}  // namespace asc
