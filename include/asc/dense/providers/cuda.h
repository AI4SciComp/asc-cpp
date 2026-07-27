#ifndef ASC_DENSE_PROVIDERS_CUDA_H_
#define ASC_DENSE_PROVIDERS_CUDA_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/linalg.h"
#include "asc/dense/providers/cuda_export.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"

namespace asc {

namespace internal_dense_cuda {

class DenseCudaContextState;
class DenseCudaContextAccess;

enum class ScalarType : std::uint8_t {
  kFloat = 0,
  kDouble = 1,
};

enum class PointwiseOperation : std::uint8_t {
  kCopy = 0,
  kFill = 1,
  kNegate = 2,
  kAdd = 3,
  kSubtract = 4,
  kMultiply = 5,
};

struct PointwiseOperand {
  const void* data = nullptr;
  std::array<stride_t, 8> strides{};
  double scalar_value = 0;
  bool is_scalar = false;
};

struct PointwisePlan {
  PointwiseOperation operation = PointwiseOperation::kCopy;
  ScalarType scalar_type = ScalarType::kFloat;
  std::uint8_t rank = 0;
  std::array<extent_t, 8> shape{};
  std::array<stride_t, 8> destination_strides{};
  void* destination = nullptr;
  PointwiseOperand left;
  PointwiseOperand right;
  extent_t logical_size = 0;
  bool no_op = false;
};

enum class LinalgOperation : std::uint8_t {
  kCopy = 0,
  kScal = 1,
  kAxpy = 2,
  kGemv = 3,
  kGemm = 4,
  kScaleOrZero = 5,
};

struct LinalgOperand {
  const void* data = nullptr;
  std::array<extent_t, 2> shape{};
  std::array<stride_t, 2> strides{};
};

struct LinalgPlan {
  LinalgOperation operation = LinalgOperation::kCopy;
  ScalarType scalar_type = ScalarType::kFloat;
  std::uint8_t rank = 0;
  LinalgOperand left;
  LinalgOperand right;
  void* destination = nullptr;
  std::array<extent_t, 2> destination_shape{};
  std::array<stride_t, 2> destination_strides{};
  double alpha = 0;
  double beta = 0;
  MatrixOperation left_operation = MatrixOperation::kNone;
  MatrixOperation right_operation = MatrixOperation::kNone;
  extent_t logical_size = 0;
  bool no_op = false;
};

}  // namespace internal_dense_cuda

class ASC_DENSE_CUDA_EXPORT DenseCudaContext {
 public:
  static Result<DenseCudaContext> Create(ExecutionContext execution_context);

  DenseCudaContext(const DenseCudaContext&) = delete;
  DenseCudaContext& operator=(const DenseCudaContext&) = delete;
  DenseCudaContext(DenseCudaContext&& other) noexcept;
  DenseCudaContext& operator=(DenseCudaContext&& other) noexcept;
  ~DenseCudaContext();

  [[nodiscard]] const ExecutionContext& execution_context() const noexcept;

 private:
  friend class internal_dense_cuda::DenseCudaContextAccess;

  explicit DenseCudaContext(
      ExecutionContext execution_context,
      std::unique_ptr<internal_dense_cuda::DenseCudaContextState>
          state) noexcept;

  ExecutionContext execution_context_;
  std::unique_ptr<internal_dense_cuda::DenseCudaContextState> state_;
};

namespace internal_dense_cuda {

ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> LaunchPointwiseOpaque(
    const DenseCudaContext& context, const void* plan);
ASC_DENSE_CUDA_EXPORT Result<CompletionEvent> LaunchLinalgOpaque(
    const DenseCudaContext& context, const void* plan);

template <typename Scalar>
  requires std::same_as<Scalar, float> || std::same_as<Scalar, double>
constexpr ScalarType ScalarTypeFor() noexcept {
  if constexpr (std::same_as<Scalar, float>) {
    return ScalarType::kFloat;
  }
  return ScalarType::kDouble;
}

template <DenseElement Element, std::size_t Rank>
Status SetDestination(DenseView<Element, Rank> destination,
                      PointwisePlan& plan) {
  if constexpr (Rank > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation supports rank zero through eight");
  } else {
    if (destination.space() != MemorySpace::kDevice) {
      return Status(ErrorCode::kMemoryAccess,
                    "CUDA evaluation requires a device destination");
    }
    if (!destination.mapping().is_unique()) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA evaluation requires a unique destination");
    }
    plan.scalar_type = ScalarTypeFor<Element>();
    plan.rank = static_cast<std::uint8_t>(Rank);
    plan.destination = destination.data();
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      plan.shape[dimension] = destination.shape()[dimension];
      plan.destination_strides[dimension] =
          destination.mapping().strides()[dimension];
    }
    return Status::Ok();
  }
}

template <DenseElement Element, std::size_t Rank, DenseElement SourceElement,
          std::size_t SourceRank>
Status SetTerminal(DenseView<Element, Rank> destination,
                   DenseView<SourceElement, SourceRank> source,
                   PointwiseOperand& operand, bool allow_exact_no_op,
                   bool& no_op) {
  if constexpr (Rank != SourceRank ||
                !std::same_as<std::remove_const_t<SourceElement>, Element>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation requires matching terminal type and rank");
  } else {
    if (source.space() != MemorySpace::kDevice) {
      return Status(ErrorCode::kMemoryAccess,
                    "CUDA evaluation requires device terminals");
    }
    if (!source.mapping().is_unique()) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA evaluation requires unique terminals");
    }
    if (source.shape() != destination.shape()) {
      return Status(ErrorCode::kShape,
                    "CUDA evaluation requires exact terminal shape");
    }
    if (destination.IsExactView(source) && allow_exact_no_op) {
      no_op = true;
    } else if (source.MayOverlap(destination)) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA evaluation rejects destination overlap");
    }
    operand.data = source.data();
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      operand.strides[dimension] = source.mapping().strides()[dimension];
    }
    return Status::Ok();
  }
}

template <DenseElement Element, std::size_t Rank, typename Scalar>
Status SetTerminal(DenseView<Element, Rank>,
                   const ScalarExpression<Scalar>& scalar,
                   PointwiseOperand& operand, bool, bool&) {
  if constexpr (!std::same_as<Scalar, Element>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation requires an exact scalar type");
  } else {
    operand.is_scalar = true;
    operand.scalar_value = static_cast<double>(scalar.value());
    return Status::Ok();
  }
}

template <DenseElement Element, std::size_t Rank, typename Expression>
Status SetTerminal(DenseView<Element, Rank>, const Expression&,
                   PointwiseOperand&, bool, bool&) {
  return Status(
      ErrorCode::kUnsupported,
      "CUDA evaluation supports only dense terminals and rank-zero scalars");
}

template <typename T>
struct IsDenseTerminal : std::false_type {};

template <DenseElement Element, std::size_t Rank>
struct IsDenseTerminal<DenseView<Element, Rank>> : std::true_type {};

template <DenseElement Element, std::size_t Rank, DenseElement SourceElement,
          std::size_t SourceRank>
Status ConfigureExpression(DenseView<Element, Rank> destination,
                           DenseView<SourceElement, SourceRank> source,
                           PointwisePlan& plan) {
  plan.operation = PointwiseOperation::kCopy;
  return SetTerminal(destination, source, plan.left, true, plan.no_op);
}

template <DenseElement Element, std::size_t Rank, typename Scalar>
Status ConfigureExpression(DenseView<Element, Rank>,
                           const ScalarExpression<Scalar>& scalar,
                           PointwisePlan& plan) {
  if constexpr (!std::same_as<Scalar, Element>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation requires an exact scalar type");
  } else {
    plan.operation = PointwiseOperation::kFill;
    plan.left.is_scalar = true;
    plan.left.scalar_value = static_cast<double>(scalar.value());
    return Status::Ok();
  }
}

template <DenseElement Element, std::size_t Rank, typename Operation,
          typename Operand>
Status ConfigureExpression(
    DenseView<Element, Rank> destination,
    const UnaryExpression<Operation, Operand>& expression,
    PointwisePlan& plan) {
  if constexpr (!std::same_as<Operation, NegateOperation>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation does not support this unary operation");
  } else {
    using Terminal = std::remove_cvref_t<decltype(expression.operand().get())>;
    if constexpr (!IsDenseTerminal<Terminal>::value) {
      return Status(ErrorCode::kUnsupported,
                    "CUDA negate supports only a dense terminal operand");
    }
    plan.operation = PointwiseOperation::kNegate;
    return SetTerminal(destination, expression.operand().get(), plan.left,
                       false, plan.no_op);
  }
}

template <DenseElement Element, std::size_t Rank, typename Operation,
          typename Left, typename Right>
Status ConfigureExpression(
    DenseView<Element, Rank> destination,
    const BinaryExpression<Operation, Left, Right>& expression,
    PointwisePlan& plan) {
  if constexpr (std::same_as<Operation, AddOperation>) {
    plan.operation = PointwiseOperation::kAdd;
  } else if constexpr (std::same_as<Operation, SubtractOperation>) {
    plan.operation = PointwiseOperation::kSubtract;
  } else if constexpr (std::same_as<Operation, MultiplyOperation>) {
    plan.operation = PointwiseOperation::kMultiply;
  } else {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation does not support this binary operation");
  }
  Status status = SetTerminal(destination, expression.left().get(), plan.left,
                              false, plan.no_op);
  if (!status.ok()) {
    return status;
  }
  return SetTerminal(destination, expression.right().get(), plan.right, false,
                     plan.no_op);
}

template <DenseElement Element, std::size_t Rank, typename Expression>
Status ConfigureExpression(DenseView<Element, Rank>, const Expression&,
                           PointwisePlan&) {
  return Status(ErrorCode::kUnsupported,
                "CUDA evaluation does not support this expression");
}

template <DenseElement Element, std::size_t Rank>
LinalgOperand MakeOperand(DenseView<Element, Rank> view) {
  LinalgOperand operand;
  operand.data = view.data();
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    operand.shape[dimension] = view.shape()[dimension];
    operand.strides[dimension] = view.mapping().strides()[dimension];
  }
  return operand;
}

template <DenseElement Element, std::size_t Rank>
void SetLinalgDestination(DenseView<Element, Rank> view, LinalgPlan& plan) {
  plan.destination = view.data();
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    plan.destination_shape[dimension] = view.shape()[dimension];
    plan.destination_strides[dimension] = view.mapping().strides()[dimension];
  }
}

template <DenseElement Element, std::size_t Rank>
Status ValidateCudaLinalgView(DenseView<Element, Rank> view) {
  if (view.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA dense algebra requires device views");
  }
  if (!view.mapping().is_unique()) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA dense algebra requires unique views");
  }
  return Status::Ok();
}

}  // namespace internal_dense_cuda

template <DenseElement Element, std::size_t Rank, ReadableExpression Expression>
  requires(!std::is_const_v<Element>) && DenseLinearAlgebraScalar<Element> &&
          std::same_as<Element, ExpressionValue<Expression>>
Result<CompletionEvent> CudaEvaluate(const DenseCudaContext& context,
                                     DenseView<Element, Rank> destination,
                                     const Expression& expression) {
  internal_dense_cuda::PointwisePlan plan;
  const Status destination_status =
      internal_dense_cuda::SetDestination(destination, plan);
  if (!destination_status.ok()) {
    return destination_status;
  }
  const Status expression_status =
      internal_dense_cuda::ConfigureExpression(destination, expression, plan);
  if (!expression_status.ok()) {
    return expression_status;
  }
  return internal_dense_cuda::LaunchPointwiseOpaque(context, &plan);
}

template <DenseElement SourceElement, DenseLinearAlgebraScalar Scalar,
          std::size_t Rank>
  requires(Rank == 1 || Rank == 2) &&
          std::same_as<std::remove_const_t<SourceElement>, Scalar>
Result<CompletionEvent> CudaCopy(const DenseCudaContext& context,
                                 DenseView<SourceElement, Rank> source,
                                 DenseView<Scalar, Rank> destination) {
  Status status = internal_dense_cuda::ValidateCudaLinalgView(source);
  if (!status.ok()) {
    return status;
  }
  status = internal_dense_cuda::ValidateCudaLinalgView(destination);
  if (!status.ok()) {
    return status;
  }
  internal_dense_cuda::LinalgPlan plan;
  plan.operation = internal_dense_cuda::LinalgOperation::kCopy;
  plan.scalar_type = internal_dense_cuda::ScalarTypeFor<Scalar>();
  plan.rank = static_cast<std::uint8_t>(Rank);
  plan.left = internal_dense_cuda::MakeOperand(source);
  internal_dense_cuda::SetLinalgDestination(destination, plan);
  plan.no_op = destination.IsExactView(source);
  return internal_dense_cuda::LaunchLinalgOpaque(context, &plan);
}

template <DenseLinearAlgebraScalar Scalar, std::size_t Rank>
  requires(Rank == 1 || Rank == 2)
Result<CompletionEvent> CudaScal(const DenseCudaContext& context, Scalar alpha,
                                 DenseView<Scalar, Rank> destination) {
  const Status status =
      internal_dense_cuda::ValidateCudaLinalgView(destination);
  if (!status.ok()) {
    return status;
  }
  internal_dense_cuda::LinalgPlan plan;
  plan.operation = internal_dense_cuda::LinalgOperation::kScal;
  plan.scalar_type = internal_dense_cuda::ScalarTypeFor<Scalar>();
  plan.rank = static_cast<std::uint8_t>(Rank);
  plan.alpha = static_cast<double>(alpha);
  internal_dense_cuda::SetLinalgDestination(destination, plan);
  return internal_dense_cuda::LaunchLinalgOpaque(context, &plan);
}

template <DenseElement SourceElement, DenseLinearAlgebraScalar Scalar,
          std::size_t Rank>
  requires(Rank == 1 || Rank == 2) &&
          std::same_as<std::remove_const_t<SourceElement>, Scalar>
Result<CompletionEvent> CudaAxpy(const DenseCudaContext& context, Scalar alpha,
                                 DenseView<SourceElement, Rank> source,
                                 DenseView<Scalar, Rank> destination) {
  Status status = internal_dense_cuda::ValidateCudaLinalgView(source);
  if (!status.ok()) {
    return status;
  }
  status = internal_dense_cuda::ValidateCudaLinalgView(destination);
  if (!status.ok()) {
    return status;
  }
  internal_dense_cuda::LinalgPlan plan;
  plan.operation = internal_dense_cuda::LinalgOperation::kAxpy;
  plan.scalar_type = internal_dense_cuda::ScalarTypeFor<Scalar>();
  plan.rank = static_cast<std::uint8_t>(Rank);
  plan.left = internal_dense_cuda::MakeOperand(source);
  plan.alpha = static_cast<double>(alpha);
  internal_dense_cuda::SetLinalgDestination(destination, plan);
  return internal_dense_cuda::LaunchLinalgOpaque(context, &plan);
}

template <DenseElement MatrixElement, DenseElement InputElement,
          DenseLinearAlgebraScalar Scalar>
  requires std::same_as<std::remove_const_t<MatrixElement>, Scalar> &&
           std::same_as<std::remove_const_t<InputElement>, Scalar>
Result<CompletionEvent> CudaGemv(const DenseCudaContext& context,
                                 MatrixOperation operation, Scalar alpha,
                                 DenseView<MatrixElement, 2> matrix,
                                 DenseView<InputElement, 1> input, Scalar beta,
                                 DenseView<Scalar, 1> output) {
  Status status = internal_dense_cuda::ValidateCudaLinalgView(matrix);
  if (!status.ok()) {
    return status;
  }
  status = internal_dense_cuda::ValidateCudaLinalgView(input);
  if (!status.ok()) {
    return status;
  }
  status = internal_dense_cuda::ValidateCudaLinalgView(output);
  if (!status.ok()) {
    return status;
  }
  internal_dense_cuda::LinalgPlan plan;
  plan.operation = internal_dense_cuda::LinalgOperation::kGemv;
  plan.scalar_type = internal_dense_cuda::ScalarTypeFor<Scalar>();
  plan.rank = 1;
  plan.left_operation = operation;
  plan.left = internal_dense_cuda::MakeOperand(matrix);
  plan.right = internal_dense_cuda::MakeOperand(input);
  plan.alpha = static_cast<double>(alpha);
  plan.beta = static_cast<double>(beta);
  internal_dense_cuda::SetLinalgDestination(output, plan);
  return internal_dense_cuda::LaunchLinalgOpaque(context, &plan);
}

template <DenseElement LeftElement, DenseElement RightElement,
          DenseLinearAlgebraScalar Scalar>
  requires std::same_as<std::remove_const_t<LeftElement>, Scalar> &&
           std::same_as<std::remove_const_t<RightElement>, Scalar>
Result<CompletionEvent> CudaGemm(const DenseCudaContext& context,
                                 MatrixOperation left_operation,
                                 MatrixOperation right_operation, Scalar alpha,
                                 DenseView<LeftElement, 2> left,
                                 DenseView<RightElement, 2> right, Scalar beta,
                                 DenseView<Scalar, 2> output) {
  Status status = internal_dense_cuda::ValidateCudaLinalgView(left);
  if (!status.ok()) {
    return status;
  }
  status = internal_dense_cuda::ValidateCudaLinalgView(right);
  if (!status.ok()) {
    return status;
  }
  status = internal_dense_cuda::ValidateCudaLinalgView(output);
  if (!status.ok()) {
    return status;
  }
  internal_dense_cuda::LinalgPlan plan;
  plan.operation = internal_dense_cuda::LinalgOperation::kGemm;
  plan.scalar_type = internal_dense_cuda::ScalarTypeFor<Scalar>();
  plan.rank = 2;
  plan.left_operation = left_operation;
  plan.right_operation = right_operation;
  plan.left = internal_dense_cuda::MakeOperand(left);
  plan.right = internal_dense_cuda::MakeOperand(right);
  plan.alpha = static_cast<double>(alpha);
  plan.beta = static_cast<double>(beta);
  internal_dense_cuda::SetLinalgDestination(output, plan);
  return internal_dense_cuda::LaunchLinalgOpaque(context, &plan);
}

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_CUDA_H_
