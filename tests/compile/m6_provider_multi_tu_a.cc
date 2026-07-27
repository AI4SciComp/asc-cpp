#include <cstdint>

#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "m6_provider_multi_tu.h"

asc::Result<std::int32_t> M6CudaCountFromFirstTranslationUnit() {
  return asc::CudaDeviceCount();
}
