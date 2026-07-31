#include <cublas_v2.h>
#include <cuda_runtime_api.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/layout.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"

namespace {

class CountingMemoryResource final : public asc::MemoryResource {
 public:
  explicit CountingMemoryResource(asc::MemoryResource& upstream) noexcept
      : upstream_(upstream) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return upstream_.space();
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_calls_;
    return upstream_.Allocate(bytes, alignment);
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    upstream_.Deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] std::size_t allocation_calls() const noexcept {
    return allocation_calls_;
  }

 private:
  asc::MemoryResource& upstream_;
  std::size_t allocation_calls_ = 0;
};

template <typename Element, std::size_t Rank>
asc::DenseView<Element, Rank> LeftView(
    Element* data, const std::array<asc::extent_t, Rank>& extents) {
  auto mapping = asc::DenseLayout<Rank>::Create(
      std::span<const asc::extent_t, Rank>(extents), asc::LayoutLeft{});
  if (!mapping.ok()) {
    std::abort();
  }
  auto view = asc::DenseView<Element, Rank>::Create(data, *mapping,
                                                    asc::MemorySpace::kDevice);
  if (!view.ok()) {
    std::abort();
  }
  return *view;
}

bool Wait(asc::Result<asc::CompletionEvent> event) {
  return event.ok() && event->Wait().ok();
}

int DeviceCountDisposition(const asc::Result<std::int32_t>& count) {
  if (!count.ok()) {
    std::cerr << "CUDA device enumeration failed: " << count.status().message()
              << '\n';
    return 2;
  }
  return *count == 0 ? 77 : 0;
}

bool ExactOracle(const float* actual, const float* expected, std::size_t count,
                 long double* checksum) {
  *checksum = 0.0L;
  for (std::size_t index = 0; index < count; ++index) {
    if (actual[index] != expected[index]) {
      return false;
    }
    *checksum += actual[index];
  }
  return true;
}

bool ScaledOracle(const float* actual, const float* source, std::size_t count,
                  long double scale, long double* checksum) {
  *checksum = 0.0L;
  for (std::size_t index = 0; index < count; ++index) {
    const long double expected =
        scale * static_cast<long double>(source[index]);
    const long double difference =
        std::abs(static_cast<long double>(actual[index]) - expected);
    const long double tolerance = 1.0e-6L * std::max(1.0L, std::abs(expected));
    if (difference > tolerance) {
      return false;
    }
    *checksum += actual[index];
  }
  return true;
}

bool GemvOracle(const float* actual, const float* matrix, const float* input,
                std::size_t dimension, long double* checksum) {
  *checksum = 0.0L;
  for (std::size_t row = 0; row < dimension; ++row) {
    long double expected = 0.0L;
    for (std::size_t column = 0; column < dimension; ++column) {
      expected += static_cast<long double>(matrix[row + column * dimension]) *
                  static_cast<long double>(input[column]);
    }
    const long double difference =
        std::abs(static_cast<long double>(actual[row]) - expected);
    const long double tolerance = 2.0e-5L * std::max(1.0L, std::abs(expected));
    if (difference > tolerance) {
      return false;
    }
    *checksum += actual[row];
  }
  return true;
}

bool GemmSampleOracle(const float* actual, const float* left,
                      const float* right, std::size_t dimension,
                      long double* checksum) {
  *checksum = 0.0L;
  const std::size_t element_count = dimension * dimension;
  for (std::size_t position = 0; position < element_count; position += 4093) {
    const std::size_t row = position % dimension;
    const std::size_t column = position / dimension;
    long double expected = 0.0L;
    for (std::size_t inner = 0; inner < dimension; ++inner) {
      expected += static_cast<long double>(left[row + inner * dimension]) *
                  static_cast<long double>(right[inner + column * dimension]);
    }
    const long double difference =
        std::abs(static_cast<long double>(actual[position]) - expected);
    const long double tolerance = 3.0e-5L * std::max(1.0L, std::abs(expected));
    if (difference > tolerance) {
      return false;
    }
    *checksum += actual[position];
  }
  return true;
}

template <typename Operation>
std::int64_t MeasureNanoseconds(std::size_t warmup, std::size_t repetitions,
                                Operation&& operation) {
  for (std::size_t iteration = 0; iteration < warmup; ++iteration) {
    if (!operation()) {
      return -1;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration < repetitions; ++iteration) {
    if (!operation()) {
      return -1;
    }
  }
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin)
      .count();
}

void Report(std::string_view name, std::int64_t elapsed_ns,
            std::size_t repetitions, long double work_per_repetition,
            std::string_view unit, long double checksum,
            std::size_t asc_resource_allocations) {
  const long double seconds = static_cast<long double>(elapsed_ns) / 1.0e9L;
  const long double throughput =
      seconds > 0 ? work_per_repetition * repetitions / seconds : 0;
  std::cout << name << " elapsed_ns=" << elapsed_ns
            << " repetitions=" << repetitions
            << " throughput=" << static_cast<double>(throughput) << ' ' << unit
            << " checksum=" << static_cast<double>(checksum)
            << " asc_resource_allocation_calls=" << asc_resource_allocations
            << " oracle=independent\n";
}

}  // namespace

int main() {
  constexpr std::size_t kWarmup = 3;
  constexpr std::size_t kRepetitions = 12;
  constexpr std::size_t kVectorSize = 1U << 20;
  constexpr std::size_t kGemvDimension = 1024;
  constexpr std::size_t kGemmDimension = 512;
  constexpr std::size_t kVectorBytes = kVectorSize * sizeof(float);
  constexpr std::size_t kMatrixElements = kGemvDimension * kGemvDimension;
  constexpr std::size_t kMatrixBytes = kMatrixElements * sizeof(float);

  const char* force_no_device =
      std::getenv("ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE");
  if (force_no_device != nullptr && std::string_view(force_no_device) == "1") {
    return 77;
  }
  const char* force_enumeration_failure =
      std::getenv("ASC_CPP_TEST_FORCE_CUDA_ENUMERATION_FAILURE");
  asc::Result<std::int32_t> count =
      force_enumeration_failure != nullptr &&
              std::string_view(force_enumeration_failure) == "1"
          ? asc::Result<std::int32_t>(
                asc::Status(asc::ErrorCode::kProvider,
                            "forced CUDA enumeration failure", "cuda"))
          : asc::CudaDeviceCount();
  const int device_count_disposition = DeviceCountDisposition(count);
  if (device_count_disposition != 0) {
    return device_count_disposition;
  }

  int runtime_version = 0;
  int driver_version = 0;
  cudaDeviceProp properties{};
  if (cudaRuntimeGetVersion(&runtime_version) != cudaSuccess ||
      cudaDriverGetVersion(&driver_version) != cudaSuccess ||
      cudaGetDeviceProperties(&properties, 0) != cudaSuccess) {
    return 1;
  }
  std::cout << "gpu=\"" << properties.name << "\""
            << " compute_capability=" << properties.major << '.'
            << properties.minor << " runtime=" << runtime_version
            << " driver=" << driver_version
            << " cublas_headers=" << CUBLAS_VER_MAJOR << '.' << CUBLAS_VER_MINOR
            << '.' << CUBLAS_VER_PATCH
#ifdef NDEBUG
            << " configuration=release"
#else
            << " configuration=debug"
#endif
            << '\n';

  auto pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto execution =
      asc::CreateCudaExecutionContext(0, asc::Determinism::kDeterministic);
  if (!pinned.ok() || !device.ok() || !execution.ok()) {
    return 3;
  }
  CountingMemoryResource counted_pinned(**pinned);
  CountingMemoryResource counted_device(**device);
  auto dense_context = asc::DenseCudaContext::Create(*execution);
  if (!dense_context.ok()) {
    return 4;
  }

  auto host_source =
      asc::Buffer::Allocate(counted_pinned, kMatrixBytes, alignof(float));
  auto host_destination =
      asc::Buffer::Allocate(counted_pinned, kMatrixBytes, alignof(float));
  auto device_left =
      asc::Buffer::Allocate(counted_device, kMatrixBytes, alignof(float));
  auto device_right =
      asc::Buffer::Allocate(counted_device, kMatrixBytes, alignof(float));
  auto device_output =
      asc::Buffer::Allocate(counted_device, kMatrixBytes, alignof(float));
  auto host_scalar =
      asc::Buffer::Allocate(counted_pinned, sizeof(float), alignof(float));
  auto device_scalar =
      asc::Buffer::Allocate(counted_device, sizeof(float), alignof(float));
  auto host_index = asc::Buffer::Allocate(
      counted_pinned, 2 * sizeof(asc::index_t), alignof(asc::index_t));
  auto device_index = asc::Buffer::Allocate(
      counted_device, 2 * sizeof(asc::index_t), alignof(asc::index_t));
  if (!host_source.ok() || !host_destination.ok() || !device_left.ok() ||
      !device_right.ok() || !device_output.ok() || !host_scalar.ok() ||
      !device_scalar.ok() || !host_index.ok() || !device_index.ok()) {
    return 5;
  }

  auto* host_values = static_cast<float*>(host_source->data());
  for (std::size_t index = 0; index < kMatrixElements; ++index) {
    host_values[index] =
        static_cast<float>(static_cast<std::int32_t>(index % 17) - 8) / 16.0F;
  }
  auto host_source_view = host_source->const_view();
  auto host_destination_view = host_destination->mutable_view();
  auto left_mutable_memory = device_left->mutable_view();
  auto left_const_memory = device_left->const_view();
  auto right_mutable_memory = device_right->mutable_view();
  auto right_const_memory = device_right->const_view();
  auto output_mutable_memory = device_output->mutable_view();
  auto output_const_memory = device_output->const_view();
  auto host_scalar_memory = host_scalar->mutable_view();
  auto device_scalar_memory = device_scalar->const_view();
  auto host_index_memory = host_index->mutable_view();
  auto device_index_memory = device_index->const_view();
  if (!host_source_view.ok() || !host_destination_view.ok() ||
      !left_mutable_memory.ok() || !left_const_memory.ok() ||
      !right_mutable_memory.ok() || !right_const_memory.ok() ||
      !output_mutable_memory.ok() || !output_const_memory.ok() ||
      !host_scalar_memory.ok() || !device_scalar_memory.ok() ||
      !host_index_memory.ok() || !device_index_memory.ok()) {
    return 6;
  }
  const auto* observed_values =
      static_cast<const float*>(host_destination->data());
  if (!Wait(asc::CopyBytes(*execution, *left_mutable_memory,
                           *host_source_view)) ||
      !Wait(asc::CopyBytes(*execution, *right_mutable_memory,
                           *host_source_view))) {
    return 7;
  }

  const auto allocation_calls = [&] {
    return counted_pinned.allocation_calls() +
           counted_device.allocation_calls();
  };
  std::size_t allocation_checkpoint = allocation_calls();
  const auto h2d_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(
        asc::CopyBytes(*execution, *left_mutable_memory, *host_source_view));
  });
  const std::size_t h2d_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_destination_view,
                           *left_const_memory))) {
    return 11;
  }
  long double h2d_checksum = 0.0L;
  if (!ExactOracle(observed_values, host_values, kMatrixElements,
                   &h2d_checksum)) {
    return 12;
  }

  allocation_checkpoint = allocation_calls();
  const auto d2h_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(
        asc::CopyBytes(*execution, *host_destination_view, *left_const_memory));
  });
  const std::size_t d2h_allocations =
      allocation_calls() - allocation_checkpoint;
  long double d2h_checksum = 0.0L;
  if (!ExactOracle(observed_values, host_values, kMatrixElements,
                   &d2h_checksum)) {
    return 13;
  }

  allocation_checkpoint = allocation_calls();
  const auto d2d_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(
        asc::CopyBytes(*execution, *right_mutable_memory, *left_const_memory));
  });
  const std::size_t d2d_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_destination_view,
                           *right_const_memory))) {
    return 14;
  }
  long double d2d_checksum = 0.0L;
  if (!ExactOracle(observed_values, host_values, kMatrixElements,
                   &d2d_checksum)) {
    return 15;
  }

  auto vector_left_mutable =
      LeftView(static_cast<float*>(device_left->data()),
               std::array<asc::extent_t, 1>{kVectorSize});
  auto vector_output = LeftView(static_cast<float*>(device_output->data()),
                                std::array<asc::extent_t, 1>{kVectorSize});
  asc::DenseView<const float, 1> vector_left = vector_left_mutable;
  auto blas_left = asc::DenseBlasVectorView<const float>::Create(
      static_cast<const float*>(device_left->data()), kVectorSize, 1,
      *left_const_memory);
  auto blas_output = asc::DenseBlasVectorView<float>::Create(
      static_cast<float*>(device_output->data()), kVectorSize, 1,
      *output_const_memory);
  auto blas_scalar = asc::DenseBlasVectorView<float>::Create(
      static_cast<float*>(device_scalar->data()), 1, 1, *device_scalar_memory);
  auto blas_index = asc::DenseBlasVectorView<asc::index_t>::Create(
      static_cast<asc::index_t*>(device_index->data()), 1, 1,
      *device_index_memory);
  if (!blas_left.ok() || !blas_output.ok() || !blas_scalar.ok() ||
      !blas_index.ok()) {
    return 24;
  }
  asc::MutableMemoryView iamax_workspace(
      static_cast<asc::index_t*>(device_index->data()) + 1,
      sizeof(asc::index_t), asc::MemorySpace::kDevice);
  allocation_checkpoint = allocation_calls();
  const auto evaluate_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(asc::CudaEvaluate(*dense_context, vector_left, vector_output));
  });
  const std::size_t evaluate_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_destination_view,
                           *output_const_memory))) {
    return 16;
  }
  long double evaluate_checksum = 0.0L;
  if (!ExactOracle(observed_values, host_values, kVectorSize,
                   &evaluate_checksum)) {
    return 17;
  }

  allocation_checkpoint = allocation_calls();
  const auto axpy_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(asc::CudaAxpy(*dense_context, 0.25F, *blas_left, *blas_output));
  });
  const std::size_t axpy_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_destination_view,
                           *output_const_memory))) {
    return 18;
  }
  constexpr long double kAxpyResultScale =
      1.0L + 0.25L * static_cast<long double>(kWarmup + kRepetitions);
  long double axpy_checksum = 0.0L;
  if (!ScaledOracle(observed_values, host_values, kVectorSize, kAxpyResultScale,
                    &axpy_checksum)) {
    return 19;
  }

  allocation_checkpoint = allocation_calls();
  const auto dot_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(asc::CudaDot(
        *dense_context, *blas_left,
        asc::DenseBlasVectorView<const float>(*blas_output), *blas_scalar));
  });
  const std::size_t dot_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_scalar_memory,
                           *device_scalar_memory))) {
    return 25;
  }
  long double expected_dot = 0.0L;
  for (std::size_t index = 0; index < kVectorSize; ++index) {
    const long double value = host_values[index];
    expected_dot += kAxpyResultScale * value * value;
  }
  const float observed_dot = *static_cast<const float*>(host_scalar->data());
  const long double dot_difference =
      std::abs(static_cast<long double>(observed_dot) - expected_dot);
  if (dot_difference > 1.0e-4L * std::max(1.0L, std::abs(expected_dot))) {
    return 26;
  }

  allocation_checkpoint = allocation_calls();
  const auto iamax_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(asc::CudaIamax(*dense_context, *blas_left, *blas_index,
                               iamax_workspace));
  });
  const std::size_t iamax_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_index_memory,
                           *device_index_memory))) {
    return 27;
  }
  const asc::index_t observed_index =
      *static_cast<const asc::index_t*>(host_index->data());
  if (observed_index != 0) {
    return 28;
  }

  auto gemv_matrix_mutable =
      LeftView(static_cast<float*>(device_left->data()),
               std::array<asc::extent_t, 2>{kGemvDimension, kGemvDimension});
  auto gemv_input_mutable =
      LeftView(static_cast<float*>(device_right->data()),
               std::array<asc::extent_t, 1>{kGemvDimension});
  auto gemv_output = LeftView(static_cast<float*>(device_output->data()),
                              std::array<asc::extent_t, 1>{kGemvDimension});
  asc::DenseView<const float, 2> gemv_matrix = gemv_matrix_mutable;
  asc::DenseView<const float, 1> gemv_input = gemv_input_mutable;
  allocation_checkpoint = allocation_calls();
  const auto gemv_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(asc::CudaGemv(*dense_context, asc::MatrixOperation::kNone, 1.0F,
                              gemv_matrix, gemv_input, 0.0F, gemv_output));
  });
  const std::size_t gemv_allocations =
      allocation_calls() - allocation_checkpoint;
  if (!Wait(asc::CopyBytes(*execution, *host_destination_view,
                           *output_const_memory))) {
    return 20;
  }
  long double gemv_checksum = 0.0L;
  if (!GemvOracle(observed_values, host_values, host_values, kGemvDimension,
                  &gemv_checksum)) {
    return 21;
  }

  auto gemm_left_mutable =
      LeftView(static_cast<float*>(device_left->data()),
               std::array<asc::extent_t, 2>{kGemmDimension, kGemmDimension});
  auto gemm_right_mutable =
      LeftView(static_cast<float*>(device_right->data()),
               std::array<asc::extent_t, 2>{kGemmDimension, kGemmDimension});
  auto gemm_output =
      LeftView(static_cast<float*>(device_output->data()),
               std::array<asc::extent_t, 2>{kGemmDimension, kGemmDimension});
  asc::DenseView<const float, 2> gemm_left = gemm_left_mutable;
  asc::DenseView<const float, 2> gemm_right = gemm_right_mutable;
  allocation_checkpoint = allocation_calls();
  const auto gemm_ns = MeasureNanoseconds(kWarmup, kRepetitions, [&] {
    return Wait(asc::CudaGemm(*dense_context, asc::MatrixOperation::kNone,
                              asc::MatrixOperation::kNone, 1.0F, gemm_left,
                              gemm_right, 0.0F, gemm_output));
  });
  const std::size_t gemm_allocations =
      allocation_calls() - allocation_checkpoint;
  if (h2d_ns < 0 || d2h_ns < 0 || d2d_ns < 0 || evaluate_ns < 0 ||
      axpy_ns < 0 || dot_ns < 0 || iamax_ns < 0 || gemv_ns < 0 || gemm_ns < 0) {
    return 8;
  }

  if (!Wait(asc::CopyBytes(*execution, *host_destination_view,
                           *output_const_memory))) {
    return 22;
  }
  long double gemm_checksum = 0.0L;
  if (!GemmSampleOracle(observed_values, host_values, host_values,
                        kGemmDimension, &gemm_checksum)) {
    return 23;
  }

  std::cout << std::setprecision(10);
  Report("h2d", h2d_ns, kRepetitions, kMatrixBytes, "bytes/s", h2d_checksum,
         h2d_allocations);
  Report("d2h", d2h_ns, kRepetitions, kMatrixBytes, "bytes/s", d2h_checksum,
         d2h_allocations);
  Report("d2d", d2d_ns, kRepetitions, kMatrixBytes, "bytes/s", d2d_checksum,
         d2d_allocations);
  Report("terminal_evaluate", evaluate_ns, kRepetitions, kVectorBytes,
         "bytes/s", evaluate_checksum, evaluate_allocations);
  Report("blas_level1_axpy", axpy_ns, kRepetitions,
         static_cast<long double>(2) * kVectorSize, "flop/s", axpy_checksum,
         axpy_allocations);
  Report("blas_level1_dot", dot_ns, kRepetitions,
         static_cast<long double>(2) * kVectorSize, "flop/s", observed_dot,
         dot_allocations);
  Report("blas_level1_iamax", iamax_ns, kRepetitions, kVectorSize, "items/s",
         observed_index, iamax_allocations);
  Report("gemv", gemv_ns, kRepetitions,
         static_cast<long double>(2) * kGemvDimension * kGemvDimension,
         "flop/s", gemv_checksum, gemv_allocations);
  Report("gemm", gemm_ns, kRepetitions,
         static_cast<long double>(2) * kGemmDimension * kGemmDimension *
             kGemmDimension,
         "flop/s", gemm_checksum, gemm_allocations);
  return 0;
}
