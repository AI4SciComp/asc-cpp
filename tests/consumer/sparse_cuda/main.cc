#include <array>
#include <cstddef>
#include <cstdint>

#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/sparse/providers/cuda.h"

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
  auto context = asc::SparseCudaContext::Create(*execution);
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
  constexpr std::array<asc::index_t, 2> kIndices{0, 2};
  constexpr std::array<float, 2> kSparseValues{2.0F, -1.0F};
  auto host_sparse = asc::SparseBlasIndexedVectorView<const float>::Create(
      kIndices.data(), kSparseValues.data(), 2, 3,
      {kIndices.data(), sizeof(kIndices), asc::MemorySpace::kHost},
      {kSparseValues.data(), sizeof(kSparseValues), asc::MemorySpace::kHost});
  if (!host_sparse.ok()) {
    return 6;
  }
  auto sparse = asc::CudaCloneIndexedVector(*context, *host_sparse, **device);
  if (!sparse.ok() || !sparse->completion.Wait().ok()) {
    return 7;
  }
  auto host_dense =
      asc::Buffer::Allocate(**pinned, 3 * sizeof(float), alignof(float));
  auto device_dense =
      asc::Buffer::Allocate(**device, 3 * sizeof(float), alignof(float));
  if (!host_dense.ok() || !device_dense.ok()) {
    return 8;
  }
  auto* dense_values = static_cast<float*>(host_dense->data());
  dense_values[0] = 3.0F;
  dense_values[1] = 4.0F;
  dense_values[2] = 5.0F;
  auto host_source = host_dense->const_view();
  auto device_destination = device_dense->mutable_view();
  if (!host_source.ok() || !device_destination.ok()) {
    return 9;
  }
  auto copied = asc::CopyBytes(*execution, *device_destination, *host_source);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 10;
  }
  auto sparse_view = sparse->array.view();
  auto dense = asc::SparseBlasVectorView<float>::Create(
      static_cast<float*>(device_dense->data()), 3, 1,
      {device_dense->data(), device_dense->size(), asc::MemorySpace::kDevice});
  if (!sparse_view.ok() || !dense.ok()) {
    return 11;
  }
  const asc::SparseBlasIndexedVectorView<const float> const_sparse(
      *sparse_view);
  auto axpy = asc::CudaSparseAxpy(*context, 2.0F, const_sparse, *dense);
  if (!axpy.ok() || !axpy->Wait().ok()) {
    return 12;
  }
  auto host_destination = host_dense->mutable_view();
  auto device_source = device_dense->const_view();
  if (!host_destination.ok() || !device_source.ok()) {
    return 13;
  }
  copied = asc::CopyBytes(*execution, *host_destination, *device_source);
  if (!copied.ok() || !copied->Wait().ok()) {
    return 14;
  }
  return dense_values[0] == 7.0F && dense_values[1] == 4.0F &&
                 dense_values[2] == 3.0F
             ? 0
             : 15;
}
