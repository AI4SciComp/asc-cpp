#include <array>
#include <bit>
#include <cstdint>

#include "asc/random.h"

int main() {
  const asc::Philox4x32Counter counter = {0, 0, 0, 0};
  const asc::Philox4x32Key key = {0, 0};
  const asc::Philox4x32Result expected = {0x6627E8D5U, 0xE169C58DU, 0xBC57AC4CU,
                                          0x9B00DBD8U};
  if (asc::Philox4x32_10(counter, key) != expected) {
    return 1;
  }
  if (std::bit_cast<std::uint32_t>(asc::Uniform01<float>(0xFFFFFFFFU)) !=
      0x3F7FFFFFU) {
    return 2;
  }
  return 0;
}
