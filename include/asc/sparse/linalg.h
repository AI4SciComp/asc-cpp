#ifndef ASC_SPARSE_LINALG_H_
#define ASC_SPARSE_LINALG_H_

#include <array>
#include <concepts>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/export.h"

namespace asc {
namespace internal_sparse_linalg {

template <typename Element>
struct ReadableVectorDescriptor {
  const void* object;
  Element (*read)(const void*, index_t);
};

template <typename Element>
struct WritableVectorDescriptor {
  void* object;
  Element (*read)(const void*, index_t);
  void (*write)(void*, index_t, Element);
};

ASC_SPARSE_EXPORT Status SpmvReference(float alpha, CsrView<const float> matrix,
                                       ReadableVectorDescriptor<float> input,
                                       float beta,
                                       WritableVectorDescriptor<float> output);

ASC_SPARSE_EXPORT Status SpmvReference(double alpha,
                                       CsrView<const double> matrix,
                                       ReadableVectorDescriptor<double> input,
                                       double beta,
                                       WritableVectorDescriptor<double> output);

template <typename Element>
concept SpmvElement =
    std::same_as<Element, float> || std::same_as<Element, double>;

template <typename Element>
bool MatrixStructureMayOverlap(CsrView<const Element> matrix,
                               ExpressionAliasMetadata output_alias) noexcept {
  if (!output_alias.has_byte_span() || output_alias.size() == 0) {
    return false;
  }
  auto offset_count = internal_sparse_compressed::OffsetCount(matrix.rows());
  auto index_bytes = CheckedByteCount(matrix.nnz(), sizeof(index_t));
  if (!offset_count.ok() || !index_bytes.ok()) {
    return true;
  }
  auto offset_bytes = CheckedMultiply(*offset_count, sizeof(nnz_t));
  if (!offset_bytes.ok()) {
    return true;
  }
  const ExpressionAliasMetadata offset_alias(
      matrix.outer_offsets(), matrix.outer_offsets(), *offset_bytes);
  const ExpressionAliasMetadata index_alias(
      matrix.inner_indices(), matrix.inner_indices(), *index_bytes);
  return internal_expression_writable::ByteSpansOverlap(offset_alias,
                                                        output_alias) ||
         internal_expression_writable::ByteSpansOverlap(index_alias,
                                                        output_alias);
}

template <typename Element, PlacedReadableExpression Input,
          WritableExpression Output>
  requires(SpmvElement<Element> && kExpressionRank<Input> == 1 &&
           kExpressionRank<Output> == 1 &&
           std::same_as<ExpressionValue<Input>, Element> &&
           std::same_as<ExpressionValue<Output>, Element>)
Status ValidateSpmv(const ExecutionContext& context, Element /*alpha*/,
                    CsrView<const Element> matrix, const Input& input,
                    Element /*beta*/, const Output& output) {
  Status context_status = internal_sparse_coordinate::ValidateSerialHost(
      context, "Sparse SpMV requires serial execution");
  if (!context_status.ok()) {
    return context_status;
  }
  Status matrix_status = ValidateExpressionAccess(context, matrix);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  Status input_status = ValidateExpressionAccess(context, input);
  if (!input_status.ok()) {
    return input_status;
  }
  Status output_status = ValidateWritableExpressionAccess(context, output);
  if (!output_status.ok()) {
    return output_status;
  }
  if (ExpressionSpace(input) != MemorySpace::kHost ||
      ExpressionSpace(output) != MemorySpace::kHost ||
      matrix.memory_space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse SpMV requires host operands");
  }
  if (!WritableExpressionIsUnique(output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse SpMV requires a unique output mapping");
  }
  const auto input_shape = ExpressionShape(input);
  const auto output_shape = WritableExpressionShape(output);
  if (input_shape[0] != matrix.columns() || output_shape[0] != matrix.rows()) {
    return Status(ErrorCode::kShape,
                  "Sparse SpMV vector lengths do not match the matrix");
  }
  const ExpressionAliasMetadata output_alias = WritableExpressionAlias(output);
  if (ExpressionMayOverlap(input, output_alias) ||
      ExpressionMayOverlap(matrix, output_alias) ||
      internal_sparse_linalg::MatrixStructureMayOverlap(matrix, output_alias)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse SpMV rejects output operand overlap");
  }
  return Status::Ok();
}

}  // namespace internal_sparse_linalg

template <SparseViewElement MatrixElement, PlacedReadableExpression Input,
          WritableExpression Output>
  requires(
      internal_sparse_linalg::SpmvElement<std::remove_const_t<MatrixElement>> &&
      kExpressionRank<Input> == 1 && kExpressionRank<Output> == 1 &&
      std::same_as<ExpressionValue<Input>,
                   std::remove_const_t<MatrixElement>> &&
      std::same_as<ExpressionValue<Output>, std::remove_const_t<MatrixElement>>)
Status Spmv(const ExecutionContext& context,
            std::remove_const_t<MatrixElement> alpha,
            CsrView<MatrixElement> matrix, const Input& input,
            std::remove_const_t<MatrixElement> beta, Output& output) {
  using Element = std::remove_const_t<MatrixElement>;
  const CsrView<const Element> const_matrix = matrix;
  Status validation_status = internal_sparse_linalg::ValidateSpmv(
      context, alpha, const_matrix, input, beta, output);
  if (!validation_status.ok()) {
    return validation_status;
  }

  const internal_sparse_linalg::ReadableVectorDescriptor<Element>
      input_descriptor{
          .object = &input,
          .read =
              [](const void* object, index_t index) {
                const auto& operand = *static_cast<const Input*>(object);
                const std::array<index_t, 1> coordinate{index};
                return ExpressionRead(operand,
                                      std::span<const index_t, 1>(coordinate));
              },
      };
  internal_sparse_linalg::WritableVectorDescriptor<Element> output_descriptor{
      .object = &output,
      .read =
          [](const void* object, index_t index) {
            const auto& operand = *static_cast<const Output*>(object);
            const std::array<index_t, 1> coordinate{index};
            return ExpressionRead(operand,
                                  std::span<const index_t, 1>(coordinate));
          },
      .write =
          [](void* object, index_t index, Element value) {
            auto& operand = *static_cast<Output*>(object);
            const std::array<index_t, 1> coordinate{index};
            WriteExpression(operand, std::span<const index_t, 1>(coordinate),
                            value);
          },
  };
  return internal_sparse_linalg::SpmvReference(
      alpha, const_matrix, input_descriptor, beta, output_descriptor);
}

}  // namespace asc

#endif  // ASC_SPARSE_LINALG_H_
