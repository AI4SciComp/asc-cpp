#include <cstdint>

std::uint64_t RandomOdrA();
double RandomOdrB();

int main() {
  return RandomOdrA() == 0x6627e8d5e169c58dULL &&
                 RandomOdrB() == 0x1.989fa35785a70p-2
             ? 0
             : 1;
}
