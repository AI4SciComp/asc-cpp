#include <asc/random.h>

#include <cstdint>

std::uint64_t RandomOdrA() {
  const auto words = asc::Philox4x32_10::Generate(
      asc::RandomKey{0}, asc::RandomCounter{0, 0});
  return (static_cast<std::uint64_t>(words[0]) << 32U) | words[1];
}
