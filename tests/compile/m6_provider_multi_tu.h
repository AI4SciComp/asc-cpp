#ifndef ASC_TESTS_COMPILE_M6_PROVIDER_MULTI_TU_H_
#define ASC_TESTS_COMPILE_M6_PROVIDER_MULTI_TU_H_

#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/result.h"

asc::Result<std::int32_t> M6CudaCountFromFirstTranslationUnit();
asc::Result<asc::ExecutionContext> M6CudaContextFromSecondTranslationUnit();

#endif  // ASC_TESTS_COMPILE_M6_PROVIDER_MULTI_TU_H_
