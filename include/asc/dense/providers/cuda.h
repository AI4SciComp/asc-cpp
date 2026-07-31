#ifndef ASC_DENSE_PROVIDERS_CUDA_H_
#define ASC_DENSE_PROVIDERS_CUDA_H_

#include <array>
#include <complex>
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
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

using MatrixOperation = DenseBlasTranspose;

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotg(
    DenseCudaContext& context, DenseBlasVectorView<float> a,
    DenseBlasVectorView<float> b, DenseBlasVectorView<float> c,
    DenseBlasVectorView<float> s);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotg(
    DenseCudaContext& context, DenseBlasVectorView<double> a,
    DenseBlasVectorView<double> b, DenseBlasVectorView<double> c,
    DenseBlasVectorView<double> s);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotg(
    DenseCudaContext& context, DenseBlasVectorView<std::complex<float>> a,
    DenseBlasVectorView<const std::complex<float>> b,
    DenseBlasVectorView<float> c, DenseBlasVectorView<std::complex<float>> s);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotg(
    DenseCudaContext& context, DenseBlasVectorView<std::complex<double>> a,
    DenseBlasVectorView<const std::complex<double>> b,
    DenseBlasVectorView<double> c, DenseBlasVectorView<std::complex<double>> s);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotmg(
    DenseCudaContext& context, DenseBlasVectorView<float> d1,
    DenseBlasVectorView<float> d2, DenseBlasVectorView<float> x1,
    DenseBlasVectorView<const float> y1, DenseBlasVectorView<float> parameters);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotmg(
    DenseCudaContext& context, DenseBlasVectorView<double> d1,
    DenseBlasVectorView<double> d2, DenseBlasVectorView<double> x1,
    DenseBlasVectorView<const double> y1,
    DenseBlasVectorView<double> parameters);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRot(
    DenseCudaContext& context, DenseBlasVectorView<float> x,
    DenseBlasVectorView<float> y, float c, float s);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRot(
    DenseCudaContext& context, DenseBlasVectorView<double> x,
    DenseBlasVectorView<double> y, double c, double s);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRot(
    DenseCudaContext& context, DenseBlasVectorView<std::complex<float>> x,
    DenseBlasVectorView<std::complex<float>> y, float c, float s);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRot(
    DenseCudaContext& context, DenseBlasVectorView<std::complex<double>> x,
    DenseBlasVectorView<std::complex<double>> y, double c, double s);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotm(
    DenseCudaContext& context, DenseBlasVectorView<float> x,
    DenseBlasVectorView<float> y, DenseBlasVectorView<const float> parameters);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaRotm(
    DenseCudaContext& context, DenseBlasVectorView<double> x,
    DenseBlasVectorView<double> y,
    DenseBlasVectorView<const double> parameters);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSwap(
    DenseCudaContext& context, DenseBlasVectorView<float> x,
    DenseBlasVectorView<float> y);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSwap(
    DenseCudaContext& context, DenseBlasVectorView<double> x,
    DenseBlasVectorView<double> y);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSwap(
    DenseCudaContext& context, DenseBlasVectorView<std::complex<float>> x,
    DenseBlasVectorView<std::complex<float>> y);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSwap(
    DenseCudaContext& context, DenseBlasVectorView<std::complex<double>> x,
    DenseBlasVectorView<std::complex<double>> y);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, float alpha,
    DenseBlasVectorView<float> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, double alpha,
    DenseBlasVectorView<double> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, std::complex<float> alpha,
    DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, std::complex<double> alpha,
    DenseBlasVectorView<std::complex<double>> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, float alpha,
    DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaScal(
    DenseCudaContext& context, double alpha,
    DenseBlasVectorView<std::complex<double>> destination);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context, DenseBlasVectorView<const float> source,
    DenseBlasVectorView<float> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context, DenseBlasVectorView<const double> source,
    DenseBlasVectorView<double> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<float>> source,
    DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaCopy(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<double>> source,
    DenseBlasVectorView<std::complex<double>> destination);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, float alpha,
    DenseBlasVectorView<const float> source,
    DenseBlasVectorView<float> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, double alpha,
    DenseBlasVectorView<const double> source,
    DenseBlasVectorView<double> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, std::complex<float> alpha,
    DenseBlasVectorView<const std::complex<float>> source,
    DenseBlasVectorView<std::complex<float>> destination);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAxpy(
    DenseCudaContext& context, std::complex<double> alpha,
    DenseBlasVectorView<const std::complex<double>> source,
    DenseBlasVectorView<std::complex<double>> destination);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDot(
    DenseCudaContext& context, DenseBlasVectorView<const float> left,
    DenseBlasVectorView<const float> right, DenseBlasVectorView<float> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDot(
    DenseCudaContext& context, DenseBlasVectorView<const double> left,
    DenseBlasVectorView<const double> right,
    DenseBlasVectorView<double> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDot(
    DenseCudaContext& context, float bias,
    DenseBlasVectorView<const float> left,
    DenseBlasVectorView<const float> right, DenseBlasVectorView<float> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDot(
    DenseCudaContext& context, DenseBlasDotAccumulation accumulation,
    DenseBlasVectorView<const float> left,
    DenseBlasVectorView<const float> right, DenseBlasVectorView<double> result);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDotu(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<float>> left,
    DenseBlasVectorView<const std::complex<float>> right,
    DenseBlasVectorView<std::complex<float>> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDotu(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<double>> left,
    DenseBlasVectorView<const std::complex<double>> right,
    DenseBlasVectorView<std::complex<double>> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDotc(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<float>> left,
    DenseBlasVectorView<const std::complex<float>> right,
    DenseBlasVectorView<std::complex<float>> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaDotc(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<double>> left,
    DenseBlasVectorView<const std::complex<double>> right,
    DenseBlasVectorView<std::complex<double>> result);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaNrm2(
    DenseCudaContext& context, DenseBlasVectorView<const float> operand,
    DenseBlasVectorView<float> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaNrm2(
    DenseCudaContext& context, DenseBlasVectorView<const double> operand,
    DenseBlasVectorView<double> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaNrm2(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<float>> operand,
    DenseBlasVectorView<float> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaNrm2(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<double>> operand,
    DenseBlasVectorView<double> result);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAsum(
    DenseCudaContext& context, DenseBlasVectorView<const float> operand,
    DenseBlasVectorView<float> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAsum(
    DenseCudaContext& context, DenseBlasVectorView<const double> operand,
    DenseBlasVectorView<double> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAsum(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<float>> operand,
    DenseBlasVectorView<float> result);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaAsum(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<double>> operand,
    DenseBlasVectorView<double> result);

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaIamax(
    DenseCudaContext& context, DenseBlasVectorView<const float> operand,
    DenseBlasVectorView<index_t> result, MutableMemoryView provider_workspace);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaIamax(
    DenseCudaContext& context, DenseBlasVectorView<const double> operand,
    DenseBlasVectorView<index_t> result, MutableMemoryView provider_workspace);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaIamax(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<float>> operand,
    DenseBlasVectorView<index_t> result, MutableMemoryView provider_workspace);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaIamax(
    DenseCudaContext& context,
    DenseBlasVectorView<const std::complex<double>> operand,
    DenseBlasVectorView<index_t> result, MutableMemoryView provider_workspace);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGemv(
    DenseCudaContext& context, DenseBlasTranspose transpose, Element alpha,
    DenseBlasMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGbmv(
    DenseCudaContext& context, DenseBlasTranspose transpose, Element alpha,
    DenseBlasBandMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHemv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHbmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHpmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSymv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSbmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSpmv(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<const Element> input, Element beta,
    DenseBlasVectorView<Element> output);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTrmv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const Element> matrix,
    DenseBlasVectorView<Element> vector);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTbmv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<Element> vector);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTpmv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<Element> vector);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTrsv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const Element> matrix,
    DenseBlasVectorView<Element> vector);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTbsv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasVectorView<Element> vector);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTpsv(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<const Element> matrix,
    DenseBlasVectorView<Element> vector);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGer(
    DenseCudaContext& context, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGeru(
    DenseCudaContext& context, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGerc(
    DenseCudaContext& context, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHer(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasRealType<Element> alpha, DenseBlasVectorView<const Element> x,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHpr(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasRealType<Element> alpha, DenseBlasVectorView<const Element> x,
    DenseBlasPackedMatrixView<Element> matrix);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHer2(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHpr2(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasPackedMatrixView<Element> matrix);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSyr(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasMatrixView<Element> matrix);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSpr(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasVectorView<const Element> x,
    DenseBlasPackedMatrixView<Element> matrix);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSyr2(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasReal Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSpr2(
    DenseCudaContext& context, DenseBlasTriangle triangle, Element alpha,
    DenseBlasVectorView<const Element> x, DenseBlasVectorView<const Element> y,
    DenseBlasPackedMatrixView<Element> matrix);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaGemm(
    DenseCudaContext& context, DenseBlasTranspose left_transpose,
    DenseBlasTranspose right_transpose, Element alpha,
    DenseBlasMatrixView<const Element> left,
    DenseBlasMatrixView<const Element> right, Element beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSymm(
    DenseCudaContext& context, DenseBlasSide side, DenseBlasTriangle triangle,
    Element alpha, DenseBlasMatrixView<const Element> symmetric,
    DenseBlasMatrixView<const Element> other, Element beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHemm(
    DenseCudaContext& context, DenseBlasSide side, DenseBlasTriangle triangle,
    Element alpha, DenseBlasMatrixView<const Element> hermitian,
    DenseBlasMatrixView<const Element> other, Element beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSyrk(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, Element alpha,
    DenseBlasMatrixView<const Element> input, Element beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHerk(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasRealType<Element> alpha,
    DenseBlasMatrixView<const Element> input, DenseBlasRealType<Element> beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaSyr2k(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, Element alpha,
    DenseBlasMatrixView<const Element> left,
    DenseBlasMatrixView<const Element> right, Element beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasComplex Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaHer2k(
    DenseCudaContext& context, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, Element alpha,
    DenseBlasMatrixView<const Element> left,
    DenseBlasMatrixView<const Element> right, DenseBlasRealType<Element> beta,
    DenseBlasMatrixView<Element> output);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTrmm(
    DenseCudaContext& context, DenseBlasSide side, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal, Element alpha,
    DenseBlasMatrixView<const Element> triangular,
    DenseBlasMatrixView<Element> matrix);

template <DenseBlasScalar Element>
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> CudaTrsm(
    DenseCudaContext& context, DenseBlasSide side, DenseBlasTriangle triangle,
    DenseBlasTranspose transpose, DenseBlasDiagonal diagonal, Element alpha,
    DenseBlasMatrixView<const Element> triangular,
    DenseBlasMatrixView<Element> matrix);

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
