#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/dense.h"

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
      asc::Gemv(asc::ExecutionContext::Serial(), asc::DenseTranspose::kNone,
                1.0, const_matrix, *input, 0.0, *output);
  if (!status.ok()) {
    return 7;
  }
  return output_storage == std::array<double, 2>{3.0, 7.0} ? 0 : 8;
}
