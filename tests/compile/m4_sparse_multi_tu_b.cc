#include <array>
#include <cstddef>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/linalg.h"
#include "m4_sparse_multi_tu.h"

double M4SpmvChecksum() {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets = {0, 1, 2};
  constexpr std::array<asc::index_t, 2> kIndices = {0, 1};
  std::array<double, 2> matrix_values = {2.0, 3.0};
  auto matrix = asc::CsrView<const double>::Create(
      kShape, kOffsets, kIndices, std::span<const double>(matrix_values),
      asc::MemorySpace::kHost);

  constexpr std::array<asc::extent_t, 1> kVectorShape = {2};
  auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kVectorShape);
  std::array<double, 2> input_values = {4.0, 5.0};
  std::array<double, 2> output_values{};
  auto input = asc::DenseView<const double, 1>::Create(
      input_values.data(), *mapping, asc::MemorySpace::kHost);
  auto output = asc::DenseView<double, 1>::Create(
      output_values.data(), *mapping, asc::MemorySpace::kHost);
  if (!matrix.ok() || !mapping.ok() || !input.ok() || !output.ok() ||
      !asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, *input, 0.0,
                 *output)
           .ok()) {
    return -1.0;
  }
  return output_values[0] + output_values[1];
}
