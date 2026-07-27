#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/layout.h"
#include "asc/dense/linalg.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"

namespace {

using Clock = std::chrono::steady_clock;

template <typename Function>
asc::Result<std::int64_t> TimeOperations(int warmups, int repetitions,
                                         Function operation) {
  for (int iteration = 0; iteration < warmups; ++iteration) {
    auto event = operation();
    if (!event.ok()) {
      return event.status();
    }
    const asc::Status status = event->Wait();
    if (!status.ok()) {
      return status;
    }
  }

  std::vector<asc::CompletionEvent> events;
  events.reserve(static_cast<std::size_t>(repetitions));
  const auto start = Clock::now();
  for (int iteration = 0; iteration < repetitions; ++iteration) {
    auto event = operation();
    if (!event.ok()) {
      return event.status();
    }
    events.push_back(std::move(*event));
  }
  if (!events.empty()) {
    const asc::Status status = events.back().Wait();
    if (!status.ok()) {
      return status;
    }
  }
  const auto finish = Clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(finish - start)
      .count();
}

asc::Status CopyAndWait(const asc::ExecutionContext& context,
                        asc::MutableMemoryView destination,
                        asc::ConstMemoryView source) {
  auto event = asc::CopyBytes(context, destination, source, source.size());
  if (!event.ok()) {
    return event.status();
  }
  return event->Wait();
}

void PrintCompiler() {
#if defined(__clang__)
  std::cout << "compiler=Clang " << __clang_major__ << '.' << __clang_minor__
            << '.' << __clang_patchlevel__ << '\n';
#elif defined(__GNUC__)
  std::cout << "compiler=GCC " << __GNUC__ << '.' << __GNUC_MINOR__ << '.'
            << __GNUC_PATCHLEVEL__ << '\n';
#elif defined(_MSC_VER)
  std::cout << "compiler=MSVC " << _MSC_VER << '\n';
#else
  std::cout << "compiler=unknown\n";
#endif
#if defined(NDEBUG)
  std::cout << "configuration=Release-like\n";
#else
  std::cout << "configuration=Debug-like\n";
#endif
}

template <typename Scalar>
int RunBenchmark() {
  const asc::Device device{asc::Backend::kCuda, 0};
  auto execution =
      asc::CreateCudaExecutionContext(device, asc::Determinism::kDeterministic);
  if (!execution.ok()) {
    std::cerr << execution.status().ToString() << '\n';
    return 1;
  }
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!provider.ok() || !resource.ok()) {
    std::cerr << (provider.ok() ? resource.status().ToString()
                                : provider.status().ToString())
              << '\n';
    return 2;
  }

  constexpr std::size_t kVectorSize = 1U << 20;
  constexpr std::size_t kMatrixRows = 512;
  constexpr std::size_t kMatrixColumns = 512;
  constexpr std::size_t kGemmSize = 256;
  constexpr int kWarmups = 3;
  constexpr int kVectorRepetitions = 80;
  constexpr int kGemvRepetitions = 80;
  constexpr int kGemmRepetitions = 40;
  constexpr std::size_t kBytes = kVectorSize * sizeof(Scalar);

  std::vector<Scalar> host_x(kVectorSize);
  std::vector<Scalar> host_y(kVectorSize);
  std::vector<Scalar> host_z(kVectorSize);
  for (std::size_t index = 0; index < kVectorSize; ++index) {
    host_x[index] =
        static_cast<Scalar>(static_cast<int>(index % 97U) - 48) / Scalar{32};
    host_y[index] =
        static_cast<Scalar>(static_cast<int>(index % 89U) - 44) / Scalar{64};
  }

  auto x = asc::Buffer::Allocate(**resource, kBytes, alignof(Scalar));
  auto y = asc::Buffer::Allocate(**resource, kBytes, alignof(Scalar));
  auto z = asc::Buffer::Allocate(**resource, kBytes, alignof(Scalar));
  if (!x.ok() || !y.ok() || !z.ok()) {
    return 3;
  }

  const auto transfer_start = Clock::now();
  asc::Status status = CopyAndWait(
      *execution, *x->mutable_view(),
      asc::ConstMemoryView(host_x.data(), kBytes, asc::MemorySpace::kHost));
  if (status.ok()) {
    status = CopyAndWait(
        *execution, *y->mutable_view(),
        asc::ConstMemoryView(host_y.data(), kBytes, asc::MemorySpace::kHost));
  }
  const auto transfer_finish = Clock::now();
  if (!status.ok()) {
    std::cerr << status.ToString() << '\n';
    return 4;
  }
  const auto host_to_device_us =
      std::chrono::duration_cast<std::chrono::microseconds>(transfer_finish -
                                                            transfer_start)
          .count();

  const std::array<asc::extent_t, 1> vector_shape = {
      static_cast<asc::extent_t>(kVectorSize)};
  const auto vector_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, vector_shape);
  const auto x_view = asc::DenseView<const Scalar, 1>::Create(
      static_cast<const Scalar*>(x->data()), *vector_mapping,
      asc::MemorySpace::kDevice);
  const auto y_const_view = asc::DenseView<const Scalar, 1>::Create(
      static_cast<const Scalar*>(y->data()), *vector_mapping,
      asc::MemorySpace::kDevice);
  const auto y_view = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(y->data()), *vector_mapping,
      asc::MemorySpace::kDevice);
  const auto z_view = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(z->data()), *vector_mapping,
      asc::MemorySpace::kDevice);
  auto add = asc::MakeAdd(*x_view, *y_const_view);
  auto add_us = TimeOperations(kWarmups, kVectorRepetitions, [&] {
    return asc::CudaEvaluate(*provider, *z_view, *add);
  });
  auto axpy_us = TimeOperations(kWarmups, kVectorRepetitions, [&] {
    return asc::CudaAxpy(*provider, Scalar{0.0001}, *x_view, *y_view);
  });
  if (!add_us.ok() || !axpy_us.ok()) {
    std::cerr << (add_us.ok() ? axpy_us.status().ToString()
                              : add_us.status().ToString())
              << '\n';
    return 5;
  }

  const std::array<asc::extent_t, 2> matrix_shape = {
      static_cast<asc::extent_t>(kMatrixRows),
      static_cast<asc::extent_t>(kMatrixColumns)};
  const auto matrix_mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, matrix_shape);
  const auto matrix_view = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(x->data()), *matrix_mapping,
      asc::MemorySpace::kDevice);
  const std::array<asc::extent_t, 1> gemv_output_shape = {
      static_cast<asc::extent_t>(kMatrixRows)};
  const auto gemv_output_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, gemv_output_shape);
  const std::array<asc::extent_t, 1> gemv_input_shape = {
      static_cast<asc::extent_t>(kMatrixColumns)};
  const auto gemv_input_mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, gemv_input_shape);
  const auto gemv_input = asc::DenseView<const Scalar, 1>::Create(
      static_cast<const Scalar*>(x->data()), *gemv_input_mapping,
      asc::MemorySpace::kDevice);
  const auto gemv_output = asc::DenseView<Scalar, 1>::Create(
      static_cast<Scalar*>(z->data()), *gemv_output_mapping,
      asc::MemorySpace::kDevice);
  auto gemv_us = TimeOperations(kWarmups, kGemvRepetitions, [&] {
    return asc::CudaGemv(*provider, asc::MatrixOperation::kNone, Scalar{1},
                         *matrix_view, *gemv_input, Scalar{0}, *gemv_output);
  });
  if (!gemv_us.ok()) {
    std::cerr << gemv_us.status().ToString() << '\n';
    return 6;
  }

  const std::array<asc::extent_t, 2> gemm_shape = {
      static_cast<asc::extent_t>(kGemmSize),
      static_cast<asc::extent_t>(kGemmSize)};
  const auto gemm_mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutLeft{}, gemm_shape);
  const auto gemm_left = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(x->data()), *gemm_mapping,
      asc::MemorySpace::kDevice);
  const auto gemm_right = asc::DenseView<const Scalar, 2>::Create(
      static_cast<const Scalar*>(y->data()), *gemm_mapping,
      asc::MemorySpace::kDevice);
  const auto gemm_output = asc::DenseView<Scalar, 2>::Create(
      static_cast<Scalar*>(z->data()), *gemm_mapping,
      asc::MemorySpace::kDevice);
  auto gemm_us = TimeOperations(kWarmups, kGemmRepetitions, [&] {
    return asc::CudaGemm(*provider, asc::MatrixOperation::kNone,
                         asc::MatrixOperation::kNone, Scalar{1}, *gemm_left,
                         *gemm_right, Scalar{0}, *gemm_output);
  });
  if (!gemm_us.ok()) {
    std::cerr << gemm_us.status().ToString() << '\n';
    return 7;
  }

  const auto result_transfer_start = Clock::now();
  status = CopyAndWait(
      *execution,
      asc::MutableMemoryView(host_z.data(), kBytes, asc::MemorySpace::kHost),
      *z->const_view());
  const auto result_transfer_finish = Clock::now();
  if (!status.ok()) {
    std::cerr << status.ToString() << '\n';
    return 8;
  }
  const auto device_to_host_us =
      std::chrono::duration_cast<std::chrono::microseconds>(
          result_transfer_finish - result_transfer_start)
          .count();
  long double checksum = 0.0L;
  for (Scalar value : host_z) {
    checksum += static_cast<long double>(value);
  }

  std::cout << "scalar=" << (std::is_same_v<Scalar, float> ? "float" : "double")
            << '\n';
  std::cout << "provider=CUDA Runtime plus cuBLAS\n";
  std::cout << "device_ordinal=0\n";
  std::cout << "benchmark_classification=smoke-only\n";
  std::cout << "performance_claim=none\n";
  std::cout << "environment_metadata=reported-by-validation-harness\n";
  std::cout << "warmups=" << kWarmups << '\n';
  std::cout << "vector_size=" << kVectorSize << '\n';
  std::cout << "vector_repetitions=" << kVectorRepetitions << '\n';
  std::cout << "host_to_device_us=" << host_to_device_us << '\n';
  std::cout << "pointwise_add_us=" << *add_us << '\n';
  std::cout << "axpy_us=" << *axpy_us << '\n';
  std::cout << "gemv_shape=" << kMatrixRows << 'x' << kMatrixColumns << '\n';
  std::cout << "gemv_repetitions=" << kGemvRepetitions << '\n';
  std::cout << "gemv_us=" << *gemv_us << '\n';
  std::cout << "gemm_shape=" << kGemmSize << 'x' << kGemmSize << '\n';
  std::cout << "gemm_repetitions=" << kGemmRepetitions << '\n';
  std::cout << "gemm_us=" << *gemm_us << '\n';
  std::cout << "device_to_host_us=" << device_to_host_us << '\n';
  std::cout << "checksum=" << static_cast<double>(checksum) << '\n';
  return std::isfinite(checksum) ? 0 : 9;
}

}  // namespace

int main() {
  PrintCompiler();
  const int float_result = RunBenchmark<float>();
  if (float_result != 0) {
    return float_result;
  }
  return RunBenchmark<double>();
}
