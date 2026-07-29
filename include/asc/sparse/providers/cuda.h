#ifndef ASC_SPARSE_PROVIDERS_CUDA_H_
#define ASC_SPARSE_PROVIDERS_CUDA_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
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

class SparseCudaContext;
template <typename Element>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaCsrClone;

namespace internal_sparse_cuda {

class Access;
class ContextState;

enum class ElementKind {
  kFloat,
  kDouble,
};

enum class FormatKind {
  kCoordinate,
  kCsr,
  kCsc,
};

enum class PointwiseOperation {
  kCopy,
  kNegate,
  kAdd,
  kSubtract,
  kMultiply,
};

enum class OperandKind {
  kView,
  kScalar,
};

struct SparseDescriptor {
  const void* structure_first = nullptr;
  const void* structure_second = nullptr;
  const void* values = nullptr;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  FormatKind format = FormatKind::kCoordinate;
  std::size_t rank = 0;
  const extent_t* extents = nullptr;
  nnz_t nonzeros = 0;
  bool canonical_structure_trusted = false;
};

struct OperandDescriptor {
  OperandKind kind = OperandKind::kScalar;
  SparseDescriptor view;
  double scalar = 0.0;
};

struct VectorDescriptor {
  const void* data = nullptr;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  extent_t extent = 0;
  stride_t stride = 1;
  bool writable = false;
};

struct CloneBuffers {
  Buffer outer_offsets;
  Buffer inner_indices;
  Buffer values;
  CompletionEvent completion;
};

ASC_SPARSE_CUDA_EXPORT Result<CloneBuffers> CloneCsrErased(
    SparseCudaContext& context, SparseDescriptor source,
    MemoryResource& resource);

ASC_SPARSE_CUDA_EXPORT Result<std::size_t> CsrSpmvWorkspaceSizeErased(
    SparseCudaContext& context, SparseDescriptor matrix, VectorDescriptor input,
    VectorDescriptor output);

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> CsrSpmvErased(
    SparseCudaContext& context, double alpha, SparseDescriptor matrix,
    VectorDescriptor input, double beta, VectorDescriptor output,
    MutableMemoryView workspace);

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> EvaluateErased(
    SparseCudaContext& context, PointwiseOperation operation,
    OperandDescriptor left, OperandDescriptor right,
    SparseDescriptor destination);

template <typename T>
inline constexpr bool kSupportedElement =
    !std::is_volatile_v<T> && (std::same_as<std::remove_cv_t<T>, float> ||
                               std::same_as<std::remove_cv_t<T>, double>);

template <typename T>
inline constexpr ExpressionOperation kOperation = [] {
  using Adapter = ExpressionAdapter<std::remove_cvref_t<T>>;
  if constexpr (requires { Adapter::operation; }) {
    return Adapter::operation;
  } else {
    return ExpressionOperation::kExternal;
  }
}();

template <typename T>
struct IsCoordinateView : std::false_type {};

template <typename Element, std::size_t Rank>
struct IsCoordinateView<CoordinateView<Element, Rank>> : std::true_type {};

template <typename T>
struct IsCompressedView : std::false_type {};

template <typename Element, SparseCompressedFormat Format>
struct IsCompressedView<CompressedSparseView<Element, Format>>
    : std::true_type {};

template <typename Element, std::size_t Rank>
SparseDescriptor Describe(const CoordinateView<Element, Rank>& view) {
  SparseDescriptor descriptor;
  descriptor.structure_first = view.coordinates();
  descriptor.values = view.values();
  descriptor.memory_space = view.memory_space();
  descriptor.element_kind = std::same_as<std::remove_cv_t<Element>, float>
                                ? ElementKind::kFloat
                                : ElementKind::kDouble;
  descriptor.format = FormatKind::kCoordinate;
  descriptor.rank = Rank;
  descriptor.extents = view.extents().data();
  descriptor.nonzeros = view.nnz();
  descriptor.canonical_structure_trusted = view.canonical_structure_trusted();
  return descriptor;
}

template <typename Element, SparseCompressedFormat Format>
SparseDescriptor Describe(const CompressedSparseView<Element, Format>& view) {
  SparseDescriptor descriptor;
  descriptor.structure_first = view.outer_offsets();
  descriptor.structure_second = view.inner_indices();
  descriptor.values = view.values();
  descriptor.memory_space = view.memory_space();
  descriptor.element_kind = std::same_as<std::remove_cv_t<Element>, float>
                                ? ElementKind::kFloat
                                : ElementKind::kDouble;
  descriptor.format = Format == SparseCompressedFormat::kCsr ? FormatKind::kCsr
                                                             : FormatKind::kCsc;
  descriptor.rank = 2;
  descriptor.extents = view.extents().data();
  descriptor.nonzeros = view.nnz();
  descriptor.canonical_structure_trusted = view.canonical_structure_trusted();
  return descriptor;
}

template <typename Operand, typename Element>
Result<OperandDescriptor> DescribeOperand(const Operand& operand) {
  using OperandType = std::remove_cvref_t<Operand>;
  if constexpr (std::is_arithmetic_v<OperandType>) {
    if constexpr (!std::same_as<OperandType, Element>) {
      return Status(ErrorCode::kUnsupported,
                    "CUDA sparse scalar and value types must match");
    } else {
      OperandDescriptor descriptor;
      descriptor.kind = OperandKind::kScalar;
      descriptor.scalar = static_cast<double>(operand);
      descriptor.view.element_kind = std::same_as<Element, float>
                                         ? ElementKind::kFloat
                                         : ElementKind::kDouble;
      return descriptor;
    }
  } else if constexpr (IsCoordinateView<OperandType>::value ||
                       IsCompressedView<OperandType>::value) {
    if constexpr (!std::same_as<typename OperandType::value_type, Element>) {
      return Status(ErrorCode::kUnsupported,
                    "CUDA sparse operand value types must match");
    } else {
      OperandDescriptor descriptor;
      descriptor.kind = OperandKind::kView;
      descriptor.view = Describe(operand);
      return descriptor;
    }
  } else {
    return Status(
        ErrorCode::kUnsupported,
        "CUDA sparse evaluation accepts sparse terminals and scalars");
  }
}

struct PreparedEvaluation {
  PointwiseOperation operation = PointwiseOperation::kCopy;
  OperandDescriptor left;
  OperandDescriptor right;
};

template <typename Value, ReadableExpression Expression>
Result<PreparedEvaluation> PrepareEvaluation(const Expression& expression) {
  constexpr ExpressionOperation kExpressionOperation = kOperation<Expression>;
  PreparedEvaluation prepared;
  if constexpr (kExpressionOperation == ExpressionOperation::kTerminal) {
    auto described = DescribeOperand<Expression, Value>(expression);
    if (!described.ok()) {
      return described.status();
    }
    prepared.left = *described;
    prepared.operation = PointwiseOperation::kCopy;
  } else if constexpr (kExpressionOperation == ExpressionOperation::kNegate) {
    if constexpr (requires { expression.operand_storage(); }) {
      const auto& operand =
          internal_expression::StoredExpression(expression.operand_storage());
      if constexpr (kOperation<decltype(operand)> !=
                    ExpressionOperation::kTerminal) {
        return Status(ErrorCode::kUnsupported,
                      "CUDA sparse evaluation rejects nested negation");
      } else {
        auto described = DescribeOperand<decltype(operand), Value>(operand);
        if (!described.ok()) {
          return described.status();
        }
        prepared.left = *described;
        prepared.operation = PointwiseOperation::kNegate;
      }
    } else {
      return Status(ErrorCode::kUnsupported,
                    "CUDA sparse evaluation rejects external negation");
    }
  } else if constexpr (kExpressionOperation == ExpressionOperation::kAdd ||
                       kExpressionOperation == ExpressionOperation::kSubtract ||
                       kExpressionOperation == ExpressionOperation::kMultiply) {
    if constexpr (requires {
                    expression.left_storage();
                    expression.right_storage();
                  }) {
      const auto& left_operand =
          internal_expression::StoredExpression(expression.left_storage());
      const auto& right_operand =
          internal_expression::StoredExpression(expression.right_storage());
      constexpr ExpressionOperation kLeftOperation =
          kOperation<decltype(left_operand)>;
      constexpr ExpressionOperation kRightOperation =
          kOperation<decltype(right_operand)>;
      if constexpr ((kLeftOperation != ExpressionOperation::kTerminal &&
                     kLeftOperation != ExpressionOperation::kScalar) ||
                    (kRightOperation != ExpressionOperation::kTerminal &&
                     kRightOperation != ExpressionOperation::kScalar) ||
                    (kLeftOperation == ExpressionOperation::kScalar &&
                     kRightOperation == ExpressionOperation::kScalar)) {
        return Status(ErrorCode::kUnsupported,
                      "CUDA sparse evaluation rejects nested or scalar-only "
                      "binary expressions");
      } else {
        if constexpr (kExpressionOperation != ExpressionOperation::kMultiply) {
          if constexpr (kLeftOperation == ExpressionOperation::kScalar) {
            if (left_operand != Value{}) {
              return Status(ErrorCode::kUnsupported,
                            "Sparse scalar addition/subtraction supports only "
                            "exact zero");
            }
          }
          if constexpr (kRightOperation == ExpressionOperation::kScalar) {
            if (right_operand != Value{}) {
              return Status(ErrorCode::kUnsupported,
                            "Sparse scalar addition/subtraction supports only "
                            "exact zero");
            }
          }
        }
        auto described_left =
            DescribeOperand<decltype(left_operand), Value>(left_operand);
        if (!described_left.ok()) {
          return described_left.status();
        }
        auto described_right =
            DescribeOperand<decltype(right_operand), Value>(right_operand);
        if (!described_right.ok()) {
          return described_right.status();
        }
        prepared.left = *described_left;
        prepared.right = *described_right;
        if constexpr (kExpressionOperation == ExpressionOperation::kAdd) {
          prepared.operation = PointwiseOperation::kAdd;
        } else if constexpr (kExpressionOperation ==
                             ExpressionOperation::kSubtract) {
          prepared.operation = PointwiseOperation::kSubtract;
        } else {
          prepared.operation = PointwiseOperation::kMultiply;
        }
      }
    } else {
      return Status(ErrorCode::kUnsupported,
                    "CUDA sparse evaluation rejects external binary nodes");
    }
  } else {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse evaluation rejects this expression");
  }
  return prepared;
}

}  // namespace internal_sparse_cuda

class ASC_SPARSE_CUDA_EXPORT SparseCudaContext {
 public:
  static Result<SparseCudaContext> Create(ExecutionContext execution_context);

  SparseCudaContext(const SparseCudaContext&) = delete;
  SparseCudaContext& operator=(const SparseCudaContext&) = delete;
  SparseCudaContext(SparseCudaContext&& other) noexcept;
  SparseCudaContext& operator=(SparseCudaContext&& other) noexcept;
  ~SparseCudaContext();

  [[nodiscard]] const ExecutionContext& execution_context() const noexcept {
    return execution_context_;
  }

 private:
  friend class internal_sparse_cuda::Access;

  SparseCudaContext(
      ExecutionContext execution_context,
      std::unique_ptr<internal_sparse_cuda::ContextState> state) noexcept;

  ExecutionContext execution_context_;
  std::unique_ptr<internal_sparse_cuda::ContextState> state_;
};

template <typename Element>
  requires internal_sparse_cuda::kSupportedElement<Element>
class CudaStridedVectorView {
 public:
  static Result<CudaStridedVectorView> Create(Element* data, extent_t extent,
                                              stride_t stride) {
    if (extent < 0 || stride <= 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "A CUDA strided vector requires nonnegative extent and "
                    "positive stride");
    }
    if (extent != 0 && data == nullptr) {
      return Status(ErrorCode::kInvalidArgument,
                    "A nonempty CUDA strided vector cannot be null");
    }
    return CudaStridedVectorView(data, extent, stride);
  }

  template <typename OtherElement>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<OtherElement>,
                          std::remove_const_t<Element>> &&
             !std::is_const_v<OtherElement>)
  constexpr CudaStridedVectorView(
      CudaStridedVectorView<OtherElement> other) noexcept
      : data_(other.data()), extent_(other.extent()), stride_(other.stride()) {}

  [[nodiscard]] constexpr Element* data() const noexcept { return data_; }
  [[nodiscard]] constexpr extent_t extent() const noexcept { return extent_; }
  [[nodiscard]] constexpr stride_t stride() const noexcept { return stride_; }
  [[nodiscard]] static constexpr MemorySpace memory_space() noexcept {
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

namespace internal_sparse_cuda {

template <typename Element>
VectorDescriptor Describe(CudaStridedVectorView<Element> view) {
  VectorDescriptor descriptor;
  descriptor.data = view.data();
  descriptor.memory_space = MemorySpace::kDevice;
  descriptor.element_kind = std::same_as<std::remove_cv_t<Element>, float>
                                ? ElementKind::kFloat
                                : ElementKind::kDouble;
  descriptor.extent = view.extent();
  descriptor.stride = view.stride();
  descriptor.writable = !std::is_const_v<Element>;
  return descriptor;
}

class Access {
 public:
  [[nodiscard]] static ContextState* State(
      SparseCudaContext& context) noexcept {
    return context.state_.get();
  }
};

}  // namespace internal_sparse_cuda

template <typename Element>
  requires internal_sparse_cuda::kSupportedElement<Element>
class CudaCsrArray {
 public:
  CudaCsrArray(const CudaCsrArray&) = delete;
  CudaCsrArray& operator=(const CudaCsrArray&) = delete;
  CudaCsrArray(CudaCsrArray&&) noexcept = default;
  CudaCsrArray& operator=(CudaCsrArray&&) noexcept = default;
  ~CudaCsrArray() = default;

  [[nodiscard]] bool valid() const noexcept {
    return outer_offsets_.valid() && inner_indices_.valid() && values_.valid();
  }
  [[nodiscard]] extent_t rows() const noexcept { return shape_[0]; }
  [[nodiscard]] extent_t columns() const noexcept { return shape_[1]; }
  [[nodiscard]] nnz_t nnz() const noexcept { return nonzeros_; }

  [[nodiscard]] Result<CsrView<Element>> view() {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CUDA CSR array has no view");
    }
    return internal_sparse_compressed::ProviderAccess::MakeCanonicalView<
        Element, SparseCompressedFormat::kCsr>(
        static_cast<const nnz_t*>(outer_offsets_.data()),
        static_cast<const index_t*>(inner_indices_.data()),
        static_cast<Element*>(values_.data()), shape_, nonzeros_,
        MemorySpace::kDevice);
  }

  [[nodiscard]] Result<CsrView<const Element>> view() const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CUDA CSR array has no view");
    }
    return internal_sparse_compressed::ProviderAccess::MakeCanonicalView<
        const Element, SparseCompressedFormat::kCsr>(
        static_cast<const nnz_t*>(outer_offsets_.data()),
        static_cast<const index_t*>(inner_indices_.data()),
        static_cast<const Element*>(values_.data()), shape_, nonzeros_,
        MemorySpace::kDevice);
  }

 private:
  template <typename SourceElement>
    requires internal_sparse_cuda::kSupportedElement<SourceElement>
  friend Result<CudaCsrClone<std::remove_const_t<SourceElement>>> CudaCloneCsr(
      SparseCudaContext&, CsrView<SourceElement>, MemoryResource&);

  CudaCsrArray(std::array<extent_t, 2> shape, nnz_t nonzeros,
               Buffer outer_offsets, Buffer inner_indices,
               Buffer values) noexcept
      : shape_(shape),
        nonzeros_(nonzeros),
        outer_offsets_(std::move(outer_offsets)),
        inner_indices_(std::move(inner_indices)),
        values_(std::move(values)) {}

  std::array<extent_t, 2> shape_;
  nnz_t nonzeros_;
  Buffer outer_offsets_;
  Buffer inner_indices_;
  Buffer values_;
};

template <typename Element>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaCsrClone {
  CudaCsrArray<Element> array;
  CompletionEvent completion;
};

template <typename SourceElement>
  requires internal_sparse_cuda::kSupportedElement<SourceElement>
[[nodiscard]] Result<CudaCsrClone<std::remove_const_t<SourceElement>>>
CudaCloneCsr(SparseCudaContext& context, CsrView<SourceElement> source,
             MemoryResource& resource) {
  using Element = std::remove_const_t<SourceElement>;
  auto cloned = internal_sparse_cuda::CloneCsrErased(
      context, internal_sparse_cuda::Describe(source), resource);
  if (!cloned.ok()) {
    return cloned.status();
  }
  CudaCsrArray<Element> array(
      source.shape(), source.nnz(), std::move(cloned->outer_offsets),
      std::move(cloned->inner_indices), std::move(cloned->values));
  return CudaCsrClone<Element>{.array = std::move(array),
                               .completion = std::move(cloned->completion)};
}

template <typename Element>
  requires internal_sparse_cuda::kSupportedElement<Element>
[[nodiscard]] Result<std::size_t> CudaCsrSpmvWorkspaceSize(
    SparseCudaContext& context, CsrView<const Element> matrix,
    CudaStridedVectorView<const Element> input,
    CudaStridedVectorView<Element> output) {
  return internal_sparse_cuda::CsrSpmvWorkspaceSizeErased(
      context, internal_sparse_cuda::Describe(matrix),
      internal_sparse_cuda::Describe(input),
      internal_sparse_cuda::Describe(output));
}

template <typename Element>
  requires internal_sparse_cuda::kSupportedElement<Element>
[[nodiscard]] Result<CompletionEvent> CudaCsrSpmv(
    SparseCudaContext& context, Element alpha, CsrView<const Element> matrix,
    CudaStridedVectorView<const Element> input, Element beta,
    CudaStridedVectorView<Element> output, MutableMemoryView workspace) {
  return internal_sparse_cuda::CsrSpmvErased(
      context, static_cast<double>(alpha),
      internal_sparse_cuda::Describe(matrix),
      internal_sparse_cuda::Describe(input), static_cast<double>(beta),
      internal_sparse_cuda::Describe(output), workspace);
}

template <ReadableExpression Expression, typename Element, std::size_t Rank>
  requires(internal_sparse_cuda::kSupportedElement<Element> &&
           !std::is_const_v<Element>)
[[nodiscard]] Result<CompletionEvent> CudaEvaluate(
    SparseCudaContext& context, const Expression& expression,
    CoordinateView<Element, Rank> destination) {
  using Value = std::remove_cv_t<Element>;
  if constexpr (!std::same_as<ExpressionValue<Expression>, Value>) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse evaluation requires matching float/double "
                  "operands");
  } else {
    auto prepared = internal_sparse_cuda::PrepareEvaluation<Value>(expression);
    if (!prepared.ok()) {
      return prepared.status();
    }
    return internal_sparse_cuda::EvaluateErased(
        context, prepared->operation, prepared->left, prepared->right,
        internal_sparse_cuda::Describe(destination));
  }
}

template <ReadableExpression Expression, typename Element,
          SparseCompressedFormat Format>
  requires(internal_sparse_cuda::kSupportedElement<Element> &&
           !std::is_const_v<Element>)
[[nodiscard]] Result<CompletionEvent> CudaEvaluate(
    SparseCudaContext& context, const Expression& expression,
    CompressedSparseView<Element, Format> destination) {
  if constexpr (std::same_as<ExpressionValue<Expression>, Element>) {
    auto prepared =
        internal_sparse_cuda::PrepareEvaluation<Element>(expression);
    if (!prepared.ok()) {
      return prepared.status();
    }
    return internal_sparse_cuda::EvaluateErased(
        context, prepared->operation, prepared->left, prepared->right,
        internal_sparse_cuda::Describe(destination));
  } else {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse evaluation value types must match");
  }
}

}  // namespace asc

#endif  // ASC_SPARSE_PROVIDERS_CUDA_H_
