#include <array>
#include <bit>
#include <cstdint>
#include <random>
#include <type_traits>

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

  asc::SplitMix64 splitmix(0);
  if (splitmix() != 0xE220A8397B1DCDAFULL) {
    return 3;
  }

  auto distribution = asc::UniformRealDistribution<double>::Create(2, 6);
  if (!distribution.ok()) {
    return 4;
  }
  asc::UniformGenerator<asc::Pcg32, double> generator(asc::Pcg32(42, 54),
                                                      *distribution);
  auto value = generator();
  if (!value.ok() || *value < 2 || !(*value < 6)) {
    return 5;
  }

  using SeedFunction =
      asc::Result<std::uint64_t> (*)(std::random_device& source);
  static_assert(std::is_same_v<decltype(&asc::AcquireNondeterministicSeed),
                               SeedFunction>);
  return 0;
}
