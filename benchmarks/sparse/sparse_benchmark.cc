#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string_view>

#include "../../tests/allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/sparse/blas.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/evaluate.h"
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

// This benchmark keeps setup, timed operations, and independent oracles
// together so its reported measurements cannot silently drift from validation.
// NOLINTNEXTLINE(readability-function-size)
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

  constexpr std::size_t kIndexedNnz = 64;
  std::array<asc::index_t, kIndexedNnz> indexed_indices{};
  std::array<double, kIndexedNnz> indexed_values{};
  for (std::size_t index = 0; index < kIndexedNnz; ++index) {
    indexed_indices[index] = static_cast<asc::index_t>(index);
    indexed_values[index] = 0.5 + static_cast<double>(index % 7);
  }
  auto indexed = asc::SparseBlasIndexedVectorView<const double>::Create(
      indexed_indices.data(), indexed_values.data(), kIndexedNnz, kColumns,
      {indexed_indices.data(), sizeof(indexed_indices),
       asc::MemorySpace::kHost},
      {indexed_values.data(), sizeof(indexed_values), asc::MemorySpace::kHost});
  auto dense_vector = asc::SparseBlasVectorView<const double>::Create(
      input_values.data(), kColumns, 1,
      {input_values.data(), sizeof(input_values), asc::MemorySpace::kHost});
  if (!indexed.ok() || !dense_vector.ok()) {
    return 10;
  }
  constexpr std::size_t kDotIterations = 1024;
  double dot_checksum = 0.0;
  std::size_t dot_allocations = 0;
  const auto dot_start = Clock::now();
  {
    asc_sparse_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kDotIterations; ++iteration) {
      auto result =
          asc::SparseDot(context, asc::SparseBlasConjugation::kUnconjugated,
                         *indexed, *dense_vector);
      if (!result.ok()) {
        return 11;
      }
      dot_checksum += *result;
    }
    dot_allocations = probe.count();
  }
  const auto dot_elapsed = Clock::now() - dot_start;
  long double expected_dot = 0.0L;
  for (std::size_t position = 0; position < kIndexedNnz; ++position) {
    expected_dot +=
        static_cast<long double>(indexed_values[position]) *
        static_cast<long double>(
            input_values[static_cast<std::size_t>(indexed_indices[position])]);
  }
  expected_dot *= static_cast<long double>(kDotIterations);
  if (std::abs(static_cast<long double>(dot_checksum) - expected_dot) >
      1.0e-11L * std::max(1.0L, std::abs(expected_dot))) {
    return 12;
  }
  PrintResult("sparse_dot", kDotIterations, dot_elapsed, dot_checksum,
              dot_allocations);

  constexpr std::size_t kRightHandSides = 4;
  std::array<double, kColumns * kRightHandSides> matrix_input{};
  std::array<double, kRows * kRightHandSides> matrix_output{};
  for (std::size_t position = 0; position < matrix_input.size(); ++position) {
    matrix_input[position] = 0.125 * static_cast<double>(1 + position % 11);
  }
  // A is kRows by kColumns, so the dense right operand has kColumns rows.
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  auto dense_matrix = asc::SparseBlasMatrixView<const double>::Create(
      matrix_input.data(), kColumns, kRightHandSides,
      asc::SparseBlasLayout::kRowMajor, kRightHandSides,
      {matrix_input.data(), sizeof(matrix_input), asc::MemorySpace::kHost});
  // The product has kRows rows and kRightHandSides columns.
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  auto product_matrix = asc::SparseBlasMatrixView<double>::Create(
      matrix_output.data(), kRows, kRightHandSides,
      asc::SparseBlasLayout::kRowMajor, kRightHandSides,
      {matrix_output.data(), sizeof(matrix_output), asc::MemorySpace::kHost});
  if (!dense_matrix.ok() || !product_matrix.ok()) {
    return 13;
  }
  constexpr std::size_t kSpmmIterations = 64;
  std::size_t spmm_allocations = 0;
  const auto spmm_start = Clock::now();
  {
    asc_sparse_test::AllocationProbe probe;
    for (std::size_t iteration = 0; iteration < kSpmmIterations; ++iteration) {
      if (!asc::Spmm(context, asc::SparseBlasTranspose::kNone, 1.0,
                     const_source, *dense_matrix, *product_matrix)
               .ok()) {
        return 14;
      }
    }
    spmm_allocations = probe.count();
  }
  const auto spmm_elapsed = Clock::now() - spmm_start;
  double spmm_checksum = 0.0;
  for (std::size_t row = 0; row < kRows; ++row) {
    for (std::size_t rhs = 0; rhs < kRightHandSides; ++rhs) {
      long double expected = 0.0L;
      for (std::size_t position = static_cast<std::size_t>(offsets[row]);
           position < static_cast<std::size_t>(offsets[row + 1]); ++position) {
        const std::size_t column = static_cast<std::size_t>(indices[position]);
        expected += static_cast<long double>(source_values[position]) *
                    static_cast<long double>(
                        matrix_input[column * kRightHandSides + rhs]);
      }
      expected *= static_cast<long double>(kSpmmIterations);
      const double actual = matrix_output[row * kRightHandSides + rhs];
      if (std::abs(static_cast<long double>(actual) - expected) >
          1.0e-11L * std::max(1.0L, std::abs(expected))) {
        return 15;
      }
      spmm_checksum += actual;
    }
  }
  PrintResult("spmm", kSpmmIterations, spmm_elapsed, spmm_checksum,
              spmm_allocations);

  if (!asc_test::ProcessAllocationCountMatches(evaluate_allocations, 0) ||
      !asc_test::ProcessAllocationCountMatches(spmv_allocations, 0) ||
      !asc_test::ProcessAllocationCountMatches(dot_allocations, 0) ||
      !asc_test::ProcessAllocationCountMatches(spmm_allocations, 0) ||
      !std::isfinite(evaluate_checksum) || !std::isfinite(spmv_checksum) ||
      !std::isfinite(dot_checksum) || !std::isfinite(spmm_checksum)) {
    return 16;
  }
  return 0;
}
