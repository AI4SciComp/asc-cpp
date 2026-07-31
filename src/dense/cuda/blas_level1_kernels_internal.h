#ifndef ASC_SRC_DENSE_CUDA_BLAS_LEVEL1_KERNELS_INTERNAL_H_
#define ASC_SRC_DENSE_CUDA_BLAS_LEVEL1_KERNELS_INTERNAL_H_

#include <complex>

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_dense_cuda {

Status LaunchSetScalar(void* stream, float value, float* destination);
Status LaunchSetScalar(void* stream, double value, double* destination);
Status LaunchSetScalar(void* stream, std::complex<float> value,
                       std::complex<float>* destination);
Status LaunchSetScalar(void* stream, std::complex<double> value,
                       std::complex<double>* destination);
Status LaunchSetScalar(void* stream, index_t value, index_t* destination);
Status LaunchSdsdot(void* stream, float bias, extent_t size, const float* left,
                    stride_t left_increment, const float* right,
                    stride_t right_increment, float* result);
Status LaunchDsdot(void* stream, extent_t size, const float* left,
                   stride_t left_increment, const float* right,
                   stride_t right_increment, double* result);

Status LaunchComplexRotg(void* stream, std::complex<float>* a,
                         const std::complex<float>* b, float* c,
                         std::complex<float>* s);
Status LaunchComplexRotg(void* stream, std::complex<double>* a,
                         const std::complex<double>* b, double* c,
                         std::complex<double>* s);

Status LaunchNegativeIamax(void* stream, extent_t size, const float* operand,
                           stride_t increment, index_t* provider_index);
Status LaunchNegativeIamax(void* stream, extent_t size, const double* operand,
                           stride_t increment, index_t* provider_index);
Status LaunchNegativeIamax(void* stream, extent_t size,
                           const std::complex<float>* operand,
                           stride_t increment, index_t* provider_index);
Status LaunchNegativeIamax(void* stream, extent_t size,
                           const std::complex<double>* operand,
                           stride_t increment, index_t* provider_index);

Status LaunchIamaxIndexConversion(void* stream, const index_t* provider_index,
                                  extent_t size, bool reversed,
                                  index_t* public_index);

}  // namespace asc::internal_dense_cuda

#endif  // ASC_SRC_DENSE_CUDA_BLAS_LEVEL1_KERNELS_INTERNAL_H_
