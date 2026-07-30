#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string_view>

#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/sparse.h"
#include "external_vector.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kRows = 128;
constexpr std::size_t kColumns = 256;
constexpr std::size_t kEntriesPerRow = 4;
constexpr std::size_t kNnz = kRows * kEntriesPerRow;

const char* CompilerName() {
#if defined(__clang__)
  return "clang";
#elif defined(__GNUC__)
  return "gcc";
#elif defined(_MSC_VER)
  return "msvc";
#else
  return "unknown";
#endif
}

const char* BuildConfiguration() {
#if defined(NDEBUG)
  return "release-like";
#else
  return "debug-like";
#endif
}

void PrintResult(std::string_view operation, std::size_t iterations,
                 Clock::duration elapsed, double checksum,
                 std::size_t allocations) {
  const auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
  std::cout << "compiler=" << CompilerName()
            << " configuration=" << BuildConfiguration() << " rows=" << kRows
            << " columns=" << kColumns << " nnz=" << kNnz << " format=csr"
            << " operation=" << operation << " iterations=" << iterations
            << " total_ns=" << nanoseconds << " per_iteration_ns="
            << static_cast<double>(nanoseconds) /
                   static_cast<double>(iterations)
            << " checksum=" << checksum << " allocations=" << allocations
            << " oracle=independent\n";
}

}  // namespace

int main() {
  constexpr std::size_t kEvaluateIterations = 64;
  constexpr std::size_t kSpmvIterations = 256;
  std::array<asc::nnz_t, kRows + 1> offsets{};
  std::array<asc::index_t, kNnz> indices{};
  std::array<double, kNnz> source_values{};
  std::array<double, kNnz> destination_values{};
  for (std::size_t row = 0; row < kRows; ++row) {
    offsets[row] = static_cast<asc::nnz_t>(row * kEntriesPerRow);
    const std::size_t first_column =
        (row * kEntriesPerRow) % (kColumns - kEntriesPerRow);
    for (std::size_t entry = 0; entry < kEntriesPerRow; ++entry) {
      const std::size_t position = row * kEntriesPerRow + entry;
      indices[position] = static_cast<asc::index_t>(first_column + entry);
      source_values[position] =
          0.125 * static_cast<double>(1 + (position % 17));
      destination_values[position] = -1.0;
    }
  }
  offsets[kRows] = static_cast<asc::nnz_t>(kNnz);

  constexpr std::array<asc::extent_t, 2> kShape{
      static_cast<asc::extent_t>(kRows), static_cast<asc::extent_t>(kColumns)};
  asc::HostMemoryResource resource;
  auto source = asc::CsrArray<double>::Create(resource, kShape, offsets,
                                              indices, source_values);
  auto destination = asc::CsrArray<double>::Create(resource, kShape, offsets,
                                                   indices, destination_values);
  if (!source.ok() || !destination.ok()) {
    return 1;
  }
  auto source_view = source->view();
  auto destination_view = destination->view();
  if (!source_view.ok() || !destination_view.ok()) {
    return 2;
  }
  asc::CsrView<const double> const_source(*source_view);
  auto negated = asc::MakeNegate(const_source);
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  if (!asc::Evaluate(context, negated, *destination_view).ok()) {
    return 3;
  }

  const auto evaluate_start = Clock::now();
  std::size_t evaluate_allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kEvaluateIterations;
         ++iteration) {
      if (!asc::Evaluate(context, negated, *destination_view).ok()) {
        return 4;
      }
    }
    evaluate_allocations = probe.count();
  }
  const auto evaluate_elapsed = Clock::now() - evaluate_start;
  double evaluate_checksum = 0.0;
  for (asc::nnz_t position = 0; position < destination_view->nnz();
       ++position) {
    auto value = destination_view->AtStored(position);
    const std::size_t index = static_cast<std::size_t>(position);
    const double expected = -source_values[index];
    if (!value.ok() || **value != expected) {
      return 5;
    }
    evaluate_checksum += **value;
  }
  PrintResult("structure_preserving_evaluate", kEvaluateIterations,
              evaluate_elapsed, evaluate_checksum, evaluate_allocations);

  std::array<double, kColumns> input_values{};
  std::array<double, kRows> output_values{};
  for (std::size_t column = 0; column < kColumns; ++column) {
    input_values[column] = 0.25 * static_cast<double>(1 + column % 13);
  }
  const M4ExternalVector<const double> input{
      .data = input_values.data(),
      .size = static_cast<asc::extent_t>(kColumns)};
  M4ExternalVector<double> output{.data = output_values.data(),
                                  .size = static_cast<asc::extent_t>(kRows)};
  if (!asc::Spmv(context, 1.0, const_source, input, 0.0, output).ok()) {
    return 6;
  }

  const auto spmv_start = Clock::now();
  std::size_t spmv_allocations = 0;
  {
    asc_sparse_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kSpmvIterations; ++iteration) {
      if (!asc::Spmv(context, 1.0, const_source, input, 0.0, output).ok()) {
        return 7;
      }
    }
    spmv_allocations = probe.count();
  }
  const auto spmv_elapsed = Clock::now() - spmv_start;
  double spmv_checksum = 0.0;
  for (std::size_t row = 0; row < kRows; ++row) {
    long double expected = 0.0L;
    for (std::size_t position = static_cast<std::size_t>(offsets[row]);
         position < static_cast<std::size_t>(offsets[row + 1]); ++position) {
      const std::size_t column = static_cast<std::size_t>(indices[position]);
      expected += static_cast<long double>(source_values[position]) *
                  static_cast<long double>(input_values[column]);
    }
    const long double actual = output_values[row];
    const long double tolerance = 1.0e-12L * std::max(1.0L, std::abs(expected));
    if (std::abs(actual - expected) > tolerance) {
      return 8;
    }
    spmv_checksum += output_values[row];
  }
  PrintResult("spmv", kSpmvIterations, spmv_elapsed, spmv_checksum,
              spmv_allocations);

  if (!asc_test::ProcessAllocationCountMatches(evaluate_allocations, 0) ||
      !asc_test::ProcessAllocationCountMatches(spmv_allocations, 0) ||
      !std::isfinite(evaluate_checksum) || !std::isfinite(spmv_checksum)) {
    return 9;
  }
  return 0;
}
