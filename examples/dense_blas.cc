#include <array>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/dense/blas.h"

int main() {
  std::array<double, 4> matrix_storage{1.0, 3.0, 2.0, 4.0};
  std::array<double, 2> input_storage{1.0, 1.0};
  std::array<double, 2> output_storage{};

  auto matrix = asc::DenseBlasMatrixView<const double>::Create(
      matrix_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 2,
      {matrix_storage.data(), sizeof(matrix_storage), asc::MemorySpace::kHost});
  auto input = asc::DenseBlasVectorView<const double>::Create(
      input_storage.data(), 2, 1,
      {input_storage.data(), sizeof(input_storage), asc::MemorySpace::kHost});
  auto output = asc::DenseBlasVectorView<double>::Create(
      output_storage.data(), 2, 1,
      {output_storage.data(), sizeof(output_storage), asc::MemorySpace::kHost});
  if (!matrix.ok() || !input.ok() || !output.ok()) {
    return 1;
  }

  const asc::Status status =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::DenseBlasTranspose::kNone,
                1.0, *matrix, *input, 0.0, *output);
  return status.ok() && output_storage == std::array<double, 2>{3.0, 7.0} ? 0
                                                                          : 2;
}
