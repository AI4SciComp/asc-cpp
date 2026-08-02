#include "asc/random/quasi.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {
namespace {

bool IsPrime(std::uint32_t candidate) noexcept {
  if (candidate < 2) {
    return false;
  }
  if ((candidate & 1U) == 0U) {
    return candidate == 2;
  }
  for (std::uint32_t divisor = 3; divisor <= candidate / divisor;
       divisor += 2) {
    if (candidate % divisor == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

Result<std::uint32_t> PrimeAt(std::size_t index) {
  if (index >= kSobolDimensionCount) {
    return Status(ErrorCode::kIndex,
                  "Prime index exceeds the supported QMC dimension domain");
  }
  std::size_t found = 0;
  for (std::uint32_t candidate = 2;; ++candidate) {
    if (IsPrime(candidate)) {
      if (found == index) {
        return candidate;
      }
      ++found;
    }
  }
}

Result<std::uint64_t> SobolWord(std::uint64_t index, std::size_t dimension) {
  if (dimension >= kSobolDimensionCount) {
    return Status(ErrorCode::kIndex, "Sobol dimension is out of range");
  }
  std::array<std::uint64_t, kSobolDirectionWordCount> directions{};
  const Status status = InitializeSobolDirectionNumbers(dimension, directions);
  if (!status.ok()) {
    return status;
  }
  std::uint64_t gray = index ^ (index >> 1U);
  std::uint64_t word = 0;
  while (gray != 0) {
    const auto bit = static_cast<std::size_t>(std::countr_zero(gray));
    word ^= directions[bit];
    gray &= gray - 1;
  }
  return word;
}

Result<SobolSequence> SobolSequence::Create(std::size_t dimension_count,
                                            std::uint64_t initial_index) {
  if (dimension_count > kSobolDimensionCount) {
    return Status(ErrorCode::kShape,
                  "Sobol sequence dimension exceeds the Joe-Kuo table");
  }
  return SobolSequence(dimension_count, initial_index);
}

Status SobolSequence::Reset(std::uint64_t index) {
  index_ = index;
  exhausted_ = false;
  return Status::Ok();
}

Status SobolSequence::Skip(std::uint64_t count) {
  if (exhausted_) {
    return Status(ErrorCode::kEndOfFile,
                  "Sobol sequence exhausted the uint64 index domain");
  }
  if (count > std::numeric_limits<std::uint64_t>::max() - index_) {
    return Status(ErrorCode::kOverflow, "Sobol skip overflows its index");
  }
  index_ += count;
  return Status::Ok();
}

Status SobolSequence::SkipTo(std::uint64_t index) {
  index_ = index;
  exhausted_ = false;
  return Status::Ok();
}

}  // namespace asc
