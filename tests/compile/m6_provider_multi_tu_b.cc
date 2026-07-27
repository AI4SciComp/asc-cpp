#include "asc/core/execution.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "m6_provider_multi_tu.h"

asc::Result<asc::ExecutionContext> M6CudaContextFromSecondTranslationUnit() {
  return asc::CreateCudaExecutionContext(asc::Device{asc::Backend::kCuda, 0});
}
