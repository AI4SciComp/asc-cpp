#include "m6_provider_odr.h"

int main() {
  const std::int32_t left = M6CudaOdrLeft();
  const std::int32_t right = M6CudaOdrRight();
  return left == right ? 0 : 1;
}
