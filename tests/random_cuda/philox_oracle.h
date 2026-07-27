#ifndef ASC_TESTS_RANDOM_CUDA_PHILOX_ORACLE_H_
#define ASC_TESTS_RANDOM_CUDA_PHILOX_ORACLE_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace asc_random_cuda_test {

struct PhiloxCounter {
  std::array<std::uint32_t, 4> lane;
};

struct PhiloxKey {
  std::array<std::uint32_t, 2> lane;
};

[[nodiscard]] inline std::array<std::uint32_t, 4> Philox4x32_10Oracle(
    PhiloxCounter counter, PhiloxKey key) noexcept {
  constexpr std::uint64_t kMultiplier0 = UINT64_C(0xD2511F53);
  constexpr std::uint64_t kMultiplier1 = UINT64_C(0xCD9E8D57);
  constexpr std::uint32_t kWeyl0 = UINT32_C(0x9E3779B9);
  constexpr std::uint32_t kWeyl1 = UINT32_C(0xBB67AE85);

  for (int round = 0; round < 10; ++round) {
    const std::uint64_t product0 =
        kMultiplier0 * static_cast<std::uint64_t>(counter.lane[0]);
    const std::uint64_t product1 =
        kMultiplier1 * static_cast<std::uint64_t>(counter.lane[2]);
    counter.lane = {
        static_cast<std::uint32_t>(product1 >> 32U) ^ counter.lane[1] ^
            key.lane[0],
        static_cast<std::uint32_t>(product1),
        static_cast<std::uint32_t>(product0 >> 32U) ^ counter.lane[3] ^
            key.lane[1],
        static_cast<std::uint32_t>(product0),
    };
    key.lane[0] += kWeyl0;
    key.lane[1] += kWeyl1;
  }
  return counter.lane;
}

[[nodiscard]] inline std::array<std::uint32_t, 4> PhiloxBlockOracle(
    std::uint64_t stream, std::uint64_t subsequence,
    std::uint64_t block) noexcept {
  const PhiloxCounter counter{{
      static_cast<std::uint32_t>(block),
      static_cast<std::uint32_t>(block >> 32U),
      static_cast<std::uint32_t>(subsequence),
      static_cast<std::uint32_t>(subsequence >> 32U),
  }};
  const PhiloxKey key{{
      static_cast<std::uint32_t>(stream),
      static_cast<std::uint32_t>(stream >> 32U),
  }};
  return Philox4x32_10Oracle(counter, key);
}

[[nodiscard]] inline std::uint32_t PhiloxWordOracle(
    std::uint64_t stream, std::uint64_t subsequence,
    std::uint64_t offset) noexcept {
  const auto block = PhiloxBlockOracle(stream, subsequence, offset / 4U);
  return block[static_cast<std::size_t>(offset % 4U)];
}

[[nodiscard]] inline float Uniform01FloatOracle(std::uint32_t word) noexcept {
  constexpr float kScale = 0x1p-24F;
  return static_cast<float>(word >> 8U) * kScale;
}

[[nodiscard]] inline double Uniform01DoubleOracle(
    std::uint32_t high_word, std::uint32_t low_word) noexcept {
  constexpr double kScale = 0x1p-53;
  const std::uint64_t bits = (static_cast<std::uint64_t>(high_word) << 32U) |
                             static_cast<std::uint64_t>(low_word);
  return static_cast<double>(bits >> 11U) * kScale;
}

}  // namespace asc_random_cuda_test

#endif  // ASC_TESTS_RANDOM_CUDA_PHILOX_ORACLE_H_
