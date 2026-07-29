#ifndef ASC_TESTS_RANDOM_CUDA_PHILOX_ORACLE_H_
#define ASC_TESTS_RANDOM_CUDA_PHILOX_ORACLE_H_

#include <cstddef>
#include <cstdint>

#include "asc/random/engine.h"

namespace asc_random_cuda_test {

struct PhiloxWords {
  std::uint32_t words[4];
};

inline PhiloxWords PhiloxBlockOracle(asc::RandomStream stream,
                                     asc::RandomSubsequence subsequence,
                                     std::uint64_t block) noexcept {
  constexpr std::uint32_t kMultiplier0 = UINT32_C(0xD2511F53);
  constexpr std::uint32_t kMultiplier1 = UINT32_C(0xCD9E8D57);
  constexpr std::uint32_t kWeyl0 = UINT32_C(0x9E3779B9);
  constexpr std::uint32_t kWeyl1 = UINT32_C(0xBB67AE85);

  std::uint32_t counter[4] = {static_cast<std::uint32_t>(block),
                              static_cast<std::uint32_t>(block >> 32U),
                              static_cast<std::uint32_t>(subsequence),
                              static_cast<std::uint32_t>(subsequence >> 32U)};
  std::uint32_t key[2] = {static_cast<std::uint32_t>(stream),
                          static_cast<std::uint32_t>(stream >> 32U)};
  for (int round = 0; round < 10; ++round) {
    const std::uint64_t product0 =
        static_cast<std::uint64_t>(kMultiplier0) * counter[0];
    const std::uint64_t product1 =
        static_cast<std::uint64_t>(kMultiplier1) * counter[2];
    const std::uint32_t next[4] = {
        static_cast<std::uint32_t>(product1 >> 32U) ^ counter[1] ^ key[0],
        static_cast<std::uint32_t>(product1),
        static_cast<std::uint32_t>(product0 >> 32U) ^ counter[3] ^ key[1],
        static_cast<std::uint32_t>(product0)};
    for (int lane = 0; lane < 4; ++lane) {
      counter[lane] = next[lane];
    }
    if (round != 9) {
      key[0] += kWeyl0;
      key[1] += kWeyl1;
    }
  }
  return PhiloxWords{{counter[0], counter[1], counter[2], counter[3]}};
}

inline std::uint32_t PhiloxWordOracle(asc::RandomStream stream,
                                      asc::RandomSubsequence subsequence,
                                      asc::RandomOffset offset) noexcept {
  const PhiloxWords block =
      PhiloxBlockOracle(stream, subsequence, offset / UINT64_C(4));
  return block.words[static_cast<std::size_t>(offset % UINT64_C(4))];
}

inline float Uniform01FloatOracle(std::uint32_t word) noexcept {
  return static_cast<float>(word >> 8U) * 0x1.0p-24F;
}

inline double Uniform01DoubleOracle(std::uint32_t high,
                                    std::uint32_t low) noexcept {
  const std::uint64_t combined = (static_cast<std::uint64_t>(high) << 32U) |
                                 static_cast<std::uint64_t>(low);
  return static_cast<double>(combined >> 11U) * 0x1.0p-53;
}

}  // namespace asc_random_cuda_test

#endif  // ASC_TESTS_RANDOM_CUDA_PHILOX_ORACLE_H_
