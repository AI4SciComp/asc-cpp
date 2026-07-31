#include <cstddef>
#include <cstdint>

#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"

int main() {
  const asc::Result<std::int32_t> count = asc::CudaDeviceCount();
  if (!count.ok()) {
    return 1;
  }
  if (*count == 0) {
    return 0;
  }
  auto execution = asc::CreateCudaExecutionContext(0);
  if (!execution.ok()) {
    return 2;
  }
  auto context = asc::DenseCudaContext::Create(*execution);
  if (!context.ok()) {
    return 3;
  }
  if (context->execution_context().backend() != asc::Backend::kCuda) {
    return 4;
  }

  auto pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  if (!pinned.ok() || !device.ok()) {
    return 5;
  }
  auto host_values =
      asc::Buffer::Allocate(**pinned, 3 * sizeof(float), alignof(float));
  auto device_values =
      asc::Buffer::Allocate(**device, 3 * sizeof(float), alignof(float));
  auto host_result =
      asc::Buffer::Allocate(**pinned, sizeof(float), alignof(float));
  auto device_result =
      asc::Buffer::Allocate(**device, sizeof(float), alignof(float));
  if (!host_values.ok() || !device_values.ok() || !host_result.ok() ||
      !device_result.ok()) {
    return 6;
  }
  auto* values = static_cast<float*>(host_values->data());
  values[0] = 1.0F;
  values[1] = -2.0F;
  values[2] = 3.0F;
  auto host_values_view = host_values->const_view();
  auto device_values_view = device_values->mutable_view();
  if (!host_values_view.ok() || !device_values_view.ok()) {
    return 7;
  }
  auto copied =
      asc::CopyBytes(*execution, *device_values_view, *host_values_view);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 8;
  }
  auto operand = asc::DenseBlasVectorView<const float>::Create(
      static_cast<const float*>(device_values->data()), 3, 1,
      asc::ConstMemoryView(device_values->data(), device_values->size(),
                           asc::MemorySpace::kDevice));
  auto result = asc::DenseBlasVectorView<float>::Create(
      static_cast<float*>(device_result->data()), 1, 1,
      asc::ConstMemoryView(device_result->data(), device_result->size(),
                           asc::MemorySpace::kDevice));
  if (!operand.ok() || !result.ok()) {
    return 9;
  }
  auto reduced = asc::CudaAsum(*context, *operand, *result);
  if (!reduced.ok() || !reduced->Wait().ok()) {
    return 10;
  }
  auto host_result_view = host_result->mutable_view();
  auto device_result_view = device_result->const_view();
  if (!host_result_view.ok() || !device_result_view.ok()) {
    return 11;
  }
  copied = asc::CopyBytes(*execution, *host_result_view, *device_result_view);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 12;
  }
  if (*static_cast<const float*>(host_result->data()) != 6.0F) {
    return 13;
  }

  auto host_level2 =
      asc::Buffer::Allocate(**pinned, 8 * sizeof(float), alignof(float));
  auto device_level2 =
      asc::Buffer::Allocate(**device, 8 * sizeof(float), alignof(float));
  if (!host_level2.ok() || !device_level2.ok()) {
    return 14;
  }
  auto* level2_values = static_cast<float*>(host_level2->data());
  level2_values[0] = 1.0F;
  level2_values[1] = 3.0F;
  level2_values[2] = 2.0F;
  level2_values[3] = 4.0F;
  level2_values[4] = 1.0F;
  level2_values[5] = 1.0F;
  level2_values[6] = 0.0F;
  level2_values[7] = 0.0F;
  auto host_level2_view = host_level2->const_view();
  auto device_level2_view = device_level2->mutable_view();
  if (!host_level2_view.ok() || !device_level2_view.ok()) {
    return 15;
  }
  copied = asc::CopyBytes(*execution, *device_level2_view, *host_level2_view);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 16;
  }
  const asc::ConstMemoryView level2_backing(
      device_level2->data(), device_level2->size(), asc::MemorySpace::kDevice);
  auto matrix = asc::DenseBlasMatrixView<const float>::Create(
      static_cast<const float*>(device_level2->data()), 2, 2,
      asc::DenseBlasLayout::kColumnMajor, 2, level2_backing);
  auto input = asc::DenseBlasVectorView<const float>::Create(
      static_cast<const float*>(device_level2->data()) + 4, 2, 1,
      level2_backing);
  auto output = asc::DenseBlasVectorView<float>::Create(
      static_cast<float*>(device_level2->data()) + 6, 2, 1, level2_backing);
  if (!matrix.ok() || !input.ok() || !output.ok()) {
    return 17;
  }
  auto multiplied = asc::CudaGemv(*context, asc::DenseBlasTranspose::kNone,
                                  1.0F, *matrix, *input, 0.0F, *output);
  if (!multiplied.ok() || !multiplied->Wait().ok()) {
    return 18;
  }
  auto host_level2_destination = host_level2->mutable_view();
  auto device_level2_source = device_level2->const_view();
  if (!host_level2_destination.ok() || !device_level2_source.ok()) {
    return 19;
  }
  copied = asc::CopyBytes(*execution, *host_level2_destination,
                          *device_level2_source);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 20;
  }
  if (level2_values[6] != 3.0F || level2_values[7] != 7.0F) {
    return 21;
  }

  auto host_level3 =
      asc::Buffer::Allocate(**pinned, 12 * sizeof(float), alignof(float));
  auto device_level3 =
      asc::Buffer::Allocate(**device, 12 * sizeof(float), alignof(float));
  if (!host_level3.ok() || !device_level3.ok()) {
    return 22;
  }
  auto* level3_values = static_cast<float*>(host_level3->data());
  constexpr float kLevel3Values[12] = {1.0F, 3.0F, 2.0F, 4.0F, 2.0F, 1.0F,
                                       0.0F, 2.0F, 0.0F, 0.0F, 0.0F, 0.0F};
  for (std::size_t index = 0; index < 12; ++index) {
    level3_values[index] = kLevel3Values[index];
  }
  auto host_level3_source = host_level3->const_view();
  auto device_level3_destination = device_level3->mutable_view();
  if (!host_level3_source.ok() || !device_level3_destination.ok()) {
    return 23;
  }
  copied = asc::CopyBytes(*execution, *device_level3_destination,
                          *host_level3_source);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 24;
  }
  const asc::ConstMemoryView level3_backing(
      device_level3->data(), device_level3->size(), asc::MemorySpace::kDevice);
  auto level3_left = asc::DenseBlasMatrixView<const float>::Create(
      static_cast<const float*>(device_level3->data()), 2, 2,
      asc::DenseBlasLayout::kColumnMajor, 2, level3_backing);
  auto level3_right = asc::DenseBlasMatrixView<const float>::Create(
      static_cast<const float*>(device_level3->data()) + 4, 2, 2,
      asc::DenseBlasLayout::kColumnMajor, 2, level3_backing);
  auto level3_output = asc::DenseBlasMatrixView<float>::Create(
      static_cast<float*>(device_level3->data()) + 8, 2, 2,
      asc::DenseBlasLayout::kColumnMajor, 2, level3_backing);
  if (!level3_left.ok() || !level3_right.ok() || !level3_output.ok()) {
    return 25;
  }
  auto level3_multiplied = asc::CudaGemm(
      *context, asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kNone,
      1.0F, *level3_left, *level3_right, 0.0F, *level3_output);
  if (!level3_multiplied.ok() || !level3_multiplied->Wait().ok()) {
    return 26;
  }
  auto host_level3_destination = host_level3->mutable_view();
  auto device_level3_source = device_level3->const_view();
  if (!host_level3_destination.ok() || !device_level3_source.ok()) {
    return 27;
  }
  copied = asc::CopyBytes(*execution, *host_level3_destination,
                          *device_level3_source);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 28;
  }
  constexpr float kExpectedLevel3[4] = {4.0F, 10.0F, 4.0F, 8.0F};
  for (std::size_t index = 0; index < 4; ++index) {
    if (level3_values[index + 8] != kExpectedLevel3[index]) {
      return 29;
    }
  }
  return 0;
}
