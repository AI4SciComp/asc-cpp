#ifndef ASC_DENSE_PROVIDERS_CUDA_H_
#define ASC_DENSE_PROVIDERS_CUDA_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/linalg.h"
#include "asc/dense/providers/cuda_export.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"

namespace asc {

class DenseCudaContext;

namespace internal_dense_cuda {

class Access;
class ContextState;

enum class PointwiseOperation {
  kCopy,
  kFill,
  kNegate,
  kAdd,
  kSubtract,
  kMultiply,
};

enum class OperandKind {
  kView,
  kScalar,
};

enum class ElementKind {
  kFloat,
  kDouble,
};

struct ViewDescriptor {
  const void* data = nullptr;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  std::size_t rank = 0;
  std::array<extent_t, 8> extents{};
  std::array<stride_t, 8> strides{};
  extent_t logical_size = 0;
  std::size_t required_span_size = 0;
  DenseLayoutKind layout_kind = DenseLayoutKind::kStride;
};

struct OperandDescriptor {
  OperandKind kind = OperandKind::kScalar;
  ViewDescriptor view;
  double scalar = 0.0;
};

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaEvaluateErased(
    DenseCudaContext& context, PointwiseOperation operation,
    OperandDescriptor left, OperandDescriptor right,
    ViewDescriptor destination);

template <typename T>
inline constexpr bool kSupportedElement =
    std::same_as<std::remove_cv_t<T>, float> ||
    std::same_as<std::remove_cv_t<T>, double>;

template <typename T>
struct IsDenseView : std::false_type {};

template <typename Element, std::size_t Rank>
struct IsDenseView<DenseView<Element, Rank>> : std::true_type {};

template <typename T>
inline constexpr bool kIsDenseView = IsDenseView<std::remove_cvref_t<T>>::value;

template <typename T>
inline constexpr ExpressionOperation kOperation = [] {
  using Adapter = ExpressionAdapter<std::remove_cvref_t<T>>;
  if constexpr (requires { Adapter::operation; }) {
    return Adapter::operation;
  } else {
    return ExpressionOperation::kExternal;
  }
}();

template <typename Element, std::size_t Rank>
ViewDescriptor Describe(DenseView<Element, Rank> view) {
  ViewDescriptor descriptor;
  descriptor.data = view.data();
  descriptor.memory_space = view.memory_space();
  descriptor.element_kind = std::same_as<std::remove_cv_t<Element>, float>
                                ? ElementKind::kFloat
                                : ElementKind::kDouble;
  descriptor.rank = Rank;
  descriptor.logical_size = view.logical_size();
  descriptor.required_span_size = view.mapping().required_span_size();
  descriptor.layout_kind = view.mapping().kind();
  for (std::size_t dimension = 0; dimension < Rank && dimension < 8;
       ++dimension) {
    descriptor.extents[dimension] = view.extents()[dimension];
    descriptor.strides[dimension] = view.strides()[dimension];
  }
  return descriptor;
}

template <typename Operand, typename Element>
Result<OperandDescriptor> DescribeOperand(const Operand& operand) {
  using OperandType = std::remove_cvref_t<Operand>;
  if constexpr (std::is_arithmetic_v<OperandType>) {
    if constexpr (std::same_as<OperandType, float> ||
                  std::same_as<OperandType, double>) {
      if constexpr (!std::same_as<OperandType, Element>) {
        return Status(ErrorCode::kUnsupported,
                      "CUDA scalar and destination element types must match");
      } else {
        OperandDescriptor descriptor;
        descriptor.kind = OperandKind::kScalar;
        descriptor.scalar = static_cast<double>(operand);
        descriptor.view.element_kind = std::same_as<Element, float>
                                           ? ElementKind::kFloat
                                           : ElementKind::kDouble;
        return descriptor;
      }
    } else {
      return Status(ErrorCode::kUnsupported,
                    "CUDA evaluation supports only float/double scalars");
    }
  } else if constexpr (kIsDenseView<OperandType>) {
    using View = OperandType;
    if constexpr (!std::same_as<typename View::value_type, Element> ||
                  View::kRank > 8) {
      return Status(ErrorCode::kUnsupported,
                    "CUDA evaluation operand type or rank is not supported");
    } else {
      OperandDescriptor descriptor;
      descriptor.kind = OperandKind::kView;
      descriptor.view = Describe(operand);
      return descriptor;
    }
  } else {
    return Status(
        ErrorCode::kUnsupported,
        "CUDA evaluation accepts only Dense terminals and scalar operands");
  }
}

}  // namespace internal_dense_cuda

class ASC_DENSE_CUDA_EXPORT DenseCudaContext {
 public:
  static Result<DenseCudaContext> Create(ExecutionContext execution_context);

  DenseCudaContext(const DenseCudaContext&) = delete;
  DenseCudaContext& operator=(const DenseCudaContext&) = delete;
  DenseCudaContext(DenseCudaContext&& other) noexcept;
  DenseCudaContext& operator=(DenseCudaContext&& other) noexcept;
  ~DenseCudaContext();

  [[nodiscard]] const ExecutionContext& execution_context() const noexcept {
    return execution_context_;
  }

 private:
  friend class internal_dense_cuda::Access;

  DenseCudaContext(
      ExecutionContext execution_context,
      std::unique_ptr<internal_dense_cuda::ContextState> state) noexcept;

  ExecutionContext execution_context_;
  std::unique_ptr<internal_dense_cuda::ContextState> state_;
};

template <ReadableExpression Expression, typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
Result<CompletionEvent> CudaEvaluate(DenseCudaContext& context,
                                     const Expression& expression,
                                     DenseView<Element, Rank> destination) {
  using Value = std::remove_cv_t<Element>;
  if constexpr (Rank > 8 || !std::same_as<ExpressionValue<Expression>, Value>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation supports matching float/double ranks 0--8");
  } else {
    constexpr ExpressionOperation kExpressionOperation =
        internal_dense_cuda::kOperation<Expression>;
    internal_dense_cuda::OperandDescriptor left;
    internal_dense_cuda::OperandDescriptor right;
    internal_dense_cuda::PointwiseOperation operation;

    if constexpr (kExpressionOperation == ExpressionOperation::kTerminal ||
                  kExpressionOperation == ExpressionOperation::kScalar) {
      auto described =
          internal_dense_cuda::DescribeOperand<Expression, Value>(expression);
      if (!described.ok()) {
        return described.status();
      }
      left = *described;
      operation = kExpressionOperation == ExpressionOperation::kTerminal
                      ? internal_dense_cuda::PointwiseOperation::kCopy
                      : internal_dense_cuda::PointwiseOperation::kFill;
    } else if constexpr (kExpressionOperation == ExpressionOperation::kNegate) {
      if constexpr (requires { expression.operand_storage(); }) {
        const auto& operand =
            internal_expression::StoredExpression(expression.operand_storage());
        if constexpr (internal_dense_cuda::kOperation<decltype(operand)> !=
                      ExpressionOperation::kTerminal) {
          return Status(ErrorCode::kUnsupported,
                        "CUDA evaluation rejects nested Negate operands");
        } else {
          auto described =
              internal_dense_cuda::DescribeOperand<decltype(operand), Value>(
                  operand);
          if (!described.ok()) {
            return described.status();
          }
          left = *described;
          operation = internal_dense_cuda::PointwiseOperation::kNegate;
        }
      } else {
        return Status(ErrorCode::kUnsupported,
                      "CUDA evaluation rejects external Negate nodes");
      }
    } else if constexpr (kExpressionOperation == ExpressionOperation::kAdd ||
                         kExpressionOperation ==
                             ExpressionOperation::kSubtract ||
                         kExpressionOperation ==
                             ExpressionOperation::kMultiply) {
      if constexpr (requires {
                      expression.left_storage();
                      expression.right_storage();
                    }) {
        const auto& left_operand =
            internal_expression::StoredExpression(expression.left_storage());
        const auto& right_operand =
            internal_expression::StoredExpression(expression.right_storage());
        constexpr ExpressionOperation kLeftOperation =
            internal_dense_cuda::kOperation<decltype(left_operand)>;
        constexpr ExpressionOperation kRightOperation =
            internal_dense_cuda::kOperation<decltype(right_operand)>;
        if constexpr ((kLeftOperation != ExpressionOperation::kTerminal &&
                       kLeftOperation != ExpressionOperation::kScalar) ||
                      (kRightOperation != ExpressionOperation::kTerminal &&
                       kRightOperation != ExpressionOperation::kScalar)) {
          return Status(ErrorCode::kUnsupported,
                        "CUDA evaluation rejects nested binary operands");
        } else {
          auto described_left =
              internal_dense_cuda::DescribeOperand<decltype(left_operand),
                                                   Value>(left_operand);
          if (!described_left.ok()) {
            return described_left.status();
          }
          auto described_right =
              internal_dense_cuda::DescribeOperand<decltype(right_operand),
                                                   Value>(right_operand);
          if (!described_right.ok()) {
            return described_right.status();
          }
          left = *described_left;
          right = *described_right;
          if constexpr (kExpressionOperation == ExpressionOperation::kAdd) {
            operation = internal_dense_cuda::PointwiseOperation::kAdd;
          } else if constexpr (kExpressionOperation ==
                               ExpressionOperation::kSubtract) {
            operation = internal_dense_cuda::PointwiseOperation::kSubtract;
          } else {
            operation = internal_dense_cuda::PointwiseOperation::kMultiply;
          }
        }
      } else {
        return Status(ErrorCode::kUnsupported,
                      "CUDA evaluation rejects external binary nodes");
      }
    } else {
      return Status(ErrorCode::kUnsupported,
                    "CUDA evaluation rejects external expression nodes");
    }
    return internal_dense_cuda::CudaEvaluateErased(
        context, operation, left, right,
        internal_dense_cuda::Describe(destination));
  }
}

using MatrixOperation = DenseTranspose;

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context, DenseView<const float, 1> source,
    DenseView<float, 1> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context, DenseView<const float, 2> source,
    DenseView<float, 2> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context, DenseView<const double, 1> source,
    DenseView<double, 1> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context, DenseView<const double, 2> source,
    DenseView<double, 2> destination);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, float alpha, DenseView<float, 1> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, float alpha, DenseView<float, 2> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, double alpha, DenseView<double, 1> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, double alpha, DenseView<double, 2> destination);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, float alpha, DenseView<const float, 1> source,
    DenseView<float, 1> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, float alpha, DenseView<const float, 2> source,
    DenseView<float, 2> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, double alpha, DenseView<const double, 1> source,
    DenseView<double, 1> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, double alpha, DenseView<const double, 2> source,
    DenseView<double, 2> destination);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGemv(
    DenseCudaContext& context, MatrixOperation operation, float alpha,
    DenseView<const float, 2> matrix, DenseView<const float, 1> input,
    float beta, DenseView<float, 1> output);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGemv(
    DenseCudaContext& context, MatrixOperation operation, double alpha,
    DenseView<const double, 2> matrix, DenseView<const double, 1> input,
    double beta, DenseView<double, 1> output);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGemm(
    DenseCudaContext& context, MatrixOperation left_operation,
    MatrixOperation right_operation, float alpha,
    DenseView<const float, 2> left, DenseView<const float, 2> right, float beta,
    DenseView<float, 2> output);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGemm(
    DenseCudaContext& context, MatrixOperation left_operation,
    MatrixOperation right_operation, double alpha,
    DenseView<const double, 2> left, DenseView<const double, 2> right,
    double beta, DenseView<double, 2> output);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_CUDA_H_
