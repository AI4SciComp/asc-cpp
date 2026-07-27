#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "allocation_counter.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"

namespace {

#if defined(__clang__)
constexpr const char* kCompiler = "Clang " __clang_version__;
#elif defined(__GNUC__)
constexpr const char* kCompiler = "GCC " __VERSION__;
#elif defined(_MSC_VER)
constexpr const char* kCompiler = "MSVC";
#else
constexpr const char* kCompiler = "unknown";
#endif

#if defined(NDEBUG)
constexpr const char* kConfiguration = "Release-like";
#else
constexpr const char* kConfiguration = "Debug-like";
#endif

constexpr asc::extent_t kDenseRows = 64;
constexpr asc::extent_t kDenseColumns = 64;
constexpr asc::stride_t kDenseLeadingStride = 80;
constexpr std::size_t kDenseSpan = 5104;
constexpr std::size_t kDenseIterations = 500;
constexpr asc::extent_t kSparseRows = 32;
constexpr asc::extent_t kSparseColumns = 32;
constexpr asc::nnz_t kSparseCount = 64;
constexpr std::size_t kSparseIterations = 20;

class CountingResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    return host_.Allocate(bytes, alignment);
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      ++deallocation_calls_;
    }
    host_.Deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] std::size_t allocation_calls() const noexcept {
    return allocation_calls_;
  }
  [[nodiscard]] std::size_t deallocation_calls() const noexcept {
    return deallocation_calls_;
  }

 private:
  asc::HostMemoryResource host_;
  std::size_t allocation_calls_ = 0;
  std::size_t deallocation_calls_ = 0;
};

}  // namespace

int main() {
  constexpr std::array<asc::extent_t, 2> kDenseShape{kDenseRows, kDenseColumns};
  constexpr std::array<asc::stride_t, 2> kDenseStrides{1, kDenseLeadingStride};
  const auto dense_mapping = asc::DenseLayoutMapping<2>::Create(
      asc::LayoutStride{}, kDenseShape, kDenseStrides);
  if (!dense_mapping.ok()) {
    return 1;
  }
  std::array<float, kDenseSpan> dense_storage{};
  const auto dense_view = asc::DenseView<float, 2>::Create(
      dense_storage.data(), *dense_mapping, asc::MemorySpace::kHost);
  if (!dense_view.ok()) {
    return 2;
  }
  for (std::size_t warmup = 0; warmup < 5; ++warmup) {
    if (!asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *dense_view,
                                 1, 2, warmup * 4096U)
             .ok()) {
      return 3;
    }
  }

  asc::RandomOffset dense_next = 0;
  std::size_t dense_allocations = 1;
  const auto dense_start = std::chrono::steady_clock::now();
  {
    asc_random_storage_benchmark::AllocationCountScope allocation_scope;
    for (std::size_t iteration = 0; iteration < kDenseIterations; ++iteration) {
      const auto result =
          asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *dense_view,
                                  1, 2, iteration * 4096U);
      if (!result.ok()) {
        return 4;
      }
      dense_next = *result;
    }
    dense_allocations = allocation_scope.count();
  }
  const auto dense_stop = std::chrono::steady_clock::now();
  double dense_checksum = 0.0;
  for (asc::index_t column = 0; column < kDenseColumns; ++column) {
    for (asc::index_t row = 0; row < kDenseRows; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      const auto value = dense_view->At(coordinate);
      if (!value.ok()) {
        return 5;
      }
      dense_checksum += **value;
    }
  }

  const auto sparse_extents =
      asc::Extents<kSparseRows, kSparseColumns>::Create();
  if (!sparse_extents.ok()) {
    return 6;
  }
  CountingResource sparse_resource;
  double sparse_checksum = 0.0;
  asc::RandomOffset sparse_structure_next = 0;
  asc::RandomOffset sparse_value_next = 0;
  const auto sparse_start = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration < kSparseIterations; ++iteration) {
    auto generated = asc::GenerateSparseUniform01<double>(
        asc::ExecutionContext::Serial(), *sparse_extents, kSparseCount,
        sparse_resource, 10, 20, iteration * 2048U, 11, 20, iteration * 128U);
    if (!generated.ok()) {
      return 7;
    }
    sparse_structure_next = generated->next_structure_offset;
    sparse_value_next = generated->next_value_offset;
    auto view = generated->array.view();
    if (!view.ok()) {
      return 8;
    }
    for (asc::nnz_t position = 0; position < view->nnz(); ++position) {
      auto coordinate = view->CoordinateAt(position);
      auto value = view->ValueAt(position);
      if (!coordinate.ok() || !value.ok()) {
        return 9;
      }
      sparse_checksum +=
          static_cast<double>((*coordinate)[0] + (*coordinate)[1]) + **value;
    }
  }
  const auto sparse_stop = std::chrono::steady_clock::now();

  const auto dense_elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(dense_stop -
                                                            dense_start);
  const auto sparse_elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(sparse_stop -
                                                            sparse_start);

  std::cout << "compiler=" << kCompiler << '\n'
            << "configuration=" << kConfiguration << '\n'
            << "backend=serial-reference\n"
            << "dense_shape=" << kDenseRows << 'x' << kDenseColumns << '\n'
            << "dense_layout=unique-padded-stride\n"
            << "dense_iterations=" << kDenseIterations << '\n'
            << "dense_allocations_in_operations=" << dense_allocations << '\n'
            << "dense_elapsed_us=" << dense_elapsed.count() << '\n'
            << "dense_next_offset=" << dense_next << '\n'
            << "dense_checksum=" << dense_checksum << '\n'
            << "sparse_shape=" << kSparseRows << 'x' << kSparseColumns << '\n'
            << "sparse_count=" << kSparseCount << '\n'
            << "sparse_iterations=" << kSparseIterations << '\n'
            << "sparse_resource_allocation_calls="
            << sparse_resource.allocation_calls() << '\n'
            << "sparse_resource_deallocation_calls="
            << sparse_resource.deallocation_calls() << '\n'
            << "sparse_elapsed_us=" << sparse_elapsed.count() << '\n'
            << "sparse_next_structure_offset=" << sparse_structure_next << '\n'
            << "sparse_next_value_offset=" << sparse_value_next << '\n'
            << "sparse_checksum=" << sparse_checksum << '\n';

  const std::size_t expected_sparse_allocations = 2 * kSparseIterations;
  return dense_allocations == 0 &&
                 sparse_resource.allocation_calls() ==
                     expected_sparse_allocations &&
                 sparse_resource.deallocation_calls() ==
                     expected_sparse_allocations
             ? 0
             : 10;
}
