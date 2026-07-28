#ifndef ASC_TESTS_COMPILE_M7_PROVIDER_ODR_H_
#define ASC_TESTS_COMPILE_M7_PROVIDER_ODR_H_

#include <cstddef>

#include "asc/random/providers/cuda.h"
#include "asc/random/providers/dense_cuda.h"
#include "asc/random/providers/sparse_cuda.h"
#include "asc/sparse/providers/cuda.h"

inline constexpr std::size_t kM7ProviderTypeSize =
    sizeof(asc::CudaRandomWordGeneration) +
    sizeof(asc::CudaDenseUniform01Generation<float, 2>) +
    sizeof(asc::CudaSparseUniform01Generation<float, asc::Extents<2, 3>>) +
    sizeof(asc::CudaStridedVectorView<float>);

std::size_t M7ProviderOdrA();
std::size_t M7ProviderOdrB();

#endif  // ASC_TESTS_COMPILE_M7_PROVIDER_ODR_H_
