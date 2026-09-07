#include <cstdint>

#include "asc/core/providers/cuda.h"
#include "m6_provider_odr.h"

std::int32_t M6CudaOdrRight() {
  const auto count = asc::CudaDeviceCount();
  return count.ok() ? *count : std::int32_t{-1};
}
