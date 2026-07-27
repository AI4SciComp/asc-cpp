#ifndef ASC_TESTS_RANDOM_REFERENCE_PHILOX_H_
#define ASC_TESTS_RANDOM_REFERENCE_PHILOX_H_

#include <array>
#include <cstdint>

namespace asc_random_test {

using ReferenceCounter = std::array<std::uint32_t, 4>;
using ReferenceKey = std::array<std::uint32_t, 2>;
using ReferenceBlock = std::array<std::uint32_t, 4>;

// Independent transcription of the frozen SC11 generalized-Feistel mapping.
// It deliberately shares no production helper or implementation structure.
inline ReferenceBlock ReferencePhilox4x32_10(ReferenceCounter counter,
                                             ReferenceKey key) {
  constexpr std::uint64_t kFirstMultiplier = 0xD2511F53ULL;
  constexpr std::uint64_t kSecondMultiplier = 0xCD9E8D57ULL;
  constexpr std::uint32_t kFirstWeyl = 0x9E3779B9U;
  constexpr std::uint32_t kSecondWeyl = 0xBB67AE85U;

  for (int round = 0; round < 10; ++round) {
    const std::uint64_t first_product =
        kFirstMultiplier * static_cast<std::uint64_t>(counter[0]);
    const std::uint64_t second_product =
        kSecondMultiplier * static_cast<std::uint64_t>(counter[2]);
    const std::uint32_t first_high =
        static_cast<std::uint32_t>(first_product >> 32U);
    const std::uint32_t second_high =
        static_cast<std::uint32_t>(second_product >> 32U);

    counter = {
        second_high ^ counter[1] ^ key[0],
        static_cast<std::uint32_t>(second_product),
        first_high ^ counter[3] ^ key[1],
        static_cast<std::uint32_t>(first_product),
    };

    if (round != 9) {
      key[0] = static_cast<std::uint32_t>(key[0] + kFirstWeyl);
      key[1] = static_cast<std::uint32_t>(key[1] + kSecondWeyl);
    }
  }
  return counter;
}

}  // namespace asc_random_test

#endif  // ASC_TESTS_RANDOM_REFERENCE_PHILOX_H_
