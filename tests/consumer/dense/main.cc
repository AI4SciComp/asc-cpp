#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/dense.h"
#include "asc/dense/blas.h"

int main() {
  using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

  asc::HostMemoryResource resource;
  auto extents = MatrixExtents::Create(2, 2);
  if (!extents.ok()) {
    return 1;
  }
  auto matrix =
      asc::DenseArray<double, MatrixExtents>::Create(resource, *extents);
  if (!matrix.ok()) {
    return 2;
  }
  auto matrix_view = matrix->view();
  if (!matrix_view.ok()) {
    return 3;
  }

  constexpr std::array<double, 4> kValues{1.0, 3.0, 2.0, 4.0};
  for (asc::index_t column = 0; column < 2; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> coordinate{row, column};
      auto element =
          matrix_view->At(std::span<const asc::index_t, 2>(coordinate));
      if (!element.ok()) {
        return 4;
      }
      **element = kValues[static_cast<std::size_t>(row + 2 * column)];
    }
  }

  std::array<double, 2> input_storage{1.0, 1.0};
  std::array<double, 2> output_storage{};
  const std::array<asc::extent_t, 1> vector_shape{2};
  auto vector_mapping = asc::DenseLayout<1>::Create(
      std::span<const asc::extent_t, 1>(vector_shape));
  if (!vector_mapping.ok()) {
    return 5;
  }
  auto input = asc::DenseView<const double, 1>::Create(
      input_storage.data(), *vector_mapping, asc::MemorySpace::kHost);
  auto output = asc::DenseView<double, 1>::Create(
      output_storage.data(), *vector_mapping, asc::MemorySpace::kHost);
  if (!input.ok() || !output.ok()) {
    return 6;
  }
  asc::DenseView<const double, 2> const_matrix(*matrix_view);
  const asc::Status status =
      asc::Gemv(asc::ExecutionContext::Serial(), asc::DenseBlasTranspose::kNone,
                1.0, const_matrix, *input, 0.0, *output);
  if (!status.ok()) {
    return 7;
  }
  if (output_storage != std::array<double, 2>{3.0, 7.0}) {
    return 8;
  }

  std::array<double, 5> left_storage{1.0, 0.0, 2.0, 0.0, 3.0};
  std::array<double, 5> right_storage{4.0, 0.0, 5.0, 0.0, 6.0};
  std::array<double, 1> dot_storage{};
  auto left = asc::DenseBlasVectorView<const double>::Create(
      left_storage.data() + 4, 3, -2,
      asc::ConstMemoryView(left_storage.data(), sizeof(left_storage),
                           asc::MemorySpace::kHost));
  auto right = asc::DenseBlasVectorView<const double>::Create(
      right_storage.data(), 3, 2,
      asc::ConstMemoryView(right_storage.data(), sizeof(right_storage),
                           asc::MemorySpace::kHost));
  auto dot = asc::DenseBlasVectorView<double>::Create(
      dot_storage.data(), 1, 1,
      asc::ConstMemoryView(dot_storage.data(), sizeof(dot_storage),
                           asc::MemorySpace::kHost));
  if (!left.ok() || !right.ok() || !dot.ok()) {
    return 9;
  }
  if (!asc::Dot(asc::ExecutionContext::Serial(), *left, *right, *dot).ok()) {
    return 10;
  }
  if (dot_storage[0] != 28.0) {
    return 11;
  }

  std::array<double, 3> packed_storage{2.0, 1.0, 3.0};
  std::array<double, 2> level2_input_storage{1.0, 2.0};
  std::array<double, 2> level2_output_storage{};
  auto packed = asc::DenseBlasPackedMatrixView<const double>::Create(
      packed_storage.data(), 2, asc::DenseBlasLayout::kColumnMajor,
      asc::ConstMemoryView(packed_storage.data(), sizeof(packed_storage),
                           asc::MemorySpace::kHost));
  auto level2_input = asc::DenseBlasVectorView<const double>::Create(
      level2_input_storage.data(), 2, 1,
      asc::ConstMemoryView(level2_input_storage.data(),
                           sizeof(level2_input_storage),
                           asc::MemorySpace::kHost));
  auto level2_output = asc::DenseBlasVectorView<double>::Create(
      level2_output_storage.data(), 2, 1,
      asc::ConstMemoryView(level2_output_storage.data(),
                           sizeof(level2_output_storage),
                           asc::MemorySpace::kHost));
  if (!packed.ok() || !level2_input.ok() || !level2_output.ok()) {
    return 12;
  }
  if (!asc::Spmv(asc::ExecutionContext::Serial(),
                 asc::DenseBlasTriangle::kUpper, 1.0, *packed, *level2_input,
                 0.0, *level2_output)
           .ok()) {
    return 13;
  }
  return level2_output_storage == std::array<double, 2>{4.0, 7.0} ? 0 : 14;
}
