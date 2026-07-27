#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "../../tests/random_cuda/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/providers/cuda.h"

namespace {

constexpr asc::extent_t kRows = 1024;
constexpr asc::extent_t kColumns = 1024;
constexpr asc::nnz_t kEntriesPerRow = 5;
constexpr int kWarmups = 3;
constexpr int kRepetitions = 20;

template <typename Element>
int BenchmarkSpmv(asc::ExecutionContext execution,
                  asc::SparseCudaContext& provider,
                  asc::CudaMemoryResource& resource,
                  std::string_view scalar_name) {
  std::vector<asc::nnz_t> offsets(static_cast<std::size_t>(kRows + 1));
  std::vector<asc::index_t> indices(
      static_cast<std::size_t>(kRows * kEntriesPerRow));
  std::vector<Element> values(indices.size(), Element{1});
  for (asc::index_t row = 0; row < kRows; ++row) {
    offsets[static_cast<std::size_t>(row)] = row * kEntriesPerRow;
    for (asc::index_t entry = 0; entry < kEntriesPerRow; ++entry) {
      indices[static_cast<std::size_t>(row * kEntriesPerRow + entry)] = entry;
    }
  }
  offsets.back() = static_cast<asc::nnz_t>(indices.size());
  const std::array<asc::extent_t, 2> shape = {kRows, kColumns};
  auto host_matrix = asc::CsrView<const Element>::Create(
      shape, offsets, indices, values, asc::MemorySpace::kHost);
  if (!host_matrix.ok()) {
    return 1;
  }
  auto clone = asc::CudaCloneCsr(provider, *host_matrix, resource);
  if (!clone.ok() || !clone->completion.Wait().ok()) {
    return 2;
  }
  auto matrix = clone->array.view();
  if (!matrix.ok()) {
    return 3;
  }
  const asc::CsrView<const Element> const_matrix(*matrix);

  std::vector<Element> host_input(static_cast<std::size_t>(kColumns));
  for (std::size_t index = 0; index < host_input.size(); ++index) {
    host_input[index] = static_cast<Element>(index % 17U);
  }
  std::vector<Element> host_output(static_cast<std::size_t>(kRows), Element{0});
  auto input =
      asc_random_cuda_test::Upload<Element>(host_input, resource, execution);
  auto output =
      asc_random_cuda_test::Upload<Element>(host_output, resource, execution);
  if (!input.ok() || !output.ok()) {
    return 4;
  }
  auto input_view = asc::CudaStridedVectorView<const Element>::Create(
      static_cast<const Element*>(input->data()), kColumns, 1,
      asc::MemorySpace::kDevice);
  auto output_view = asc::CudaStridedVectorView<Element>::Create(
      static_cast<Element*>(output->data()), kRows, 1,
      asc::MemorySpace::kDevice);
  if (!input_view.ok() || !output_view.ok()) {
    return 5;
  }
  auto workspace_size =
      asc::CudaCsrSpmvWorkspaceSize(provider, Element{1}, const_matrix,
                                    *input_view, Element{0}, *output_view);
  if (!workspace_size.ok()) {
    return 6;
  }
  auto workspace = asc::Buffer::Allocate(
      resource, std::max(*workspace_size, std::size_t{1}), 256);
  if (!workspace.ok()) {
    return 7;
  }
  const asc::MutableMemoryView workspace_view =
      *workspace_size == 0
          ? asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice)
          : asc::MutableMemoryView(workspace->data(), *workspace_size,
                                   asc::MemorySpace::kDevice);

  const auto run_once = [&]() {
    auto event =
        asc::CudaCsrSpmv(provider, Element{1}, const_matrix, *input_view,
                         Element{0}, *output_view, workspace_view);
    return event.ok() ? event->Wait() : event.status();
  };
  for (int repetition = 0; repetition < kWarmups; ++repetition) {
    if (!run_once().ok()) {
      return 8;
    }
  }
  const auto begin = std::chrono::steady_clock::now();
  for (int repetition = 0; repetition < kRepetitions; ++repetition) {
    if (!run_once().ok()) {
      return 9;
    }
  }
  const auto end = std::chrono::steady_clock::now();

  auto result = asc_random_cuda_test::Download<Element>(*output, execution);
  if (!result.ok()) {
    return 10;
  }
  long double checksum = 0;
  long double expected_checksum = 0;
  for (asc::index_t row = 0; row < kRows; ++row) {
    long double expected = 0;
    for (asc::index_t entry = 0; entry < kEntriesPerRow; ++entry) {
      expected += host_input[static_cast<std::size_t>(entry)];
    }
    checksum += (*result)[static_cast<std::size_t>(row)];
    expected_checksum += expected;
  }
  if (std::fabs(checksum - expected_checksum) != 0.0L) {
    return 11;
  }
  const auto microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();
  std::cout << "operation=csr_spmv scalar=" << scalar_name << " rows=" << kRows
            << " columns=" << kColumns << " nnz=" << indices.size()
            << " warmups=" << kWarmups << " repetitions=" << kRepetitions
            << " workspace_bytes=" << *workspace_size
            << " elapsed_us=" << microseconds
            << " checksum=" << static_cast<double>(checksum) << '\n';
  return 0;
}

}  // namespace

int main() {
  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  const asc::Device device = asc_random_cuda_test::CudaDevice();
  auto execution = asc::CreateCudaExecutionContext(device);
  auto provider = execution.ok()
                      ? asc::SparseCudaContext::Create(*execution)
                      : asc::Result<asc::SparseCudaContext>(execution.status());
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    return 1;
  }
  const int float_result =
      BenchmarkSpmv<float>(*execution, *provider, **resource, "float");
  const int double_result =
      BenchmarkSpmv<double>(*execution, *provider, **resource, "double");
  return float_result == 0 && double_result == 0 ? 0 : 2;
}
