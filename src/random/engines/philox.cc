// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <cstdint>
#include <array>

#include "asc/random/counter_engine.h"

namespace asc {

namespace {

using PhiloxCounter = std::array<std::uint32_t, 4>;
using PhiloxKey = std::array<std::uint32_t, 2>;

constexpr std::uint32_t kPhiloxMultiplier0 = 0xD2511F53U;
constexpr std::uint32_t kPhiloxMultiplier1 = 0xCD9E8D57U;
constexpr std::uint32_t kPhiloxKeyBump0 = 0x9E3779B9U;
constexpr std::uint32_t kPhiloxKeyBump1 = 0xBB67AE85U;

PhiloxCounter PhiloxRound(PhiloxCounter counter, PhiloxKey key) noexcept {
  const std::uint64_t product0 =
      static_cast<std::uint64_t>(kPhiloxMultiplier0) * counter[0];
  const std::uint64_t product1 =
      static_cast<std::uint64_t>(kPhiloxMultiplier1) * counter[2];
  const auto low0 = static_cast<std::uint32_t>(product0);
  const auto high0 = static_cast<std::uint32_t>(product0 >> 32U);
  const auto low1 = static_cast<std::uint32_t>(product1);
  const auto high1 = static_cast<std::uint32_t>(product1 >> 32U);
  return {high1 ^ counter[1] ^ key[0], low1,
          high0 ^ counter[3] ^ key[1], low0};
}

PhiloxKey BumpPhiloxKey(PhiloxKey key) noexcept {
  key[0] += kPhiloxKeyBump0;
  key[1] += kPhiloxKeyBump1;
  return key;
}

}  // namespace

Philox4x32_10::ResultType Philox4x32_10::Generate(
    RandomKey key, RandomCounter counter) noexcept {
  PhiloxCounter words{
      static_cast<std::uint32_t>(counter.offset),
      static_cast<std::uint32_t>(counter.offset >> 32U),
      static_cast<std::uint32_t>(counter.subsequence),
      static_cast<std::uint32_t>(counter.subsequence >> 32U)};
  PhiloxKey key_words{static_cast<std::uint32_t>(key.value),
                      static_cast<std::uint32_t>(key.value >> 32U)};
  for (int round = 0; round < 10; ++round) {
    words = PhiloxRound(words, key_words);
    key_words = BumpPhiloxKey(key_words);
  }
  return words;
}

std::uint64_t Philox4x32_10::Generate64(RandomKey key,
                                        RandomCounter counter) noexcept {
  const ResultType words = Generate(key, counter);
  return (static_cast<std::uint64_t>(words[0]) << 32U) |
         static_cast<std::uint64_t>(words[1]);
}

}  // namespace asc
