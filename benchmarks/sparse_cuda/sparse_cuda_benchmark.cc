#include <cuda_runtime_api.h>
#include <cusparse.h>
#include <driver_types.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/sparse/blas.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/providers/cuda.h"

namespace {

constexpr std::size_t kWarmup = 3;
constexpr std::size_t kRepetitions = 20;
constexpr asc::extent_t kDimension = 1024;
constexpr asc::nnz_t kEntriesPerRow = 5;

class CountingResource final : public asc::MemoryResource {
 public:
  explicit CountingResource(asc::MemoryResource& upstream) noexcept
      : upstream_(upstream) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return upstream_.space();
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    allocated_bytes_ += bytes;
    auto result = upstream_.Allocate(bytes, alignment);
    if (result.ok() && *result != nullptr) {
      ++live_;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      ++deallocation_calls_;
      --live_;
    }
    upstream_.Deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] std::size_t allocation_calls() const noexcept {
    return allocation_calls_;
  }
  [[nodiscard]] std::size_t deallocation_calls() const noexcept {
    return deallocation_calls_;
  }
  [[nodiscard]] std::size_t allocated_bytes() const noexcept {
    return allocated_bytes_;
  }
  [[nodiscard]] std::size_t live() const noexcept { return live_; }

 private:
  asc::MemoryResource& upstream_;
  std::size_t allocation_calls_ = 0;
  std::size_t deallocation_calls_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t live_ = 0;
};

std::uint64_t Mix(std::uint64_t checksum, std::uint64_t value) {
  return (checksum ^ value) * 1099511628211ULL;
}

template <typename Element>
std::uint64_t Bits(Element value) {
  if constexpr (std::same_as<Element, float>) {
    return std::bit_cast<std::uint32_t>(value);
  } else {
    return std::bit_cast<std::uint64_t>(value);
  }
}

template <typename Element>
bool NearlyEqual(Element actual, Element expected) {
  const Element scale =
      std::max(Element{1}, std::max(std::abs(actual), std::abs(expected)));
  const Element tolerance =
      std::same_as<Element, float> ? Element{2.0e-5F} : Element{2.0e-12};
  return std::abs(actual - expected) <= tolerance * scale;
}

template <typename Element>
// Each precision runs one end-to-end benchmark and oracle with shared
// allocation accounting and provider state.
// NOLINTNEXTLINE(readability-function-size)
bool Run(asc::SparseCudaContext& context, asc::MemoryResource& raw_resource) {
  CountingResource resource(raw_resource);
  constexpr asc::nnz_t kNonzeros = kDimension * kEntriesPerRow;
  const std::array<asc::extent_t, 2> shape = {kDimension, kDimension};
  std::vector<asc::nnz_t> offsets(static_cast<std::size_t>(kDimension) + 1U);
  std::vector<asc::index_t> indices(static_cast<std::size_t>(kNonzeros));
  std::vector<Element> values(static_cast<std::size_t>(kNonzeros));
  std::vector<Element> input(static_cast<std::size_t>(kDimension));
  std::vector<Element> expected(static_cast<std::size_t>(kDimension));
  for (asc::extent_t row = 0; row < kDimension; ++row) {
    offsets[static_cast<std::size_t>(row)] = row * kEntriesPerRow;
    std::array<asc::index_t, kEntriesPerRow> row_columns{};
    for (asc::nnz_t entry = 0; entry < kEntriesPerRow; ++entry) {
      row_columns[static_cast<std::size_t>(entry)] =
          (row * 17 + entry * 193) % kDimension;
    }
    std::sort(row_columns.begin(), row_columns.end());
    for (asc::nnz_t entry = 0; entry < kEntriesPerRow; ++entry) {
      const std::size_t position =
          static_cast<std::size_t>(row * kEntriesPerRow + entry);
      indices[position] = row_columns[static_cast<std::size_t>(entry)];
      values[position] =
          static_cast<Element>((position % 23U) + 1U) / Element{32};
    }
  }
  offsets.back() = kNonzeros;
  for (asc::extent_t index = 0; index < kDimension; ++index) {
    input[static_cast<std::size_t>(index)] =
        static_cast<Element>((index % 29) - 14) / Element{16};
  }
  for (asc::extent_t row = 0; row < kDimension; ++row) {
    Element sum = 0;
    for (asc::nnz_t position = offsets[static_cast<std::size_t>(row)];
         position < offsets[static_cast<std::size_t>(row) + 1U]; ++position) {
      const std::size_t stored = static_cast<std::size_t>(position);
      sum += values[stored] * input[static_cast<std::size_t>(indices[stored])];
    }
    expected[static_cast<std::size_t>(row)] = sum;
  }

  auto host_matrix = asc::CsrView<const Element>::Create(
      offsets, indices, values, shape, asc::MemorySpace::kHost);
  if (!host_matrix.ok()) {
    return false;
  }
  auto clone = asc::CudaCloneCsr(context, *host_matrix, resource);
  if (!clone.ok() || !clone->completion.Wait().ok()) {
    return false;
  }
  auto matrix = clone->array.view();
  if (!matrix.ok()) {
    return false;
  }
  const asc::CsrView<const Element> const_matrix(*matrix);

  const std::size_t vector_bytes =
      static_cast<std::size_t>(kDimension) * sizeof(Element);
  auto device_input =
      asc::Buffer::Allocate(resource, vector_bytes, alignof(Element));
  auto device_output =
      asc::Buffer::Allocate(resource, vector_bytes, alignof(Element));
  if (!device_input.ok() || !device_output.ok() ||
      cudaMemcpy(device_input->data(), input.data(), vector_bytes,
                 cudaMemcpyHostToDevice) != cudaSuccess) {
    return false;
  }
  std::vector<Element> initial(static_cast<std::size_t>(kDimension),
                               std::numeric_limits<Element>::quiet_NaN());
  if (cudaMemcpy(device_output->data(), initial.data(), vector_bytes,
                 cudaMemcpyHostToDevice) != cudaSuccess) {
    return false;
  }
  auto input_view = asc::CudaStridedVectorView<const Element>::Create(
      static_cast<const Element*>(device_input->data()), kDimension, 1);
  auto output_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(device_output->data()), kDimension, 1);
  if (!input_view.ok() || !output_view.ok()) {
    return false;
  }
  auto required = asc::CudaCsrSpmvWorkspaceSize(context, const_matrix,
                                                *input_view, *output_view);
  if (!required.ok()) {
    return false;
  }
  asc::Result<asc::Buffer> workspace =
      asc::Buffer::Allocate(resource, *required, alignof(std::max_align_t));
  if (!workspace.ok()) {
    return false;
  }
  const asc::MutableMemoryView workspace_view(
      workspace->data(), workspace->size(), asc::MemorySpace::kDevice);
  const std::size_t setup_allocations = resource.allocation_calls();
  const std::size_t setup_bytes = resource.allocated_bytes();
  const auto operation = [&]() {
    auto event =
        asc::CudaCsrSpmv(context, Element{1}, const_matrix, *input_view,
                         Element{0}, *output_view, workspace_view);
    return event.ok() && event->Wait().ok();
  };
  for (std::size_t iteration = 0; iteration < kWarmup; ++iteration) {
    if (!operation()) {
      return false;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration < kRepetitions; ++iteration) {
    if (!operation()) {
      return false;
    }
  }
  const std::int64_t elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - begin)
          .count();
  if (resource.allocation_calls() != setup_allocations ||
      resource.allocated_bytes() != setup_bytes) {
    return false;
  }

  std::vector<Element> actual(static_cast<std::size_t>(kDimension));
  if (cudaMemcpy(actual.data(), device_output->data(), vector_bytes,
                 cudaMemcpyDeviceToHost) != cudaSuccess) {
    return false;
  }
  std::uint64_t checksum = 1469598103934665603ULL;
  for (std::size_t row = 0; row < actual.size(); ++row) {
    if (!NearlyEqual(actual[row], expected[row])) {
      return false;
    }
    checksum = Mix(checksum, Bits(actual[row]));
  }
  const long double seconds = static_cast<long double>(elapsed) / 1.0e9L;
  const long double nonzeros_per_second =
      static_cast<long double>(kNonzeros) * kRepetitions / seconds;
  std::cout << "operation=csr_spmv scalar="
            << (std::same_as<Element, float> ? "float" : "double")
            << " shape=1024x1024 nnz=" << kNonzeros
            << " algorithm=CUSPARSE_SPMV_CSR_ALG2 warmups=" << kWarmup
            << " repetitions=" << kRepetitions << " elapsed_ns=" << elapsed
            << " nonzeros_per_second="
            << static_cast<double>(nonzeros_per_second)
            << " workspace_bytes=" << *required
            << " operation_allocation_calls=0"
            << " setup_asc_resource_allocation_calls=" << setup_allocations
            << " setup_asc_resource_allocated_bytes=" << setup_bytes
            << " checksum=" << checksum << '\n';

  std::fill(initial.begin(), initial.end(), Element{});
  if (cudaMemcpy(device_output->data(), initial.data(), vector_bytes,
                 cudaMemcpyHostToDevice) != cudaSuccess) {
    return false;
  }
  auto standard_input = asc::SparseBlasVectorView<const Element>::Create(
      static_cast<const Element*>(device_input->data()), kDimension, 1,
      {device_input->data(), device_input->size(), asc::MemorySpace::kDevice});
  auto standard_output = asc::SparseBlasVectorView<Element>::Create(
      static_cast<Element*>(device_output->data()), kDimension, 1,
      {device_output->data(), device_output->size(),
       asc::MemorySpace::kDevice});
  if (!standard_input.ok() || !standard_output.ok()) {
    return false;
  }
  const auto standard_operation = [&]() {
    auto event =
        asc::CudaSpmv(context, asc::SparseBlasTranspose::kNone, Element{1},
                      const_matrix, *standard_input, *standard_output);
    return event.ok() && event->Wait().ok();
  };
  for (std::size_t iteration = 0; iteration < kWarmup; ++iteration) {
    if (!standard_operation()) {
      return false;
    }
  }
  const auto standard_begin = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration < kRepetitions; ++iteration) {
    if (!standard_operation()) {
      return false;
    }
  }
  const std::int64_t standard_elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - standard_begin)
          .count();
  if (resource.allocation_calls() != setup_allocations ||
      resource.allocated_bytes() != setup_bytes) {
    return false;
  }
  if (cudaMemcpy(actual.data(), device_output->data(), vector_bytes,
                 cudaMemcpyDeviceToHost) != cudaSuccess) {
    return false;
  }
  std::uint64_t standard_checksum = 1469598103934665603ULL;
  const Element factor = static_cast<Element>(kWarmup + kRepetitions);
  for (std::size_t row = 0; row < actual.size(); ++row) {
    if (!NearlyEqual(actual[row], factor * expected[row])) {
      return false;
    }
    standard_checksum = Mix(standard_checksum, Bits(actual[row]));
  }
  const long double standard_seconds =
      static_cast<long double>(standard_elapsed) / 1.0e9L;
  std::cout << "operation=standard_sparse_blas_spmv scalar="
            << (std::same_as<Element, float> ? "float" : "double")
            << " shape=1024x1024 nnz=" << kNonzeros
            << " algorithm=asc_project_csr warmups=" << kWarmup
            << " repetitions=" << kRepetitions
            << " elapsed_ns=" << standard_elapsed << " nonzeros_per_second="
            << static_cast<double>(static_cast<long double>(kNonzeros) *
                                   kRepetitions / standard_seconds)
            << " workspace_bytes=0 operation_allocation_calls=0"
            << " checksum=" << standard_checksum << '\n';
  return true;
}

}  // namespace

int main() {
  int runtime_version = 0;
  int driver_version = 0;
  cudaDeviceProp properties{};
  if (cudaRuntimeGetVersion(&runtime_version) != cudaSuccess ||
      cudaDriverGetVersion(&driver_version) != cudaSuccess ||
      cudaGetDeviceProperties(&properties, 0) != cudaSuccess) {
    return 77;
  }
  std::cout << "gpu=\"" << properties.name
            << "\" compute_capability=" << properties.major << '.'
            << properties.minor << " runtime=" << runtime_version
            << " driver=" << driver_version
            << " cusparse_headers=" << CUSPARSE_VERSION
#ifdef NDEBUG
            << " configuration=release"
#else
            << " configuration=debug"
#endif
#if defined(__clang_version__)
            << " compiler=\"Clang " << __clang_version__ << "\""
#elif defined(__VERSION__)
            << " compiler=\"" << __VERSION__ << "\""
#endif
            << '\n';

  auto execution =
      asc::CreateCudaExecutionContext(0, asc::Determinism::kDeterministic);
  auto resource = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  if (!execution.ok() || !resource.ok()) {
    return 1;
  }
  auto context = asc::SparseCudaContext::Create(*execution);
  if (!context.ok()) {
    return 2;
  }
  return Run<float>(*context, **resource) && Run<double>(*context, **resource)
             ? 0
             : 3;
}
