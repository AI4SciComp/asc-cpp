#include <array>
#include <cstdint>

#include "asc/random.h"

int main() {
  const auto block =
      asc::Philox4x32_10(asc::Philox4x32Counter{}, asc::Philox4x32Key{});
  constexpr std::array<std::uint32_t, 4> kExpected = {
      0x6627e8d5U,
      0xe169c58dU,
      0xbc57ac4cU,
      0x9b00dbd8U,
  };
  if (block != kExpected) {
    return 1;
  }
  if (asc::Philox4x32Word(0, 0, 2) != kExpected[2]) {
    return 2;
  }
  const auto next = asc::AdvanceRandomOffset(3, 1);
  if (!next.ok() || *next != 4) {
    return 3;
  }
  return asc::Uniform01<float>(0xffffffffU) == 1.0F - 0x1p-24F ? 0 : 4;
}
