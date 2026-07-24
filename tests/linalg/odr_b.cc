#include <asc/array.h>
#include <asc/linalg.h>

#include <array>

int LinalgOdrB() {
  using MatrixExtents = asc::Extents<2, 2>;
  using VectorExtents = asc::Extents<2>;
  auto matrix_mapping =
      asc::LayoutRightMapping<MatrixExtents>::Create(MatrixExtents());
  auto vector_mapping =
      asc::LayoutRightMapping<VectorExtents>::Create(VectorExtents());
  if (!matrix_mapping.ok() || !vector_mapping.ok()) {
    return -1;
  }
  std::array<float, 4> matrix{2.0F, 0.0F, 0.0F, 3.0F};
  std::array<float, 2> input{4.0F, 5.0F};
  std::array<float, 2> output{};
  auto matrix_view = asc::TensorView<const float, MatrixExtents,
                                     asc::LayoutRightMapping<MatrixExtents>>
                         ::Create(matrix.data(), matrix_mapping.value(),
                                   matrix.size());
  auto input_view = asc::TensorView<const float, VectorExtents,
                                    asc::LayoutRightMapping<VectorExtents>>
                        ::Create(input.data(), vector_mapping.value(),
                                  input.size());
  auto output_view = asc::TensorView<float, VectorExtents,
                                     asc::LayoutRightMapping<VectorExtents>>
                         ::Create(output.data(), vector_mapping.value(),
                                   output.size());
  if (!matrix_view.ok() || !input_view.ok() || !output_view.ok()) {
    return -1;
  }
  const asc::Status status = asc::Gemv(
      asc::ExecutionContext::Serial(), asc::TransposeMode::kNoTranspose,
      1.0F, matrix_view.value(), input_view.value(), 0.0F,
      output_view.value());
  return status.ok() && output[0] == 8.0F && output[1] == 15.0F ? 25 : -1;
}
