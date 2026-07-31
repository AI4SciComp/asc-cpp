#include "asc/dense/blas.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"

namespace asc {
namespace internal_dense_blas {

Status ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Dense CPU linear algebra requires serial execution");
  }
  if (!context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial execution cannot access host memory");
  }
  return Status::Ok();
}

template <typename Element, std::size_t Rank>
Status ValidateView(DenseView<Element, Rank> view) {
  if (view.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Dense CPU linear algebra requires host storage");
  }
  if (view.mapping().required_span_size() != 0 && view.data() == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty DenseView has a null pointer");
  }
  return Status::Ok();
}

template <typename LeftElement, typename RightElement, std::size_t Rank>
bool SameDescriptor(DenseView<LeftElement, Rank> left,
                    DenseView<RightElement, Rank> right) {
  if (static_cast<const void*>(left.data()) !=
          static_cast<const void*>(right.data()) ||
      left.memory_space() != right.memory_space()) {
    return false;
  }
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    if (left.extents()[dimension] != right.extents()[dimension] ||
        left.strides()[dimension] != right.strides()[dimension]) {
      return false;
    }
  }
  return true;
}

template <typename LeftElement, std::size_t LeftRank, typename RightElement,
          std::size_t RightRank>
bool SpansOverlap(DenseView<LeftElement, LeftRank> left,
                  DenseView<RightElement, RightRank> right) {
  if (left.mapping().required_span_size() == 0 ||
      right.mapping().required_span_size() == 0) {
    return false;
  }
  const std::uintptr_t left_begin =
      reinterpret_cast<std::uintptr_t>(left.data());
  const std::uintptr_t right_begin =
      reinterpret_cast<std::uintptr_t>(right.data());
  const std::size_t left_bytes = left.mapping().required_span_size() *
                                 sizeof(std::remove_const_t<LeftElement>);
  const std::size_t right_bytes = right.mapping().required_span_size() *
                                  sizeof(std::remove_const_t<RightElement>);
  if (left_bytes > std::numeric_limits<std::uintptr_t>::max() - left_begin ||
      right_bytes > std::numeric_limits<std::uintptr_t>::max() - right_begin) {
    return true;
  }
  const std::uintptr_t left_end = left_begin + left_bytes;
  const std::uintptr_t right_end = right_begin + right_bytes;
  return left_begin < right_end && right_begin < left_end;
}

template <typename Element, std::size_t Rank>
Element& ElementAt(DenseView<Element, Rank> view,
                   std::span<const index_t, Rank> coordinates) {
  stride_t offset = 0;
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    offset += coordinates[dimension] * view.strides()[dimension];
  }
  return view.data()[static_cast<std::size_t>(offset)];
}

template <typename Element, std::size_t Rank>
Status ValidatePair(DenseView<const Element, Rank> source,
                    DenseView<Element, Rank> destination) {
  Status source_status = ValidateView(source);
  if (!source_status.ok()) {
    return source_status;
  }
  Status destination_status = ValidateView(destination);
  if (!destination_status.ok()) {
    return destination_status;
  }
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    if (source.extents()[dimension] != destination.extents()[dimension]) {
      return Status(ErrorCode::kShape,
                    "Dense linear-algebra operand shapes do not match");
    }
  }
  return Status::Ok();
}

template <typename Element, std::size_t Rank>
Status CopyImpl(const ExecutionContext& context,
                DenseView<const Element, Rank> source,
                DenseView<Element, Rank> destination) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(source, destination);
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (SameDescriptor(source, destination)) {
    return Status::Ok();
  }
  if (SpansOverlap(source, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Copy rejects partially overlapping DenseViews");
  }

  if constexpr (Rank == 1) {
    for (index_t index = 0; index < source.extents()[0]; ++index) {
      const std::array<index_t, 1> coordinate{index};
      ElementAt(destination, std::span<const index_t, 1>(coordinate)) =
          ElementAt(source, std::span<const index_t, 1>(coordinate));
    }
  } else {
    for (index_t column = 0; column < source.extents()[1]; ++column) {
      for (index_t row = 0; row < source.extents()[0]; ++row) {
        const std::array<index_t, 2> coordinate{row, column};
        ElementAt(destination, std::span<const index_t, 2>(coordinate)) =
            ElementAt(source, std::span<const index_t, 2>(coordinate));
      }
    }
  }
  return Status::Ok();
}

template <typename Element, std::size_t Rank>
Status ScalImpl(const ExecutionContext& context, Element alpha,
                DenseView<Element, Rank> destination) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status view_status = ValidateView(destination);
  if (!view_status.ok()) {
    return view_status;
  }

  if constexpr (Rank == 1) {
    for (index_t index = 0; index < destination.extents()[0]; ++index) {
      const std::array<index_t, 1> coordinate{index};
      Element& value =
          ElementAt(destination, std::span<const index_t, 1>(coordinate));
      value = alpha * value;
    }
  } else {
    for (index_t column = 0; column < destination.extents()[1]; ++column) {
      for (index_t row = 0; row < destination.extents()[0]; ++row) {
        const std::array<index_t, 2> coordinate{row, column};
        Element& value =
            ElementAt(destination, std::span<const index_t, 2>(coordinate));
        value = alpha * value;
      }
    }
  }
  return Status::Ok();
}

template <typename Element, std::size_t Rank>
Status AxpyImpl(const ExecutionContext& context, Element alpha,
                DenseView<const Element, Rank> source,
                DenseView<Element, Rank> destination) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status pair_status = ValidatePair(source, destination);
  if (!pair_status.ok()) {
    return pair_status;
  }
  if (!SameDescriptor(source, destination) &&
      SpansOverlap(source, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Axpy rejects partial operand overlap");
  }

  if constexpr (Rank == 1) {
    for (index_t index = 0; index < source.extents()[0]; ++index) {
      const std::array<index_t, 1> coordinate{index};
      Element& output =
          ElementAt(destination, std::span<const index_t, 1>(coordinate));
      output =
          alpha * ElementAt(source, std::span<const index_t, 1>(coordinate)) +
          output;
    }
  } else {
    for (index_t column = 0; column < source.extents()[1]; ++column) {
      for (index_t row = 0; row < source.extents()[0]; ++row) {
        const std::array<index_t, 2> coordinate{row, column};
        Element& output =
            ElementAt(destination, std::span<const index_t, 2>(coordinate));
        output =
            alpha * ElementAt(source, std::span<const index_t, 2>(coordinate)) +
            output;
      }
    }
  }
  return Status::Ok();
}

template <typename Element>
Result<Element> DotImpl(const ExecutionContext& context,
                        DenseView<const Element, 1> left,
                        DenseView<const Element, 1> right) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status left_status = ValidateView(left);
  if (!left_status.ok()) {
    return left_status;
  }
  Status right_status = ValidateView(right);
  if (!right_status.ok()) {
    return right_status;
  }
  if (left.extents()[0] != right.extents()[0]) {
    return Status(ErrorCode::kShape, "Dot operand lengths do not match");
  }

  Element result = 0;
  for (index_t index = 0; index < left.extents()[0]; ++index) {
    const std::array<index_t, 1> coordinate{index};
    result += ElementAt(left, std::span<const index_t, 1>(coordinate)) *
              ElementAt(right, std::span<const index_t, 1>(coordinate));
  }
  return result;
}

template <typename Element>
Result<Element> Nrm2Impl(const ExecutionContext& context,
                         DenseView<const Element, 1> operand) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  Status operand_status = ValidateView(operand);
  if (!operand_status.ok()) {
    return operand_status;
  }

  Element scale = 0;
  Element sum_of_squares = 1;
  bool has_infinity = false;
  for (index_t index = 0; index < operand.extents()[0]; ++index) {
    const std::array<index_t, 1> coordinate{index};
    const Element magnitude =
        std::abs(ElementAt(operand, std::span<const index_t, 1>(coordinate)));
    if (std::isnan(magnitude)) {
      return std::numeric_limits<Element>::quiet_NaN();
    }
    if (std::isinf(magnitude)) {
      has_infinity = true;
      continue;
    }
    if (magnitude == 0) {
      continue;
    }
    if (scale < magnitude) {
      const Element ratio = scale / magnitude;
      sum_of_squares = 1 + sum_of_squares * ratio * ratio;
      scale = magnitude;
    } else {
      const Element ratio = magnitude / scale;
      sum_of_squares += ratio * ratio;
    }
  }
  if (has_infinity) {
    return std::numeric_limits<Element>::infinity();
  }
  return scale == 0 ? Element{0}
                    : scale * static_cast<Element>(std::sqrt(sum_of_squares));
}

bool IsValidTranspose(DenseBlasTranspose transpose) {
  return transpose == DenseBlasTranspose::kNone ||
         transpose == DenseBlasTranspose::kTranspose;
}

template <typename Element>
Status GemvImpl(const ExecutionContext& context, DenseBlasTranspose transpose,
                Element alpha, DenseView<const Element, 2> matrix,
                DenseView<const Element, 1> input, Element beta,
                DenseView<Element, 1> output) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!IsValidTranspose(transpose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Gemv transpose is not a recognized enumerator");
  }
  for (Status status :
       {ValidateView(matrix), ValidateView(input), ValidateView(output)}) {
    if (!status.ok()) {
      return status;
    }
  }

  const extent_t output_size = transpose == DenseBlasTranspose::kNone
                                   ? matrix.extents()[0]
                                   : matrix.extents()[1];
  const extent_t inner_size = transpose == DenseBlasTranspose::kNone
                                  ? matrix.extents()[1]
                                  : matrix.extents()[0];
  if (input.extents()[0] != inner_size || output.extents()[0] != output_size) {
    return Status(ErrorCode::kShape, "Gemv operand shapes do not match");
  }
  if (SpansOverlap(matrix, output) || SpansOverlap(input, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Gemv rejects output overlap with an input");
  }

  for (index_t output_index = 0; output_index < output_size; ++output_index) {
    Element product = 0;
    for (index_t inner = 0; inner < inner_size; ++inner) {
      const std::array<index_t, 2> matrix_coordinate =
          transpose == DenseBlasTranspose::kNone
              ? std::array<index_t, 2>{output_index, inner}
              : std::array<index_t, 2>{inner, output_index};
      const std::array<index_t, 1> input_coordinate{inner};
      product +=
          ElementAt(matrix, std::span<const index_t, 2>(matrix_coordinate)) *
          ElementAt(input, std::span<const index_t, 1>(input_coordinate));
    }
    const std::array<index_t, 1> output_coordinate{output_index};
    Element& output_value =
        ElementAt(output, std::span<const index_t, 1>(output_coordinate));
    if (beta == 0) {
      output_value = alpha * product;
    } else {
      output_value = alpha * product + beta * output_value;
    }
  }
  return Status::Ok();
}

template <typename Element>
Status GemmImpl(const ExecutionContext& context,
                DenseBlasTranspose left_transpose,
                DenseBlasTranspose right_transpose, Element alpha,
                DenseView<const Element, 2> left,
                DenseView<const Element, 2> right, Element beta,
                DenseView<Element, 2> output) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!IsValidTranspose(left_transpose) || !IsValidTranspose(right_transpose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Gemm transpose is not a recognized enumerator");
  }
  for (Status status :
       {ValidateView(left), ValidateView(right), ValidateView(output)}) {
    if (!status.ok()) {
      return status;
    }
  }

  const extent_t rows = left_transpose == DenseBlasTranspose::kNone
                            ? left.extents()[0]
                            : left.extents()[1];
  const extent_t inner = left_transpose == DenseBlasTranspose::kNone
                             ? left.extents()[1]
                             : left.extents()[0];
  const extent_t right_inner = right_transpose == DenseBlasTranspose::kNone
                                   ? right.extents()[0]
                                   : right.extents()[1];
  const extent_t columns = right_transpose == DenseBlasTranspose::kNone
                               ? right.extents()[1]
                               : right.extents()[0];
  if (inner != right_inner || output.extents()[0] != rows ||
      output.extents()[1] != columns) {
    return Status(ErrorCode::kShape, "Gemm operand shapes do not match");
  }
  if (SpansOverlap(left, output) || SpansOverlap(right, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Gemm rejects output overlap with an input");
  }

  for (index_t column = 0; column < columns; ++column) {
    for (index_t row = 0; row < rows; ++row) {
      Element product = 0;
      for (index_t inner_index = 0; inner_index < inner; ++inner_index) {
        const std::array<index_t, 2> left_coordinate =
            left_transpose == DenseBlasTranspose::kNone
                ? std::array<index_t, 2>{row, inner_index}
                : std::array<index_t, 2>{inner_index, row};
        const std::array<index_t, 2> right_coordinate =
            right_transpose == DenseBlasTranspose::kNone
                ? std::array<index_t, 2>{inner_index, column}
                : std::array<index_t, 2>{column, inner_index};
        product +=
            ElementAt(left, std::span<const index_t, 2>(left_coordinate)) *
            ElementAt(right, std::span<const index_t, 2>(right_coordinate));
      }
      const std::array<index_t, 2> output_coordinate{row, column};
      Element& output_value =
          ElementAt(output, std::span<const index_t, 2>(output_coordinate));
      if (beta == 0) {
        output_value = alpha * product;
      } else {
        output_value = alpha * product + beta * output_value;
      }
    }
  }
  return Status::Ok();
}

}  // namespace internal_dense_blas

Status Copy(const ExecutionContext& context, DenseView<const float, 1> source,
            DenseView<float, 1> destination) {
  return internal_dense_blas::CopyImpl(context, source, destination);
}

Status Copy(const ExecutionContext& context, DenseView<const float, 2> source,
            DenseView<float, 2> destination) {
  return internal_dense_blas::CopyImpl(context, source, destination);
}

Status Copy(const ExecutionContext& context, DenseView<const double, 1> source,
            DenseView<double, 1> destination) {
  return internal_dense_blas::CopyImpl(context, source, destination);
}

Status Copy(const ExecutionContext& context, DenseView<const double, 2> source,
            DenseView<double, 2> destination) {
  return internal_dense_blas::CopyImpl(context, source, destination);
}

Status Scal(const ExecutionContext& context, float alpha,
            DenseView<float, 1> destination) {
  return internal_dense_blas::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, float alpha,
            DenseView<float, 2> destination) {
  return internal_dense_blas::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, double alpha,
            DenseView<double, 1> destination) {
  return internal_dense_blas::ScalImpl(context, alpha, destination);
}

Status Scal(const ExecutionContext& context, double alpha,
            DenseView<double, 2> destination) {
  return internal_dense_blas::ScalImpl(context, alpha, destination);
}

Status Axpy(const ExecutionContext& context, float alpha,
            DenseView<const float, 1> source, DenseView<float, 1> destination) {
  return internal_dense_blas::AxpyImpl(context, alpha, source, destination);
}

Status Axpy(const ExecutionContext& context, float alpha,
            DenseView<const float, 2> source, DenseView<float, 2> destination) {
  return internal_dense_blas::AxpyImpl(context, alpha, source, destination);
}

Status Axpy(const ExecutionContext& context, double alpha,
            DenseView<const double, 1> source,
            DenseView<double, 1> destination) {
  return internal_dense_blas::AxpyImpl(context, alpha, source, destination);
}

Status Axpy(const ExecutionContext& context, double alpha,
            DenseView<const double, 2> source,
            DenseView<double, 2> destination) {
  return internal_dense_blas::AxpyImpl(context, alpha, source, destination);
}

Result<float> Dot(const ExecutionContext& context,
                  DenseView<const float, 1> left,
                  DenseView<const float, 1> right) {
  return internal_dense_blas::DotImpl(context, left, right);
}

Result<double> Dot(const ExecutionContext& context,
                   DenseView<const double, 1> left,
                   DenseView<const double, 1> right) {
  return internal_dense_blas::DotImpl(context, left, right);
}

Result<float> Nrm2(const ExecutionContext& context,
                   DenseView<const float, 1> operand) {
  return internal_dense_blas::Nrm2Impl(context, operand);
}

Result<double> Nrm2(const ExecutionContext& context,
                    DenseView<const double, 1> operand) {
  return internal_dense_blas::Nrm2Impl(context, operand);
}

Status Gemv(const ExecutionContext& context, DenseBlasTranspose transpose,
            float alpha, DenseView<const float, 2> matrix,
            DenseView<const float, 1> input, float beta,
            DenseView<float, 1> output) {
  return internal_dense_blas::GemvImpl(context, transpose, alpha, matrix, input,
                                       beta, output);
}

Status Gemv(const ExecutionContext& context, DenseBlasTranspose transpose,
            double alpha, DenseView<const double, 2> matrix,
            DenseView<const double, 1> input, double beta,
            DenseView<double, 1> output) {
  return internal_dense_blas::GemvImpl(context, transpose, alpha, matrix, input,
                                       beta, output);
}

Status Gemm(const ExecutionContext& context, DenseBlasTranspose left_transpose,
            DenseBlasTranspose right_transpose, float alpha,
            DenseView<const float, 2> left, DenseView<const float, 2> right,
            float beta, DenseView<float, 2> output) {
  return internal_dense_blas::GemmImpl(context, left_transpose, right_transpose,
                                       alpha, left, right, beta, output);
}

Status Gemm(const ExecutionContext& context, DenseBlasTranspose left_transpose,
            DenseBlasTranspose right_transpose, double alpha,
            DenseView<const double, 2> left, DenseView<const double, 2> right,
            double beta, DenseView<double, 2> output) {
  return internal_dense_blas::GemmImpl(context, left_transpose, right_transpose,
                                       alpha, left, right, beta, output);
}

}  // namespace asc
