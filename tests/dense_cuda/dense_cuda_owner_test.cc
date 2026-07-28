#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>

#include "../../src/core/cuda/runtime_test_internal.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/array.h"
#include "asc/dense/layout.h"
#include "test_support.h"

namespace {

class PatternMemoryResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    requested_bytes_ = bytes;
    auto allocated = backing_.Allocate(bytes, alignment);
    if (!allocated.ok()) {
      return allocated.status();
    }
    if (*allocated != nullptr) {
      std::fill_n(static_cast<unsigned char*>(*allocated), bytes,
                  static_cast<unsigned char>(0xa5));
    }
    return *allocated;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocation_calls_;
    backing_.Deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] std::size_t allocation_calls() const noexcept {
    return allocation_calls_;
  }
  [[nodiscard]] std::size_t deallocation_calls() const noexcept {
    return deallocation_calls_;
  }
  [[nodiscard]] std::size_t requested_bytes() const noexcept {
    return requested_bytes_;
  }

 private:
  asc::HostMemoryResource backing_;
  std::size_t allocation_calls_ = 0;
  std::size_t deallocation_calls_ = 0;
  std::size_t requested_bytes_ = 0;
};

template <typename Element, std::size_t Rank>
asc::Result<Element*> At(asc::DenseView<Element, Rank> view,
                         const std::array<asc::index_t, Rank>& coordinate) {
  return view.At(std::span<const asc::index_t, Rank>(coordinate));
}

void TestUninitializedAndValueInitialized(
    asc_dense_cuda_test::TestContext& test) {
  using ByteExtents = asc::Extents<asc::kDynamicExtent>;
  auto extents = ByteExtents::Create(73);
  ASC_DENSE_CUDA_CHECK(test, extents.ok());
  if (!extents.ok()) {
    return;
  }

  PatternMemoryResource resource;
  {
    auto uninitialized =
        asc::DenseArray<unsigned char, ByteExtents>::CreateUninitialized(
            *extents, resource);
    ASC_DENSE_CUDA_CHECK(test, uninitialized.ok());
    if (uninitialized.ok()) {
      ASC_DENSE_CUDA_EQ(test, resource.requested_bytes(), std::size_t{73});
      auto view = uninitialized->view();
      ASC_DENSE_CUDA_CHECK(test, view.ok());
      if (view.ok()) {
        for (asc::index_t index = 0; index < 73; ++index) {
          auto value = At(*view, std::array<asc::index_t, 1>{index});
          ASC_DENSE_CUDA_CHECK(test, value.ok());
          if (value.ok()) {
            ASC_DENSE_CUDA_EQ(test, **value, static_cast<unsigned char>(0xa5));
          }
        }
      }
    }
  }
  ASC_DENSE_CUDA_EQ(test, resource.allocation_calls(), std::size_t{1});
  ASC_DENSE_CUDA_EQ(test, resource.deallocation_calls(), std::size_t{1});

  {
    auto initialized =
        asc::DenseArray<unsigned char, ByteExtents>::Create(resource, *extents);
    ASC_DENSE_CUDA_CHECK(test, initialized.ok());
    if (initialized.ok()) {
      auto view = initialized->view();
      ASC_DENSE_CUDA_CHECK(test, view.ok());
      if (view.ok()) {
        for (asc::index_t index = 0; index < 73; ++index) {
          auto value = At(*view, std::array<asc::index_t, 1>{index});
          ASC_DENSE_CUDA_CHECK(test, value.ok());
          if (value.ok()) {
            ASC_DENSE_CUDA_EQ(test, **value, static_cast<unsigned char>(0));
          }
        }
      }
    }
  }
}

void TestMemorySpacesAndClone(asc_dense_cuda_test::TestContext& test) {
  using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto extents = MatrixExtents::Create(5, 7);
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, extents.ok());
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!extents.ok() || !count.ok() || *count == 0) {
    return;
  }

  auto pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto second_pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto managed = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kManaged);
  auto context = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, pinned.ok());
  ASC_DENSE_CUDA_CHECK(test, second_pinned.ok());
  ASC_DENSE_CUDA_CHECK(test, device.ok());
  ASC_DENSE_CUDA_CHECK(test, managed.ok());
  ASC_DENSE_CUDA_CHECK(test, context.ok());
  if (!pinned.ok() || !second_pinned.ok() || !device.ok() || !managed.ok() ||
      !context.ok()) {
    return;
  }

  auto pinned_array =
      asc::DenseArray<double, MatrixExtents>::CreateUninitialized(
          *extents, **pinned, asc::LayoutRight{});
  auto device_array =
      asc::DenseArray<double, MatrixExtents>::CreateUninitialized(*extents,
                                                                  **device);
  auto managed_array =
      asc::DenseArray<double, MatrixExtents>::CreateUninitialized(*extents,
                                                                  **managed);
  ASC_DENSE_CUDA_CHECK(test, pinned_array.ok());
  ASC_DENSE_CUDA_CHECK(test, device_array.ok());
  ASC_DENSE_CUDA_CHECK(test, managed_array.ok());
  if (!pinned_array.ok() || !device_array.ok() || !managed_array.ok()) {
    return;
  }

  auto pinned_view = pinned_array->view();
  auto device_view = device_array->view();
  auto managed_view = managed_array->view();
  ASC_DENSE_CUDA_CHECK(test, pinned_view.ok());
  ASC_DENSE_CUDA_CHECK(test, device_view.ok());
  ASC_DENSE_CUDA_CHECK(test, managed_view.ok());
  if (!pinned_view.ok() || !device_view.ok() || !managed_view.ok()) {
    return;
  }

  for (asc::index_t row = 0; row < 5; ++row) {
    for (asc::index_t column = 0; column < 7; ++column) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element = At(*pinned_view, coordinate);
      ASC_DENSE_CUDA_CHECK(test, element.ok());
      if (element.ok()) {
        **element = static_cast<double>(3 * row - 2 * column);
      }
    }
  }

  auto device_access = At(*device_view, std::array<asc::index_t, 2>{0, 0});
  auto managed_access = At(*managed_view, std::array<asc::index_t, 2>{0, 0});
  ASC_DENSE_CUDA_CHECK(test, !device_access.ok());
  ASC_DENSE_CUDA_CHECK(test, !managed_access.ok());
  if (!device_access.ok()) {
    ASC_DENSE_CUDA_EQ(test, device_access.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }
  if (!managed_access.ok()) {
    ASC_DENSE_CUDA_EQ(test, managed_access.status().code(),
                      asc::ErrorCode::kMemoryAccess);
  }

  auto device_clone = pinned_array->Clone(**device, *context);
  ASC_DENSE_CUDA_CHECK(test, device_clone.ok());
  if (!device_clone.ok()) {
    return;
  }
#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
  asc::internal_core_cuda::ResetCudaRuntimeTestState();
  asc::internal_core_cuda::SetCudaRuntimeFault(
      asc::internal_core_cuda::CudaRuntimeFault::kEventRecord);
  auto failed_clone = device_clone->Clone(**second_pinned, *context);
  ASC_DENSE_CUDA_CHECK(test, !failed_clone.ok());
  if (!failed_clone.ok()) {
    ASC_DENSE_CUDA_EQ(test, failed_clone.status().code(),
                      asc::ErrorCode::kProvider);
    ASC_DENSE_CUDA_CHECK(test, failed_clone.status().native_code() != 0);
  }
  ASC_DENSE_CUDA_EQ(test, asc::internal_core_cuda::CudaRuntimeDrainCount(),
                    std::size_t{1});
  asc::internal_core_cuda::ResetCudaRuntimeTestState();
#endif
  auto round_trip = device_clone->Clone(**second_pinned, *context);
  ASC_DENSE_CUDA_CHECK(test, round_trip.ok());
  if (!round_trip.ok()) {
    return;
  }
  auto round_trip_view = round_trip->view();
  ASC_DENSE_CUDA_CHECK(test, round_trip_view.ok());
  if (!round_trip_view.ok()) {
    return;
  }
  ASC_DENSE_CUDA_EQ(test, round_trip->mapping().kind(),
                    asc::DenseLayoutKind::kRight);
  for (asc::index_t row = 0; row < 5; ++row) {
    for (asc::index_t column = 0; column < 7; ++column) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto actual = At(*round_trip_view, coordinate);
      ASC_DENSE_CUDA_CHECK(test, actual.ok());
      if (actual.ok()) {
        ASC_DENSE_CUDA_EQ(test, **actual,
                          static_cast<double>(3 * row - 2 * column));
      }
    }
  }

  auto forbidden_initialized =
      asc::DenseArray<double, MatrixExtents>::Create(**device, *extents);
  ASC_DENSE_CUDA_CHECK(test, !forbidden_initialized.ok());
  if (!forbidden_initialized.ok()) {
    ASC_DENSE_CUDA_EQ(test, forbidden_initialized.status().code(),
                      asc::ErrorCode::kUnsupported);
  }
}

void TestRankZeroZeroExtentAndRankEight(
    asc_dense_cuda_test::TestContext& test) {
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!count.ok() || *count == 0) {
    return;
  }
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  ASC_DENSE_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }

  auto scalar_extents = asc::Extents<>::Create();
  ASC_DENSE_CUDA_CHECK(test, scalar_extents.ok());
  if (scalar_extents.ok()) {
    auto scalar = asc::DenseArray<float, asc::Extents<>>::CreateUninitialized(
        *scalar_extents, **device);
    ASC_DENSE_CUDA_CHECK(test, scalar.ok());
    if (scalar.ok()) {
      ASC_DENSE_CUDA_EQ(test, scalar->logical_size(), asc::extent_t{1});
      ASC_DENSE_CUDA_EQ(test, scalar->mapping().required_span_size(),
                        std::size_t{1});
    }
  }

  using EmptyExtents = asc::Extents<asc::kDynamicExtent, 3>;
  auto empty_extents = EmptyExtents::Create(0);
  ASC_DENSE_CUDA_CHECK(test, empty_extents.ok());
  if (empty_extents.ok()) {
    auto empty = asc::DenseArray<float, EmptyExtents>::CreateUninitialized(
        *empty_extents, **device, asc::LayoutRight{});
    ASC_DENSE_CUDA_CHECK(test, empty.ok());
    if (empty.ok()) {
      ASC_DENSE_CUDA_EQ(test, empty->logical_size(), asc::extent_t{0});
      ASC_DENSE_CUDA_EQ(test, empty->mapping().required_span_size(),
                        std::size_t{0});
      auto view = empty->view();
      ASC_DENSE_CUDA_CHECK(test, view.ok());
      if (view.ok()) {
        ASC_DENSE_CUDA_EQ(test, view->data(), nullptr);
      }
    }
  }

  using RankEight = asc::Extents<1, 1, 1, 1, 1, 1, 1, asc::kDynamicExtent>;
  auto rank_eight_extents = RankEight::Create(2);
  ASC_DENSE_CUDA_CHECK(test, rank_eight_extents.ok());
  if (rank_eight_extents.ok()) {
    auto rank_eight = asc::DenseArray<double, RankEight>::CreateUninitialized(
        *rank_eight_extents, **device);
    ASC_DENSE_CUDA_CHECK(test, rank_eight.ok());
    if (rank_eight.ok()) {
      ASC_DENSE_CUDA_EQ(test, rank_eight->logical_size(), asc::extent_t{2});
    }
  }
}

}  // namespace

int main() {
  if (asc_dense_cuda_test::ForceNoCudaDevice()) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  asc_dense_cuda_test::TestContext test;
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }
  if (*count == 0) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  TestUninitializedAndValueInitialized(test);
  TestMemorySpacesAndClone(test);
  TestRankZeroZeroExtentAndRankEight(test);
  return test.Finish();
}
