#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/sparse/blas.h"

int main() {
  constexpr std::array<asc::extent_t, 2> kMatrixShape{1, 2};
  constexpr std::array<asc::nnz_t, 2> kOffsets{0, 2};
  constexpr std::array<asc::index_t, 2> kIndices{0, 1};
  std::array<double, 2> matrix_values{1.0, 2.0};
  auto matrix = asc::CsrView<const double>::Create(
      kOffsets.data(), kIndices.data(), matrix_values.data(), kMatrixShape, 2,
      asc::MemorySpace::kHost);
  if (!matrix.ok()) {
    return 1;
  }

  constexpr std::array<asc::extent_t, 1> kInputShape{2};
  auto input_mapping = asc::DenseLayout<1>::Create(kInputShape);
  constexpr std::array<asc::extent_t, 1> kOutputShape{1};
  auto output_mapping = asc::DenseLayout<1>::Create(kOutputShape);
  if (!input_mapping.ok() || !output_mapping.ok()) {
    return 2;
  }
  std::array<double, 2> input_values{3.0, 4.0};
  std::array<double, 1> output_values{};
  auto input = asc::DenseView<const double, 1>::Create(
      input_values.data(), *input_mapping, asc::MemorySpace::kHost);
  auto output = asc::DenseView<double, 1>::Create(
      output_values.data(), *output_mapping, asc::MemorySpace::kHost);
  if (!input.ok() || !output.ok()) {
    return 3;
  }
  if (!asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, *input, 0.0,
                 *output)
           .ok()) {
    return 4;
  }
  return output_values[0] == 11.0 ? 0 : 5;
}
