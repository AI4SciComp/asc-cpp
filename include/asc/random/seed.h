#ifndef ASC_RANDOM_SEED_H_
#define ASC_RANDOM_SEED_H_

#include <cstdint>
#include <random>

#include "asc/core/result.h"
#include "asc/random/export.h"

namespace asc {

[[nodiscard]] ASC_RANDOM_EXPORT Result<std::uint64_t>
AcquireNondeterministicSeed(std::random_device& source);

}  // namespace asc

#endif  // ASC_RANDOM_SEED_H_
