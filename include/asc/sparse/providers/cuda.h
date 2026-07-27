#ifndef ASC_SPARSE_PROVIDERS_CUDA_H_
#define ASC_SPARSE_PROVIDERS_CUDA_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/providers/cuda_export.h"

namespace asc {
namespace internal_sparse_cuda {

class SparseCudaContextState;
class SparseCudaContextAccess;

enum class ScalarType : std::uint8_t {
  kFloat = 0,
  kDouble = 1,
};

enum class SparseFormat : std::uint8_t {
  kCoordinate = 0,
  kCsr = 1,
  kCsc = 2,
};

enum class PointwiseOperation : std::uint8_t {
  kCopy = 0,
  kNegate = 1,
  kAdd = 2,
  kSubtract = 3,
  kMultiply = 4,
};

struct PointwiseOperand {
  const void* values = nullptr;
  double scalar = 0;
  bool is_scalar = false;
};

struct PointwisePlan {
  PointwiseOperation operation = PointwiseOperation::kCopy;
  ScalarType scalar_type = ScalarType::kFloat;
  SparseFormat format = SparseFormat::kCoordinate;
  std::uint8_t rank = 0;
  extent_t shape[8]{};
  nnz_t nnz = 0;
  const void* first_structure = nullptr;
  const void* second_structure = nullptr;
  void* destination = nullptr;
  PointwiseOperand left;
  PointwiseOperand right;
  bool no_op = false;
};

struct SpmvPlan {
  ScalarType scalar_type = ScalarType::kFloat;
  const nnz_t* outer_offsets = nullptr;
  const index_t* inner_indices = nullptr;
  const void* values = nullptr;
  extent_t rows = 0;
  extent_t columns = 0;
  nnz_t nnz = 0;
  const void* input = nullptr;
  stride_t input_stride = 1;
  void* output = nullptr;
  stride_t output_stride = 1;
  double alpha = 0;
  double beta = 0;
  MutableMemoryView workspace{nullptr, 0, MemorySpace::kDevice};
};

struct ClonePlan {
  const nnz_t* source_outer_offsets = nullptr;
  const index_t* source_inner_indices = nullptr;
  const void* source_values = nullptr;
  nnz_t* destination_outer_offsets = nullptr;
  index_t* destination_inner_indices = nullptr;
  void* destination_values = nullptr;
  std::size_t outer_bytes = 0;
  std::size_t inner_bytes = 0;
  std::size_t value_bytes = 0;
};

}  // namespace internal_sparse_cuda

class ASC_SPARSE_CUDA_EXPORT SparseCudaContext {
 public:
  // Creates a move-only cuSPARSE context on the supplied CUDA stream. The
  // ExecutionContext is shared by value and remains valid for this context's
  // lifetime. Destruction does not synchronize the device.
  static Result<SparseCudaContext> Create(ExecutionContext execution_context);

  SparseCudaContext(const SparseCudaContext&) = delete;
  SparseCudaContext& operator=(const SparseCudaContext&) = delete;
  SparseCudaContext(SparseCudaContext&& other) noexcept;
  SparseCudaContext& operator=(SparseCudaContext&& other) noexcept;
  ~SparseCudaContext();

  [[nodiscard]] const ExecutionContext& execution_context() const noexcept;

 private:
  friend class internal_sparse_cuda::SparseCudaContextAccess;

  SparseCudaContext(
      ExecutionContext execution_context,
      std::unique_ptr<internal_sparse_cuda::SparseCudaContextState> state);

  ExecutionContext execution_context_;
  std::unique_ptr<internal_sparse_cuda::SparseCudaContextState> state_;
};

// A non-owning, provider-neutral descriptor for a positive-stride device
// vector. The described allocation must remain alive through every operation
// that uses it.
template <typename Element>
  requires std::same_as<std::remove_const_t<Element>, float> ||
           std::same_as<std::remove_const_t<Element>, double>
class CudaStridedVectorView {
 public:
  static Result<CudaStridedVectorView> Create(Element* data, extent_t extent,
                                              stride_t stride,
                                              MemorySpace space) {
    if (extent < 0) {
      return Status(ErrorCode::kShape,
                    "A CUDA strided vector extent cannot be negative");
    }
    if (stride <= 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A CUDA strided vector stride must be positive");
    }
    if (space != MemorySpace::kDevice) {
      return Status(ErrorCode::kMemoryAccess,
                    "A CUDA strided vector requires device memory");
    }
    if (extent != 0 && data == nullptr) {
      return Status(ErrorCode::kMemoryAccess,
                    "A nonempty CUDA strided vector cannot be null");
    }
    return CudaStridedVectorView(data, extent, stride);
  }

  template <typename MutableElement>
    requires std::is_const_v<Element> &&
                 std::same_as<std::remove_const_t<Element>, MutableElement>
  constexpr CudaStridedVectorView(
      CudaStridedVectorView<MutableElement> mutable_view) noexcept
      : data_(mutable_view.data()),
        extent_(mutable_view.extent()),
        stride_(mutable_view.stride()) {}

  [[nodiscard]] constexpr Element* data() const noexcept { return data_; }
  [[nodiscard]] constexpr extent_t extent() const noexcept { return extent_; }
  [[nodiscard]] constexpr stride_t stride() const noexcept { return stride_; }
  [[nodiscard]] static constexpr MemorySpace space() noexcept {
    return MemorySpace::kDevice;
  }

 private:
  constexpr CudaStridedVectorView(Element* data, extent_t extent,
                                  stride_t stride) noexcept
      : data_(data), extent_(extent), stride_(stride) {}

  Element* data_;
  extent_t extent_;
  stride_t stride_;
};

template <typename Element>
struct CudaCsrClone {
  CsrArray<Element> array;
  CompletionEvent completion;
};

namespace internal_sparse_cuda {

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> CloneCsrOpaque(
    const SparseCudaContext& context, const void* plan);
ASC_SPARSE_CUDA_EXPORT Result<std::size_t> QuerySpmvWorkspaceOpaque(
    const SparseCudaContext& context, const void* plan);
ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> LaunchSpmvOpaque(
    const SparseCudaContext& context, const void* plan);
ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> LaunchPointwiseOpaque(
    const SparseCudaContext& context, const void* plan);

template <typename Element>
constexpr ScalarType ScalarTypeFor() noexcept {
  return std::same_as<std::remove_const_t<Element>, float>
             ? ScalarType::kFloat
             : ScalarType::kDouble;
}

template <SparseElement Element, std::size_t Rank>
void SetStructure(CoordinateView<Element, Rank> view, PointwisePlan& plan) {
  plan.format = SparseFormat::kCoordinate;
  plan.rank = static_cast<std::uint8_t>(Rank);
  plan.first_structure = view.coordinate_data();
}

template <SparseElement Element, SparseCompressedFormat Format>
void SetStructure(CompressedSparseView<Element, Format> view,
                  PointwisePlan& plan) {
  plan.format = Format == SparseCompressedFormat::kCsr ? SparseFormat::kCsr
                                                       : SparseFormat::kCsc;
  plan.rank = 2;
  plan.first_structure = view.outer_offset_data();
  plan.second_structure = view.inner_index_data();
}

template <typename Destination, typename Source>
Status SetTerminal(const Destination&, const Source&, PointwiseOperand&, bool,
                   bool&) {
  return Status(ErrorCode::kUnsupported,
                "CUDA sparse evaluation requires same-structure terminals");
}

template <SparseElement Element, std::size_t Rank, SparseElement SourceElement,
          std::size_t SourceRank>
Status SetTerminal(CoordinateView<Element, Rank> destination,
                   CoordinateView<SourceElement, SourceRank> source,
                   PointwiseOperand& operand, bool exact_no_op, bool& no_op) {
  if constexpr (Rank != SourceRank ||
                !std::same_as<std::remove_const_t<SourceElement>, Element>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse terminal type or rank differs");
  } else {
    if (!internal_sparse_coordinate::ViewAccess::Trusted(source) ||
        source.space() != MemorySpace::kDevice ||
        source.shape() != destination.shape() ||
        source.nnz() != destination.nnz() ||
        source.coordinate_data() != destination.coordinate_data()) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse terminals must have identical structure");
    }
    if (source.value_data() == destination.value_data()) {
      no_op = exact_no_op;
    } else if (internal_sparse_coordinate::ByteSpansOverlap(
                   source.value_data(),
                   static_cast<std::size_t>(source.nnz()) * sizeof(Element),
                   destination.value_data(),
                   static_cast<std::size_t>(destination.nnz()) *
                       sizeof(Element))) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse evaluation rejects value overlap");
    }
    operand.values = source.value_data();
    return Status::Ok();
  }
}

template <SparseElement Element, SparseCompressedFormat Format,
          SparseElement SourceElement, SparseCompressedFormat SourceFormat>
Status SetTerminal(CompressedSparseView<Element, Format> destination,
                   CompressedSparseView<SourceElement, SourceFormat> source,
                   PointwiseOperand& operand, bool exact_no_op, bool& no_op) {
  if constexpr (Format != SourceFormat ||
                !std::same_as<std::remove_const_t<SourceElement>, Element>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse terminal type or format differs");
  } else {
    if (!internal_sparse_compressed::ViewAccess::Trusted(source) ||
        source.space() != MemorySpace::kDevice ||
        source.shape() != destination.shape() ||
        source.nnz() != destination.nnz() ||
        source.outer_offset_data() != destination.outer_offset_data() ||
        source.inner_index_data() != destination.inner_index_data()) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse terminals must have identical structure");
    }
    if (source.value_data() == destination.value_data()) {
      no_op = exact_no_op;
    } else if (internal_sparse_coordinate::ByteSpansOverlap(
                   source.value_data(),
                   static_cast<std::size_t>(source.nnz()) * sizeof(Element),
                   destination.value_data(),
                   static_cast<std::size_t>(destination.nnz()) *
                       sizeof(Element))) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA sparse evaluation rejects value overlap");
    }
    operand.values = source.value_data();
    return Status::Ok();
  }
}

template <typename Destination, typename Scalar>
Status SetTerminal(const Destination&, const ScalarExpression<Scalar>& scalar,
                   PointwiseOperand& operand, bool, bool&) {
  using Element = typename Destination::value_type;
  if constexpr (!std::same_as<Scalar, Element>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse scalar type must match exactly");
  } else {
    operand.scalar = static_cast<double>(scalar.value());
    operand.is_scalar = true;
    return Status::Ok();
  }
}

template <typename Destination, typename Expression>
Status ConfigureExpression(const Destination& destination,
                           const Expression& expression, PointwisePlan& plan) {
  plan.operation = PointwiseOperation::kCopy;
  return SetTerminal(destination, expression, plan.left, true, plan.no_op);
}

template <typename Destination, typename Scalar>
Status ConfigureExpression(const Destination&, const ScalarExpression<Scalar>&,
                           PointwisePlan&) {
  return Status(ErrorCode::kUnsupported,
                "CUDA sparse evaluation rejects a scalar-only expression");
}

template <typename Destination, typename Operation, typename Operand>
Status ConfigureExpression(
    const Destination& destination,
    const UnaryExpression<Operation, Operand>& expression,
    PointwisePlan& plan) {
  if constexpr (!std::same_as<Operation, NegateOperation>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse evaluation rejects this unary operation");
  } else {
    plan.operation = PointwiseOperation::kNegate;
    return SetTerminal(destination, expression.operand().get(), plan.left,
                       false, plan.no_op);
  }
}

template <typename Destination, typename Operation, typename Left,
          typename Right>
Status ConfigureExpression(
    const Destination& destination,
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
                  "CUDA sparse evaluation rejects this binary operation");
  }
  const Status left_status = SetTerminal(destination, expression.left().get(),
                                         plan.left, false, plan.no_op);
  if (!left_status.ok()) {
    return left_status;
  }
  const Status right_status = SetTerminal(destination, expression.right().get(),
                                          plan.right, false, plan.no_op);
  if (!right_status.ok()) {
    return right_status;
  }
  if constexpr (std::same_as<Operation, AddOperation> ||
                std::same_as<Operation, SubtractOperation>) {
    if ((plan.left.is_scalar && plan.left.scalar != 0) ||
        (plan.right.is_scalar && plan.right.scalar != 0)) {
      return Status(
          ErrorCode::kUnsupported,
          "CUDA sparse add and subtract accept only a zero scalar operand");
    }
  }
  return Status::Ok();
}

template <typename Destination, ReadableExpression Expression>
Result<CompletionEvent> Evaluate(const SparseCudaContext& context,
                                 Destination destination,
                                 const Expression& expression) {
  using Element = typename Destination::value_type;
  const bool trusted = [&]() {
    if constexpr (requires {
                    internal_sparse_coordinate::ViewAccess::Trusted(
                        destination);
                  }) {
      return internal_sparse_coordinate::ViewAccess::Trusted(destination);
    } else {
      return internal_sparse_compressed::ViewAccess::Trusted(destination);
    }
  }();
  if (!trusted) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse evaluation requires trusted canonical "
                  "structure provenance");
  }
  if (destination.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA sparse evaluation requires device storage");
  }
  if constexpr (!std::same_as<Element, ExpressionValue<Expression>>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse evaluation requires an exact value type");
  }
  if constexpr (kExpressionRank<Expression> != Destination::rank()) {
    return Status(ErrorCode::kShape, "CUDA sparse expression rank differs");
  } else if (ExpressionShape(expression) != destination.shape()) {
    return Status(ErrorCode::kShape, "CUDA sparse expression shape differs");
  }
  if constexpr (static_cast<std::size_t>(Destination::rank()) > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse evaluation supports rank zero through eight");
  }
  PointwisePlan plan;
  plan.scalar_type = ScalarTypeFor<Element>();
  plan.nnz = destination.nnz();
  plan.destination = destination.value_data();
  for (std::size_t dimension = 0; dimension < destination.shape().size();
       ++dimension) {
    plan.shape[dimension] = destination.shape()[dimension];
  }
  SetStructure(destination, plan);
  const Status expression_status =
      ConfigureExpression(destination, expression, plan);
  if (!expression_status.ok()) {
    return expression_status;
  }
  return LaunchPointwiseOpaque(context, &plan);
}

}  // namespace internal_sparse_cuda

// Allocates exactly the CSR owner buffers from device_resource and enqueues
// explicit host-to-device copies. The host source, context, resource, and
// returned owner must remain alive until completion. Trusted canonical
// provenance does not itself imply readiness; observe the returned event or
// preserve ordering on the same execution context.
template <SparseElement Element>
  requires(!std::is_const_v<Element>) &&
          (std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CudaCsrClone<Element>> CudaCloneCsr(
    const SparseCudaContext& context, CsrView<const Element> source,
    MemoryResource& device_resource) {
  if (source.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaCloneCsr requires a canonical host source");
  }
  if (!internal_sparse_compressed::ViewAccess::Trusted(source)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaCloneCsr requires trusted canonical structure");
  }
  if (device_resource.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CudaCloneCsr requires a device memory resource");
  }
  auto array = internal_sparse_compressed::ArrayFactory<
      Element, SparseCompressedFormat::kCsr>::AllocateStorage(source.shape(),
                                                              source.nnz(),
                                                              device_resource);
  if (!array.ok()) {
    return array.status();
  }
  auto destination = array->view();
  if (!destination.ok()) {
    return destination.status();
  }
  const auto outer_count = static_cast<std::size_t>(source.shape()[0] + 1);
  const auto count = static_cast<std::size_t>(source.nnz());
  const internal_sparse_cuda::ClonePlan plan{
      .source_outer_offsets = source.outer_offset_data(),
      .source_inner_indices = source.inner_index_data(),
      .source_values = source.value_data(),
      .destination_outer_offsets =
          const_cast<nnz_t*>(destination->outer_offset_data()),
      .destination_inner_indices =
          const_cast<index_t*>(destination->inner_index_data()),
      .destination_values = destination->value_data(),
      .outer_bytes = outer_count * sizeof(nnz_t),
      .inner_bytes = count * sizeof(index_t),
      .value_bytes = count * sizeof(Element),
  };
  auto completion = internal_sparse_cuda::CloneCsrOpaque(context, &plan);
  if (!completion.ok()) {
    return completion.status();
  }
  return CudaCsrClone<Element>{std::move(*array), std::move(*completion)};
}

// Returns the exact caller-owned workspace requirement without enqueueing
// work. Unit-stride vectors use CUSPARSE_SPMV_CSR_ALG2. Non-unit positive
// strides use the deterministic ASC project kernel and require zero bytes.
template <typename Element>
  requires std::same_as<std::remove_const_t<Element>, float> ||
           std::same_as<std::remove_const_t<Element>, double>
[[nodiscard]] Result<std::size_t> CudaCsrSpmvWorkspaceSize(
    const SparseCudaContext& context, std::remove_const_t<Element> alpha,
    CsrView<Element> matrix,
    CudaStridedVectorView<const std::remove_const_t<Element>> input,
    std::remove_const_t<Element> beta,
    CudaStridedVectorView<std::remove_const_t<Element>> output) {
  using Scalar = std::remove_const_t<Element>;
  if (matrix.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA SpMV requires a device CSR matrix");
  }
  if (!internal_sparse_compressed::ViewAccess::Trusted(matrix)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA SpMV requires trusted canonical CSR provenance");
  }
  if (input.extent() != matrix.shape()[1] ||
      output.extent() != matrix.shape()[0]) {
    return Status(ErrorCode::kShape, "CUDA SpMV operand shapes differ");
  }
  const internal_sparse_cuda::SpmvPlan plan{
      .scalar_type = internal_sparse_cuda::ScalarTypeFor<Scalar>(),
      .outer_offsets = matrix.outer_offset_data(),
      .inner_indices = matrix.inner_index_data(),
      .values = matrix.value_data(),
      .rows = matrix.shape()[0],
      .columns = matrix.shape()[1],
      .nnz = matrix.nnz(),
      .input = input.data(),
      .input_stride = input.stride(),
      .output = output.data(),
      .output_stride = output.stride(),
      .alpha = static_cast<double>(alpha),
      .beta = static_cast<double>(beta),
  };
  return internal_sparse_cuda::QuerySpmvWorkspaceOpaque(context, &plan);
}

// Enqueues deterministic SpMV. Unit-stride vectors use
// CUSPARSE_SPMV_CSR_ALG2; non-unit positive strides use the original ASC
// project kernel. Query the exact workspace bytes first, allocate that
// caller-owned device span, and keep the context, matrix/vector storage, and
// workspace alive through completion. No transfer, conversion, packing, or
// fallback occurs.
template <typename Element>
  requires std::same_as<std::remove_const_t<Element>, float> ||
           std::same_as<std::remove_const_t<Element>, double>
[[nodiscard]] Result<CompletionEvent> CudaCsrSpmv(
    const SparseCudaContext& context, std::remove_const_t<Element> alpha,
    CsrView<Element> matrix,
    CudaStridedVectorView<const std::remove_const_t<Element>> input,
    std::remove_const_t<Element> beta,
    CudaStridedVectorView<std::remove_const_t<Element>> output,
    MutableMemoryView workspace) {
  using Scalar = std::remove_const_t<Element>;
  if (matrix.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA SpMV requires a device CSR matrix");
  }
  if (!internal_sparse_compressed::ViewAccess::Trusted(matrix)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA SpMV requires trusted canonical CSR provenance");
  }
  if (input.extent() != matrix.shape()[1] ||
      output.extent() != matrix.shape()[0]) {
    return Status(ErrorCode::kShape, "CUDA SpMV operand shapes differ");
  }
  const internal_sparse_cuda::SpmvPlan plan{
      .scalar_type = internal_sparse_cuda::ScalarTypeFor<Scalar>(),
      .outer_offsets = matrix.outer_offset_data(),
      .inner_indices = matrix.inner_index_data(),
      .values = matrix.value_data(),
      .rows = matrix.shape()[0],
      .columns = matrix.shape()[1],
      .nnz = matrix.nnz(),
      .input = input.data(),
      .input_stride = input.stride(),
      .output = output.data(),
      .output_stride = output.stride(),
      .alpha = static_cast<double>(alpha),
      .beta = static_cast<double>(beta),
      .workspace = workspace,
  };
  return internal_sparse_cuda::LaunchSpmvOpaque(context, &plan);
}

// Evaluates only the bounded shallow copy/negate/add/subtract/multiply subset
// over trusted identical canonical structure. Exact in-place operands are
// permitted; partial value overlap, nested expressions, and structure changes
// are rejected. All backing storage and the context must remain alive through
// completion.
template <SparseElement Element, std::size_t Rank,
          ReadableExpression Expression>
  requires(!std::is_const_v<Element>) &&
          (std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CompletionEvent> CudaEvaluate(
    const SparseCudaContext& context, CoordinateView<Element, Rank> destination,
    const Expression& expression) {
  return internal_sparse_cuda::Evaluate(context, destination, expression);
}

template <SparseElement Element, SparseCompressedFormat Format,
          ReadableExpression Expression>
  requires(!std::is_const_v<Element>) &&
          (std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CompletionEvent> CudaEvaluate(
    const SparseCudaContext& context,
    CompressedSparseView<Element, Format> destination,
    const Expression& expression) {
  return internal_sparse_cuda::Evaluate(context, destination, expression);
}

}  // namespace asc

#endif  // ASC_SPARSE_PROVIDERS_CUDA_H_
