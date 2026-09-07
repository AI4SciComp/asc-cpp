#include <cuComplex.h>
#include <cuda_runtime_api.h>
#include <driver_types.h>

#include <cmath>
#include <complex>

#include "../../core/cuda/cuda_internal.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "blas_level1_kernels_internal.h"

namespace asc::internal_dense_cuda {
namespace {

template <typename Element>
__global__ void SetScalarKernel(Element value, Element* destination) {
  *destination = value;
}

__global__ void SdsdotKernel(float bias, extent_t size, const float* left,
                             stride_t left_increment, const float* right,
                             stride_t right_increment, float* result) {
  double sum = static_cast<double>(bias);
  for (index_t index = 0; index < size; ++index) {
    sum += static_cast<double>(left[index * left_increment]) *
           static_cast<double>(right[index * right_increment]);
  }
  *result = static_cast<float>(sum);
}

__global__ void DsdotKernel(extent_t size, const float* left,
                            stride_t left_increment, const float* right,
                            stride_t right_increment, double* result) {
  double sum = 0.0;
  for (index_t index = 0; index < size; ++index) {
    sum += static_cast<double>(left[index * left_increment]) *
           static_cast<double>(right[index * right_increment]);
  }
  *result = sum;
}

__global__ void SetComplexFloatKernel(float real, float imaginary,
                                      cuComplex* destination) {
  *destination = make_cuComplex(real, imaginary);
}

__global__ void SetComplexDoubleKernel(double real, double imaginary,
                                       cuDoubleComplex* destination) {
  *destination = make_cuDoubleComplex(real, imaginary);
}

__global__ void ComplexRotgKernel(cuComplex* a, const cuComplex* b, float* c,
                                  cuComplex* s) {
  const float absolute_a = hypotf(a->x, a->y);
  if (absolute_a == 0.0F) {
    *a = *b;
    *c = 0.0F;
    *s = make_cuComplex(1.0F, 0.0F);
    return;
  }
  const float norm = hypotf(absolute_a, hypotf(b->x, b->y));
  const float alpha_real = a->x / absolute_a;
  const float alpha_imaginary = a->y / absolute_a;
  *a = make_cuComplex(alpha_real * norm, alpha_imaginary * norm);
  *c = absolute_a / norm;
  *s = make_cuComplex((alpha_real * b->x + alpha_imaginary * b->y) / norm,
                      (alpha_imaginary * b->x - alpha_real * b->y) / norm);
}

__global__ void ComplexRotgKernel(cuDoubleComplex* a, const cuDoubleComplex* b,
                                  double* c, cuDoubleComplex* s) {
  const double absolute_a = hypot(a->x, a->y);
  if (absolute_a == 0.0) {
    *a = *b;
    *c = 0.0;
    *s = make_cuDoubleComplex(1.0, 0.0);
    return;
  }
  const double norm = hypot(absolute_a, hypot(b->x, b->y));
  const double alpha_real = a->x / absolute_a;
  const double alpha_imaginary = a->y / absolute_a;
  *a = make_cuDoubleComplex(alpha_real * norm, alpha_imaginary * norm);
  *c = absolute_a / norm;
  *s =
      make_cuDoubleComplex((alpha_real * b->x + alpha_imaginary * b->y) / norm,
                           (alpha_imaginary * b->x - alpha_real * b->y) / norm);
}

__device__ float BlasMagnitude(float value) { return fabsf(value); }

__device__ double BlasMagnitude(double value) { return fabs(value); }

__device__ float BlasMagnitude(cuComplex value) {
  return fabsf(value.x) + fabsf(value.y);
}

__device__ double BlasMagnitude(cuDoubleComplex value) {
  return fabs(value.x) + fabs(value.y);
}

template <typename Element>
__global__ void NegativeIamaxKernel(extent_t size, const Element* operand,
                                    stride_t increment,
                                    index_t* provider_index) {
  index_t maximum_index = 0;
  auto maximum = BlasMagnitude(operand[0]);
  for (index_t index = 1; index < size; ++index) {
    const auto magnitude = BlasMagnitude(operand[index * increment]);
    if (magnitude > maximum) {
      maximum = magnitude;
      maximum_index = index;
    }
  }
  *provider_index = maximum_index + index_t{1};
}

__global__ void IamaxIndexConversionKernel(const index_t* provider_index,
                                           extent_t size, bool reversed,
                                           index_t* public_index) {
  const index_t zero_based = *provider_index - index_t{1};
  *public_index = reversed ? size - index_t{1} - zero_based : zero_based;
}

Status LastLaunchStatus(const char* message) {
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kProvider, message);
  }
  return Status::Ok();
}

}  // namespace

Status LaunchSetScalar(void* stream, float value, float* destination) {
  SetScalarKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(value,
                                                                  destination);
  return LastLaunchStatus("CUDA could not launch a float scalar write");
}

Status LaunchSetScalar(void* stream, double value, double* destination) {
  SetScalarKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(value,
                                                                  destination);
  return LastLaunchStatus("CUDA could not launch a double scalar write");
}

Status LaunchSetScalar(void* stream, std::complex<float> value,
                       std::complex<float>* destination) {
  static_assert(sizeof(std::complex<float>) == sizeof(cuComplex));
  SetComplexFloatKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      value.real(), value.imag(), reinterpret_cast<cuComplex*>(destination));
  return LastLaunchStatus("CUDA could not launch a complex-float scalar write");
}

Status LaunchSetScalar(void* stream, std::complex<double> value,
                       std::complex<double>* destination) {
  static_assert(sizeof(std::complex<double>) == sizeof(cuDoubleComplex));
  SetComplexDoubleKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      value.real(), value.imag(),
      reinterpret_cast<cuDoubleComplex*>(destination));
  return LastLaunchStatus(
      "CUDA could not launch a complex-double scalar write");
}

Status LaunchSetScalar(void* stream, index_t value, index_t* destination) {
  SetScalarKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(value,
                                                                  destination);
  return LastLaunchStatus("CUDA could not launch an index scalar write");
}

Status LaunchSdsdot(void* stream, float bias, extent_t size, const float* left,
                    stride_t left_increment, const float* right,
                    stride_t right_increment, float* result) {
  SdsdotKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      bias, size, left, left_increment, right, right_increment, result);
  return LastLaunchStatus("CUDA could not launch extended-precision Sdsdot");
}

Status LaunchDsdot(void* stream, extent_t size, const float* left,
                   stride_t left_increment, const float* right,
                   stride_t right_increment, double* result) {
  DsdotKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      size, left, left_increment, right, right_increment, result);
  return LastLaunchStatus("CUDA could not launch mixed-precision Dot");
}

Status LaunchComplexRotg(void* stream, std::complex<float>* a,
                         const std::complex<float>* b, float* c,
                         std::complex<float>* s) {
  static_assert(sizeof(std::complex<float>) == sizeof(cuComplex));
  ComplexRotgKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      reinterpret_cast<cuComplex*>(a), reinterpret_cast<const cuComplex*>(b), c,
      reinterpret_cast<cuComplex*>(s));
  return LastLaunchStatus("CUDA could not launch complex-float Rotg");
}

Status LaunchComplexRotg(void* stream, std::complex<double>* a,
                         const std::complex<double>* b, double* c,
                         std::complex<double>* s) {
  static_assert(sizeof(std::complex<double>) == sizeof(cuDoubleComplex));
  ComplexRotgKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      reinterpret_cast<cuDoubleComplex*>(a),
      reinterpret_cast<const cuDoubleComplex*>(b), c,
      reinterpret_cast<cuDoubleComplex*>(s));
  return LastLaunchStatus("CUDA could not launch complex-double Rotg");
}

template <typename Element>
Status LaunchNegativeIamaxImpl(void* stream, extent_t size,
                               const Element* operand, stride_t increment,
                               index_t* provider_index) {
  NegativeIamaxKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      size, operand, increment, provider_index);
  return LastLaunchStatus("CUDA could not launch negative-stride Iamax");
}

Status LaunchNegativeIamax(void* stream, extent_t size, const float* operand,
                           stride_t increment, index_t* provider_index) {
  return LaunchNegativeIamaxImpl(stream, size, operand, increment,
                                 provider_index);
}

Status LaunchNegativeIamax(void* stream, extent_t size, const double* operand,
                           stride_t increment, index_t* provider_index) {
  return LaunchNegativeIamaxImpl(stream, size, operand, increment,
                                 provider_index);
}

Status LaunchNegativeIamax(void* stream, extent_t size,
                           const std::complex<float>* operand,
                           stride_t increment, index_t* provider_index) {
  static_assert(sizeof(std::complex<float>) == sizeof(cuComplex));
  return LaunchNegativeIamaxImpl(stream, size,
                                 reinterpret_cast<const cuComplex*>(operand),
                                 increment, provider_index);
}

Status LaunchNegativeIamax(void* stream, extent_t size,
                           const std::complex<double>* operand,
                           stride_t increment, index_t* provider_index) {
  static_assert(sizeof(std::complex<double>) == sizeof(cuDoubleComplex));
  return LaunchNegativeIamaxImpl(
      stream, size, reinterpret_cast<const cuDoubleComplex*>(operand),
      increment, provider_index);
}

Status LaunchIamaxIndexConversion(void* stream, const index_t* provider_index,
                                  extent_t size, bool reversed,
                                  index_t* public_index) {
  IamaxIndexConversionKernel<<<1, 1, 0, static_cast<cudaStream_t>(stream)>>>(
      provider_index, size, reversed, public_index);
  return LastLaunchStatus("CUDA could not launch Iamax index conversion");
}

}  // namespace asc::internal_dense_cuda
