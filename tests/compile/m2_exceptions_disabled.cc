#include <cstdint>
#include <span>

#include "asc/expression.h"
#include "asc/random.h"
#include "asc/utilities.h"

int main() {
  asc::Timer timer;
  if (!timer.Start().ok()) {
    return 1;
  }
  if (!timer.Stop().ok()) {
    return 2;
  }

  const asc::Philox4x32Result block = asc::Philox4x32_10({0, 0, 0, 0}, {0, 0});
  if (block[0] != 0x6627E8D5U) {
    return 3;
  }
  if (asc::Uniform01<float>(std::uint32_t{0}) != 0.0F) {
    return 4;
  }

  auto expression = asc::MakeAdd(2, 3);
  if (!expression.ok() ||
      asc::ExpressionRead(*expression, std::span<const asc::index_t, 0>()) !=
          5) {
    return 5;
  }
  return 0;
}
