#ifndef ASC_SPARSE_PROVIDERS_CUDA_H_
#define ASC_SPARSE_PROVIDERS_CUDA_H_

/**
 * @file
 * @brief Public CUDA provider declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_cuda
 */

#include <array>
#include <complex>
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
#include "asc/sparse/blas.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"
#include "asc/sparse/providers/cuda_export.h"

namespace asc {

class SparseCudaContext;
template <typename Element>
  requires SparseBlasScalar<Element>
struct CudaCsrClone;
template <typename Element>
  requires SparseBlasScalar<Element>
class CudaIndexedVectorArray;
template <typename Element>
  requires SparseBlasScalar<Element>
struct CudaIndexedVectorClone;
template <typename Element>
  requires SparseBlasScalar<Element>
class CudaTriangularCsrArray;
template <typename Element>
  requires SparseBlasScalar<Element>
struct CudaTriangularCsrClone;

/**
 * @brief Enqueues the experimental CUDA CloneIndexedVector operation and
 * returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @param[in] resource Allocator that must outlive storage allocated from it.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <typename SourceElement>
  requires SparseBlasScalar<SourceElement>
Result<CudaIndexedVectorClone<std::remove_const_t<SourceElement>>>
CudaCloneIndexedVector(SparseCudaContext& context,
                       SparseBlasIndexedVectorView<SourceElement> source,
                       MemoryResource& resource);

/**
 * @brief Enqueues the experimental CUDA CloneTriangularCsr operation and
 * returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @param[in] resource Allocator that must outlive storage allocated from it.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
Result<CudaTriangularCsrClone<Element>> CudaCloneTriangularCsr(
    SparseCudaContext& context,
    SparseBlasTriangularView<Element, SparseCompressedFormat::kCsr> source,
    MemoryResource& resource);

namespace internal_sparse_cuda {

class Access;
class ContextState;

enum class ElementKind {
  kFloat,          ///< Selects float behavior.
  kDouble,         ///< Selects double behavior.
  kComplexFloat,   ///< Selects complex float behavior.
  kComplexDouble,  ///< Selects complex double behavior.
};

enum class StandardOperation {
  kDot,         ///< Selects dot behavior.
  kAxpy,        ///< Selects axpy behavior.
  kGather,      ///< Selects gather behavior.
  kGatherZero,  ///< Selects gather zero behavior.
  kScatter,     ///< Selects scatter behavior.
};

struct ScalarValue {
  double real = 0.0;
  double imaginary = 0.0;
};

enum class FormatKind {
  kCoordinate,  ///< Selects coordinate behavior.
  kCsr,         ///< Compressed sparse row storage.
  kCsc,         ///< Compressed sparse column storage.
};

enum class PointwiseOperation {
  kCopy,      ///< Selects copy behavior.
  kNegate,    ///< Selects negate behavior.
  kAdd,       ///< Selects add behavior.
  kSubtract,  ///< Selects subtract behavior.
  kMultiply,  ///< Selects multiply behavior.
};

enum class OperandKind {
  kView,    ///< Selects view behavior.
  kScalar,  ///< Selects scalar behavior.
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
  extent_t rows = 0;
  extent_t columns = 0;
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
  const void* reachable_data = nullptr;
  std::size_t reachable_size = 0;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  extent_t extent = 0;
  stride_t stride = 1;
  bool writable = false;
};

struct IndexedVectorDescriptor {
  const index_t* indices = nullptr;
  const void* values = nullptr;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  nnz_t nonzeros = 0;
  extent_t dense_extent = 0;
  bool writable = false;
  bool canonical_structure_trusted = false;
};

struct MatrixDescriptor {
  const void* data = nullptr;
  const void* reachable_data = nullptr;
  std::size_t reachable_size = 0;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  extent_t rows = 0;
  extent_t columns = 0;
  stride_t leading_dimension = 1;
  SparseBlasLayout layout = SparseBlasLayout::kColumnMajor;
  bool writable = false;
};

struct CloneBuffers {
  Buffer outer_offsets;
  Buffer inner_indices;
  Buffer values;
  CompletionEvent completion;
};

struct IndexedCloneBuffers {
  Buffer indices;
  Buffer values;
  CompletionEvent completion;
};

ASC_SPARSE_CUDA_EXPORT Result<CloneBuffers> CloneCsrErased(
    SparseCudaContext& context, SparseDescriptor source,
    MemoryResource& resource);

ASC_SPARSE_CUDA_EXPORT Result<IndexedCloneBuffers> CloneIndexedErased(
    SparseCudaContext& context, IndexedVectorDescriptor source,
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

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> StandardLevel1Erased(
    SparseCudaContext& context, StandardOperation operation,
    SparseBlasConjugation conjugation, ScalarValue alpha,
    IndexedVectorDescriptor sparse, VectorDescriptor dense,
    VectorDescriptor result);

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> StandardSpmvErased(
    SparseCudaContext& context, SparseBlasTranspose transpose,
    ScalarValue alpha, SparseDescriptor matrix, VectorDescriptor input,
    VectorDescriptor output);

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> StandardSpmmErased(
    SparseCudaContext& context, SparseBlasTranspose transpose,
    ScalarValue alpha, SparseDescriptor matrix, MatrixDescriptor input,
    MatrixDescriptor output);

ASC_SPARSE_CUDA_EXPORT Result<CompletionEvent> StandardTriangularSolveErased(
    SparseCudaContext& context, SparseBlasTranspose transpose,
    ScalarValue alpha, SparseDescriptor matrix, SparseBlasTriangle triangle,
    SparseBlasDiagonal diagonal, MatrixDescriptor right_hand_sides);

template <typename T>
inline constexpr bool kSupportedElement =
    !std::is_volatile_v<T> && (std::same_as<std::remove_cv_t<T>, float> ||
                               std::same_as<std::remove_cv_t<T>, double>);

template <typename T>
inline constexpr bool kSupportedBlasElement =
    !std::is_volatile_v<T> && SparseBlasScalar<T>;

template <typename Element>
constexpr ElementKind Kind() {
  using Value = std::remove_cv_t<Element>;
  if constexpr (std::same_as<Value, float>) {
    return ElementKind::kFloat;
  } else if constexpr (std::same_as<Value, double>) {
    return ElementKind::kDouble;
  } else if constexpr (std::same_as<Value, std::complex<float>>) {
    return ElementKind::kComplexFloat;
  } else {
    return ElementKind::kComplexDouble;
  }
}

template <SparseBlasScalar Element>
ScalarValue Scalar(Element value) {
  if constexpr (SparseBlasComplex<Element>) {
    return {static_cast<double>(value.real()),
            static_cast<double>(value.imag())};
  } else {
    return {static_cast<double>(value), 0.0};
  }
}

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
  descriptor.element_kind = Kind<Element>();
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
  descriptor.element_kind = Kind<Element>();
  descriptor.format = Format == SparseCompressedFormat::kCsr ? FormatKind::kCsr
                                                             : FormatKind::kCsc;
  descriptor.rank = 2;
  descriptor.extents = view.extents().data();
  descriptor.rows = view.rows();
  descriptor.columns = view.columns();
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

/**
 * @brief Owns experimental cuSPARSE state bound to an execution context.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
class ASC_SPARSE_CUDA_EXPORT SparseCudaContext {
 public:
  /**
   * @brief Validates inputs and creates the requested CUDA provider object.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] execution_context The execution context value required by this
   * contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
  static Result<SparseCudaContext> Create(ExecutionContext execution_context);

  /**
   * @brief Constructs a SparseCudaContext with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  SparseCudaContext(const SparseCudaContext&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  SparseCudaContext& operator=(const SparseCudaContext&) = delete;
  /**
   * @brief Constructs a SparseCudaContext with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] other The other value required by this contract.
   * @ingroup asc_cuda
   */
  SparseCudaContext(SparseCudaContext&& other) noexcept;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] other The other value required by this contract.
   * @return This context after taking ownership from `other`.
   * @ingroup asc_cuda
   */
  SparseCudaContext& operator=(SparseCudaContext&& other) noexcept;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  ~SparseCudaContext();

  /**
   * @brief Performs the public execution_context operation defined by the CUDA
   * provider contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] const ExecutionContext& execution_context() const noexcept {
    return execution_context_;
  }

 private:
  /**
   * @brief Performs the public Access operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  friend class internal_sparse_cuda::Access;

  SparseCudaContext(
      ExecutionContext execution_context,
      std::unique_ptr<internal_sparse_cuda::ContextState> state) noexcept;

  ExecutionContext execution_context_;
  std::unique_ptr<internal_sparse_cuda::ContextState> state_;
};

/**
 * @brief Describes an experimental CUDA-accessible strided vector.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires internal_sparse_cuda::kSupportedElement<Element>
class CudaStridedVectorView {
 public:
  /**
   * @brief Validates inputs and creates the requested CUDA provider object.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] data The data value required by this contract.
   * @param[in] extent The extent value required by this contract.
   * @param[in] stride The stride value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
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

  /**
   * @brief Constructs a CudaStridedVectorView with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] other The other value required by this contract.
   * @ingroup asc_cuda
   */
  template <typename OtherElement>
    requires(std::is_const_v<Element> &&
             std::same_as<std::remove_const_t<OtherElement>,
                          std::remove_const_t<Element>> &&
             !std::is_const_v<OtherElement>)
  constexpr CudaStridedVectorView(
      CudaStridedVectorView<OtherElement> other) noexcept
      : data_(other.data()), extent_(other.extent()), stride_(other.stride()) {}

  /**
   * @brief Returns the object's data contract value.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] constexpr Element* data() const noexcept { return data_; }
  /**
   * @brief Performs the public extent operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] constexpr extent_t extent() const noexcept { return extent_; }
  /**
   * @brief Performs the public stride operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] constexpr stride_t stride() const noexcept { return stride_; }
  /**
   * @brief Performs the public memory_space operation defined by the CUDA
   * provider contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
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

template <SparseBlasScalar Element>
VectorDescriptor Describe(SparseBlasVectorView<Element> view) {
  VectorDescriptor descriptor;
  descriptor.data = view.data();
  descriptor.reachable_data = view.reachable_storage().data();
  descriptor.reachable_size = view.reachable_storage().size();
  descriptor.memory_space = view.memory_space();
  descriptor.element_kind = Kind<Element>();
  descriptor.extent = view.size();
  descriptor.stride = view.increment();
  descriptor.writable = !std::is_const_v<Element>;
  return descriptor;
}

template <SparseBlasScalar Element>
IndexedVectorDescriptor Describe(SparseBlasIndexedVectorView<Element> view) {
  IndexedVectorDescriptor descriptor;
  descriptor.indices = view.indices();
  descriptor.values = view.values();
  descriptor.memory_space = view.memory_space();
  descriptor.element_kind = Kind<Element>();
  descriptor.nonzeros = view.nnz();
  descriptor.dense_extent = view.dense_extent();
  descriptor.writable = !std::is_const_v<Element>;
  descriptor.canonical_structure_trusted = view.canonical_structure_trusted();
  return descriptor;
}

template <SparseBlasScalar Element>
MatrixDescriptor Describe(SparseBlasMatrixView<Element> view) {
  MatrixDescriptor descriptor;
  descriptor.data = view.data();
  descriptor.reachable_data = view.reachable_storage().data();
  descriptor.reachable_size = view.reachable_storage().size();
  descriptor.memory_space = view.memory_space();
  descriptor.element_kind = Kind<Element>();
  descriptor.rows = view.rows();
  descriptor.columns = view.columns();
  descriptor.leading_dimension = view.leading_dimension();
  descriptor.layout = view.layout();
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

/**
 * @brief Owns experimental CUDA CSR storage and its completion state.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires internal_sparse_cuda::kSupportedBlasElement<Element>
class CudaCsrArray {
 public:
  /**
   * @brief Constructs a CudaCsrArray with the documented ownership and validity
   * state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaCsrArray(const CudaCsrArray&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaCsrArray& operator=(const CudaCsrArray&) = delete;
  /**
   * @brief Constructs a CudaCsrArray with the documented ownership and validity
   * state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaCsrArray(CudaCsrArray&&) noexcept = default;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaCsrArray& operator=(CudaCsrArray&&) noexcept = default;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  ~CudaCsrArray() = default;

  /**
   * @brief Reports whether the documented valid condition holds.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] bool valid() const noexcept {
    return outer_offsets_.valid() && inner_indices_.valid() && values_.valid();
  }
  /**
   * @brief Returns the object's rows contract value.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] extent_t rows() const noexcept { return shape_[0]; }
  /**
   * @brief Returns the object's columns contract value.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] extent_t columns() const noexcept { return shape_[1]; }
  /**
   * @brief Returns the object's nnz contract value.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] nnz_t nnz() const noexcept { return nonzeros_; }

  /**
   * @brief Performs the public view operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
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

  /**
   * @brief Performs the public view operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
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
    requires internal_sparse_cuda::kSupportedBlasElement<SourceElement>
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

/**
 * @brief Owns an experimental asynchronous CUDA CSR clone result.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires SparseBlasScalar<Element>
struct CudaCsrClone {
  /**
   * @brief Stores the array value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CudaCsrArray<Element> array;
  /**
   * @brief Stores the completion value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CompletionEvent completion;
};

/**
 * @brief Enqueues the experimental CUDA CloneCsr operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @param[in] resource Allocator that must outlive storage allocated from it.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <typename SourceElement>
  requires internal_sparse_cuda::kSupportedBlasElement<SourceElement>
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

/**
 * @brief Owns an experimental CUDA indexed sparse vector.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires SparseBlasScalar<Element>
class CudaIndexedVectorArray {
 public:
  /**
   * @brief Constructs a CudaIndexedVectorArray with the documented ownership
   * and validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaIndexedVectorArray(const CudaIndexedVectorArray&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaIndexedVectorArray& operator=(const CudaIndexedVectorArray&) = delete;
  /**
   * @brief Constructs a CudaIndexedVectorArray with the documented ownership
   * and validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaIndexedVectorArray(CudaIndexedVectorArray&&) noexcept = default;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaIndexedVectorArray& operator=(CudaIndexedVectorArray&&) noexcept =
      default;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  ~CudaIndexedVectorArray() = default;

  /**
   * @brief Reports whether the documented valid condition holds.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] bool valid() const noexcept {
    return indices_.valid() && values_.valid();
  }

  /**
   * @brief Performs the public view operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
  [[nodiscard]] Result<SparseBlasIndexedVectorView<Element>> view() {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CUDA indexed vector has no view");
    }
    return internal_sparse_standard_blas::ProviderAccess::
        MakeCanonicalIndexedVector<Element>(
            static_cast<const index_t*>(indices_.data()),
            static_cast<Element*>(values_.data()), nonzeros_, dense_extent_,
            {indices_.data(), indices_.size(), MemorySpace::kDevice},
            {values_.data(), values_.size(), MemorySpace::kDevice});
  }

  /**
   * @brief Performs the public view operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
  [[nodiscard]] Result<SparseBlasIndexedVectorView<const Element>> view()
      const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CUDA indexed vector has no view");
    }
    return internal_sparse_standard_blas::ProviderAccess::
        MakeCanonicalIndexedVector<const Element>(
            static_cast<const index_t*>(indices_.data()),
            static_cast<const Element*>(values_.data()), nonzeros_,
            dense_extent_,
            {indices_.data(), indices_.size(), MemorySpace::kDevice},
            {values_.data(), values_.size(), MemorySpace::kDevice});
  }

 private:
  template <typename SourceElement>
    requires SparseBlasScalar<SourceElement>
  friend Result<CudaIndexedVectorClone<std::remove_const_t<SourceElement>>>
  CudaCloneIndexedVector(SparseCudaContext&,
                         SparseBlasIndexedVectorView<SourceElement>,
                         MemoryResource&);

  CudaIndexedVectorArray(extent_t dense_extent, nnz_t nonzeros, Buffer indices,
                         Buffer values) noexcept
      : dense_extent_(dense_extent),
        nonzeros_(nonzeros),
        indices_(std::move(indices)),
        values_(std::move(values)) {}

  extent_t dense_extent_;
  nnz_t nonzeros_;
  Buffer indices_;
  Buffer values_;
};

/**
 * @brief Owns an experimental asynchronous CUDA indexed-vector clone.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires SparseBlasScalar<Element>
struct CudaIndexedVectorClone {
  /**
   * @brief Stores the array value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CudaIndexedVectorArray<Element> array;
  /**
   * @brief Stores the completion value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CompletionEvent completion;
};

template <typename SourceElement>
  requires SparseBlasScalar<SourceElement>
[[nodiscard]]
Result<CudaIndexedVectorClone<std::remove_const_t<SourceElement>>>
CudaCloneIndexedVector(SparseCudaContext& context,
                       SparseBlasIndexedVectorView<SourceElement> source,
                       MemoryResource& resource) {
  auto cloned = internal_sparse_cuda::CloneIndexedErased(
      context, internal_sparse_cuda::Describe(source), resource);
  if (!cloned.ok()) {
    return cloned.status();
  }
  CudaIndexedVectorArray<std::remove_const_t<SourceElement>> array(
      source.dense_extent(), source.nnz(), std::move(cloned->indices),
      std::move(cloned->values));
  return CudaIndexedVectorClone<std::remove_const_t<SourceElement>>{
      .array = std::move(array), .completion = std::move(cloned->completion)};
}

/**
 * @brief Owns an experimental CUDA triangular CSR descriptor and storage.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires SparseBlasScalar<Element>
class CudaTriangularCsrArray {
 public:
  /**
   * @brief Constructs a CudaTriangularCsrArray with the documented ownership
   * and validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaTriangularCsrArray(const CudaTriangularCsrArray&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaTriangularCsrArray& operator=(const CudaTriangularCsrArray&) = delete;
  /**
   * @brief Constructs a CudaTriangularCsrArray with the documented ownership
   * and validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaTriangularCsrArray(CudaTriangularCsrArray&&) noexcept = default;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaTriangularCsrArray& operator=(CudaTriangularCsrArray&&) noexcept =
      default;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  ~CudaTriangularCsrArray() = default;

  /**
   * @brief Performs the public matrix operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
  [[nodiscard]] Result<CsrView<const Element>> matrix() const {
    return array_.view();
  }
  /**
   * @brief Performs the public triangle operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] SparseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /**
   * @brief Performs the public diagonal operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] SparseBlasDiagonal diagonal() const noexcept {
    return diagonal_;
  }

 private:
  template <SparseBlasScalar SourceElement>
  friend Result<CudaTriangularCsrClone<SourceElement>> CudaCloneTriangularCsr(
      SparseCudaContext&,
      SparseBlasTriangularView<SourceElement, SparseCompressedFormat::kCsr>,
      MemoryResource&);

  CudaTriangularCsrArray(CudaCsrArray<Element> array,
                         SparseBlasTriangle triangle,
                         SparseBlasDiagonal diagonal) noexcept
      : array_(std::move(array)), triangle_(triangle), diagonal_(diagonal) {}

  CudaCsrArray<Element> array_;
  SparseBlasTriangle triangle_;
  SparseBlasDiagonal diagonal_;
};

/**
 * @brief Owns an asynchronous CUDA triangular-CSR clone result.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element>
  requires SparseBlasScalar<Element>
struct CudaTriangularCsrClone {
  /**
   * @brief Stores the array value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CudaTriangularCsrArray<Element> array;
  /**
   * @brief Stores the completion value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CompletionEvent completion;
};

template <SparseBlasScalar Element>
[[nodiscard]] Result<CudaTriangularCsrClone<Element>> CudaCloneTriangularCsr(
    SparseCudaContext& context,
    SparseBlasTriangularView<Element, SparseCompressedFormat::kCsr> source,
    MemoryResource& resource) {
  auto cloned = CudaCloneCsr(context, source.matrix(), resource);
  if (!cloned.ok()) {
    return cloned.status();
  }
  CudaTriangularCsrArray<Element> array(std::move(cloned->array),
                                        source.triangle(), source.diagonal());
  return CudaTriangularCsrClone<Element>{
      .array = std::move(array), .completion = std::move(cloned->completion)};
}

/**
 * @brief Enqueues the experimental CUDA SparseDot operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] conjugation The conjugation value required by this contract.
 * @param[in] sparse The sparse value required by this contract.
 * @param[in] dense The dense value required by this contract.
 * @param[out] result The result value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseDot(
    SparseCudaContext& context, SparseBlasConjugation conjugation,
    SparseBlasIndexedVectorView<const Element> sparse,
    SparseBlasVectorView<const Element> dense,
    SparseBlasVectorView<Element> result) {
  return internal_sparse_cuda::StandardLevel1Erased(
      context, internal_sparse_cuda::StandardOperation::kDot, conjugation,
      internal_sparse_cuda::Scalar(Element{1}),
      internal_sparse_cuda::Describe(sparse),
      internal_sparse_cuda::Describe(dense),
      internal_sparse_cuda::Describe(result));
}

/**
 * @brief Enqueues the experimental CUDA SparseAxpy operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] alpha Scaling factor applied to the primary operation.
 * @param[in] sparse The sparse value required by this contract.
 * @param[in] dense The dense value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseAxpy(
    SparseCudaContext& context, Element alpha,
    SparseBlasIndexedVectorView<const Element> sparse,
    SparseBlasVectorView<Element> dense) {
  return internal_sparse_cuda::StandardLevel1Erased(
      context, internal_sparse_cuda::StandardOperation::kAxpy,
      SparseBlasConjugation::kUnconjugated, internal_sparse_cuda::Scalar(alpha),
      internal_sparse_cuda::Describe(sparse),
      internal_sparse_cuda::Describe(dense), {});
}

/**
 * @brief Enqueues the experimental CUDA SparseGather operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] dense The dense value required by this contract.
 * @param[in] sparse The sparse value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseGather(
    SparseCudaContext& context, SparseBlasVectorView<const Element> dense,
    SparseBlasIndexedVectorView<Element> sparse) {
  return internal_sparse_cuda::StandardLevel1Erased(
      context, internal_sparse_cuda::StandardOperation::kGather,
      SparseBlasConjugation::kUnconjugated,
      internal_sparse_cuda::Scalar(Element{}),
      internal_sparse_cuda::Describe(sparse),
      internal_sparse_cuda::Describe(dense), {});
}

/**
 * @brief Enqueues the experimental CUDA SparseGatherZero operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] dense The dense value required by this contract.
 * @param[in] sparse The sparse value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseGatherZero(
    SparseCudaContext& context, SparseBlasVectorView<Element> dense,
    SparseBlasIndexedVectorView<Element> sparse) {
  return internal_sparse_cuda::StandardLevel1Erased(
      context, internal_sparse_cuda::StandardOperation::kGatherZero,
      SparseBlasConjugation::kUnconjugated,
      internal_sparse_cuda::Scalar(Element{}),
      internal_sparse_cuda::Describe(sparse),
      internal_sparse_cuda::Describe(dense), {});
}

/**
 * @brief Enqueues the experimental CUDA SparseScatter operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] sparse The sparse value required by this contract.
 * @param[in] dense The dense value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseScatter(
    SparseCudaContext& context,
    SparseBlasIndexedVectorView<const Element> sparse,
    SparseBlasVectorView<Element> dense) {
  return internal_sparse_cuda::StandardLevel1Erased(
      context, internal_sparse_cuda::StandardOperation::kScatter,
      SparseBlasConjugation::kUnconjugated,
      internal_sparse_cuda::Scalar(Element{}),
      internal_sparse_cuda::Describe(sparse),
      internal_sparse_cuda::Describe(dense), {});
}

/**
 * @brief Enqueues the experimental CUDA Spmv operation and returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] transpose Requested transpose/conjugation mode.
 * @param[in] alpha Scaling factor applied to the primary operation.
 * @param[in] matrix The matrix value required by this contract.
 * @param[in] input Input operand, valid and accessible for the operation.
 * @param[out] output Output operand mutated only as documented by the
 * operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSpmv(
    SparseCudaContext& context, SparseBlasTranspose transpose, Element alpha,
    CsrView<const Element> matrix, SparseBlasVectorView<const Element> input,
    SparseBlasVectorView<Element> output) {
  return internal_sparse_cuda::StandardSpmvErased(
      context, transpose, internal_sparse_cuda::Scalar(alpha),
      internal_sparse_cuda::Describe(matrix),
      internal_sparse_cuda::Describe(input),
      internal_sparse_cuda::Describe(output));
}

/**
 * @brief Enqueues the experimental CUDA Spmm operation and returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] transpose Requested transpose/conjugation mode.
 * @param[in] alpha Scaling factor applied to the primary operation.
 * @param[in] matrix The matrix value required by this contract.
 * @param[in] input Input operand, valid and accessible for the operation.
 * @param[out] output Output operand mutated only as documented by the
 * operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSpmm(
    SparseCudaContext& context, SparseBlasTranspose transpose, Element alpha,
    CsrView<const Element> matrix, SparseBlasMatrixView<const Element> input,
    SparseBlasMatrixView<Element> output) {
  return internal_sparse_cuda::StandardSpmmErased(
      context, transpose, internal_sparse_cuda::Scalar(alpha),
      internal_sparse_cuda::Describe(matrix),
      internal_sparse_cuda::Describe(input),
      internal_sparse_cuda::Describe(output));
}

/**
 * @brief Enqueues the experimental CUDA SparseTriangularSolve operation and
 * returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] transpose Requested transpose/conjugation mode.
 * @param[in] alpha Scaling factor applied to the primary operation.
 * @param[in] triangular The triangular value required by this contract.
 * @param[in] right_hand_side The right hand side value required by this
 * contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseTriangularSolve(
    SparseCudaContext& context, SparseBlasTranspose transpose, Element alpha,
    const CudaTriangularCsrArray<Element>& triangular,
    SparseBlasVectorView<Element> right_hand_side) {
  auto matrix = triangular.matrix();
  if (!matrix.ok()) {
    return matrix.status();
  }
  internal_sparse_cuda::MatrixDescriptor rhs;
  rhs.data = right_hand_side.data();
  rhs.reachable_data = right_hand_side.reachable_storage().data();
  rhs.reachable_size = right_hand_side.reachable_storage().size();
  rhs.memory_space = right_hand_side.memory_space();
  rhs.element_kind = internal_sparse_cuda::Kind<Element>();
  rhs.rows = right_hand_side.size();
  rhs.columns = 1;
  rhs.leading_dimension = right_hand_side.increment();
  rhs.layout = SparseBlasLayout::kRowMajor;
  rhs.writable = true;
  return internal_sparse_cuda::StandardTriangularSolveErased(
      context, transpose, internal_sparse_cuda::Scalar(alpha),
      internal_sparse_cuda::Describe(*matrix), triangular.triangle(),
      triangular.diagonal(), rhs);
}

/**
 * @brief Enqueues the experimental CUDA SparseTriangularSolveMultiple operation
 * and returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Element Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] transpose Requested transpose/conjugation mode.
 * @param[in] alpha Scaling factor applied to the primary operation.
 * @param[in] triangular The triangular value required by this contract.
 * @param[in] right_hand_sides The right hand sides value required by this
 * contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <SparseBlasScalar Element>
[[nodiscard]] Result<CompletionEvent> CudaSparseTriangularSolveMultiple(
    SparseCudaContext& context, SparseBlasTranspose transpose, Element alpha,
    const CudaTriangularCsrArray<Element>& triangular,
    SparseBlasMatrixView<Element> right_hand_sides) {
  auto matrix = triangular.matrix();
  if (!matrix.ok()) {
    return matrix.status();
  }
  return internal_sparse_cuda::StandardTriangularSolveErased(
      context, transpose, internal_sparse_cuda::Scalar(alpha),
      internal_sparse_cuda::Describe(*matrix), triangular.triangle(),
      triangular.diagonal(), internal_sparse_cuda::Describe(right_hand_sides));
}

/**
 * @brief Enqueues the experimental CUDA CsrSpmvWorkspaceSize operation and
 * returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] matrix The matrix value required by this contract.
 * @param[in] input Input operand, valid and accessible for the operation.
 * @param[out] output Output operand mutated only as documented by the
 * operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
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

/**
 * @brief Enqueues the experimental CUDA CsrSpmv operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] alpha Scaling factor applied to the primary operation.
 * @param[in] matrix The matrix value required by this contract.
 * @param[in] input Input operand, valid and accessible for the operation.
 * @param[in] beta Scaling factor applied to the prior output value.
 * @param[out] output Output operand mutated only as documented by the
 * operation.
 * @param[in,out] workspace Caller-owned workspace with the documented capacity
 * and lifetime.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
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

/**
 * @brief Enqueues the experimental CUDA Evaluate operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Expression Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Rank Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] expression The expression value required by this contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
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

/**
 * @brief Enqueues the experimental CUDA Evaluate operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Expression Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Format Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] expression The expression value required by this contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
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
