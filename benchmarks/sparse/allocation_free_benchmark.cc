#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <span>

#include "allocation_counter.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/linalg.h"
#include "test_support.h"

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

constexpr std::size_t kRows = 128;
constexpr std::size_t kNnz = 3 * kRows - 2;
constexpr std::size_t kIterations = 200;

}  // namespace

int main() {
  std::array<asc::nnz_t, kRows + 1> offsets{};
  std::array<asc::index_t, kNnz> indices{};
  std::array<double, kNnz> matrix_values{};
  std::size_t stored = 0;
  offsets[0] = 0;
  for (std::size_t row = 0; row < kRows; ++row) {
    if (row != 0) {
      indices[stored] = static_cast<asc::index_t>(row - 1);
      matrix_values[stored++] = 1.0;
    }
    indices[stored] = static_cast<asc::index_t>(row);
    matrix_values[stored++] = 1.0;
    if (row + 1 != kRows) {
      indices[stored] = static_cast<asc::index_t>(row + 1);
      matrix_values[stored++] = 1.0;
    }
    offsets[row + 1] = static_cast<asc::nnz_t>(stored);
  }

  constexpr std::array<asc::extent_t, 2> kShape = {
      static_cast<asc::extent_t>(kRows), static_cast<asc::extent_t>(kRows)};
  auto matrix = asc::CsrView<const double>::Create(
      kShape, offsets, indices, std::span<const double>(matrix_values),
      asc::MemorySpace::kHost);
  if (!matrix.ok()) {
    return 1;
  }

  std::array<double, kRows> input_values{};
  std::array<double, kRows> output_values{};
  input_values.fill(1.0);
  int input_identity = 0;
  int output_identity = 0;
  const external_sparse_test::Vector<const double> input = {
      .data = input_values.data(),
      .size = static_cast<asc::extent_t>(kRows),
      .alias_identity = std::addressof(input_identity),
  };
  external_sparse_test::Vector<double> output = {
      .data = output_values.data(),
      .size = static_cast<asc::extent_t>(kRows),
      .alias_identity = std::addressof(output_identity),
  };

  for (std::size_t warmup = 0; warmup < 5; ++warmup) {
    if (!asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, input, 0.0,
                   output)
             .ok()) {
      return 2;
    }
  }

  asc::Status status = asc::Status::Ok();
  std::size_t allocations = 1;
  const auto start = std::chrono::steady_clock::now();
  {
    asc_sparse_test::AllocationCountScope allocation_scope;
    for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
      status = asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, input,
                         0.0, output);
      if (!status.ok()) {
        break;
      }
    }
    allocations = allocation_scope.count();
  }
  const auto stop = std::chrono::steady_clock::now();
  if (!status.ok()) {
    return 3;
  }

  double checksum = 0.0;
  for (double value : output_values) {
    checksum += value;
  }
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

  std::cout << "compiler=" << kCompiler << '\n'
            << "configuration=" << kConfiguration << '\n'
            << "backend=serial-reference\n"
            << "format=CSR-zero-based-canonical\n"
            << "shape=" << kRows << 'x' << kRows << '\n'
            << "nnz=" << kNnz << '\n'
            << "vector_layout=contiguous-external\n"
            << "iterations=" << kIterations << '\n'
            << "operation=spmv\n"
            << "allocations_in_operations=" << allocations << '\n'
            << "elapsed_us=" << elapsed.count() << '\n'
            << "checksum=" << checksum << '\n';
  return allocations == 0 && checksum == static_cast<double>(kNnz) ? 0 : 4;
}
