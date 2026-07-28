#include "multi_tu.h"

int main() {
  constexpr int kExpectedWidth = 8;
  return CoreMultiTuA() == kExpectedWidth && CoreMultiTuB() == kExpectedWidth
             ? 0
             : 1;
}
