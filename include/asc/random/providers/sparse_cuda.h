#ifndef ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_
#define ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_

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
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/engine.h"
#include "asc/random/providers/sparse_cuda_export.h"
#include "asc/sparse/coordinate.h"

namespace asc {
namespace internal_random_sparse_cuda {

class Access;

enum class ElementKind {
  kFloat,   ///< Selects float behavior.
  kDouble,  ///< Selects double behavior.
};

struct GenerationBuffers {
  Buffer coordinates;
  Buffer values;
  CompletionEvent completion;
  RandomOffset next_structure_offset;
  RandomOffset next_value_offset;
};

ASC_RANDOM_SPARSE_CUDA_EXPORT Result<GenerationBuffers>
GenerateSparseUniform01Erased(
    const ExecutionContext& context, std::span<const extent_t> extents,
    extent_t logical_size, nnz_t exact_count, ElementKind element_kind,
    MemoryResource& resource, RandomStream structure_stream,
    RandomSubsequence structure_subsequence, RandomOffset structure_offset,
    RandomStream value_stream, RandomSubsequence value_subsequence,
    RandomOffset value_offset);

}  // namespace internal_random_sparse_cuda

/**
 * @brief Owns an experimental CUDA coordinate-array generation result.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
class CudaCoordinateArray {
 public:
  /**
   * @brief Constructs a CudaCoordinateArray with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaCoordinateArray(const CudaCoordinateArray&) = delete;
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
  CudaCoordinateArray& operator=(const CudaCoordinateArray&) = delete;
  /**
   * @brief Constructs a CudaCoordinateArray with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaCoordinateArray(CudaCoordinateArray&&) noexcept = default;
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
  CudaCoordinateArray& operator=(CudaCoordinateArray&&) noexcept = default;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  ~CudaCoordinateArray() = default;

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
    return coordinates_.valid() && values_.valid();
  }

  /**
   * @brief Returns the object's extents contract value.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }
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
  [[nodiscard]] Result<CoordinateView<Element, ExtentsType::kRank>> view() {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CUDA coordinate array has no view");
    }
    return internal_sparse_coordinate::ProviderAccess::MakeCanonicalView(
        static_cast<const index_t*>(coordinates_.data()),
        static_cast<Element*>(values_.data()), Shape(), nonzeros_,
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
  [[nodiscard]] Result<CoordinateView<const Element, ExtentsType::kRank>> view()
      const {
    if (!valid()) {
      return Status(ErrorCode::kInvalidState,
                    "A moved-from CUDA coordinate array has no view");
    }
    return internal_sparse_coordinate::ProviderAccess::MakeCanonicalView(
        static_cast<const index_t*>(coordinates_.data()),
        static_cast<const Element*>(values_.data()), Shape(), nonzeros_,
        MemorySpace::kDevice);
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
  friend class internal_random_sparse_cuda::Access;

  CudaCoordinateArray(ExtentsType extents, nnz_t nonzeros, Buffer coordinates,
                      Buffer values) noexcept
      : extents_(std::move(extents)),
        nonzeros_(nonzeros),
        coordinates_(std::move(coordinates)),
        values_(std::move(values)) {}

  [[nodiscard]] std::array<extent_t, ExtentsType::kRank> Shape()
      const noexcept {
    std::array<extent_t, ExtentsType::kRank> shape{};
    for (std::size_t dimension = 0; dimension < ExtentsType::kRank;
         ++dimension) {
      shape[dimension] = extents_.values()[dimension];
    }
    return shape;
  }

  ExtentsType extents_;
  nnz_t nonzeros_;
  Buffer coordinates_;
  Buffer values_;
};

namespace internal_random_sparse_cuda {

class Access {
 public:
  template <typename Element, SparseExtents ExtentsType>
    requires(std::same_as<Element, float> || std::same_as<Element, double>)
  static CudaCoordinateArray<Element, ExtentsType> MakeArray(
      ExtentsType extents, nnz_t nonzeros, Buffer coordinates, Buffer values) {
    return CudaCoordinateArray<Element, ExtentsType>(
        std::move(extents), nonzeros, std::move(coordinates),
        std::move(values));
  }
};

}  // namespace internal_random_sparse_cuda

/**
 * @brief Owns an experimental CUDA sparse random generation result.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaSparseUniform01Generation {
  /**
   * @brief Stores the array value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CudaCoordinateArray<Element, ExtentsType> array;
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
  /**
   * @brief Stores the next structure offset value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  RandomOffset next_structure_offset;
  /**
   * @brief Stores the next value offset value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  RandomOffset next_value_offset;
};

/**
 * @brief Enqueues the experimental CUDA GenerateSparseUniform01 operation and
 * returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam ExtentsType Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] extents Logical extents; every extent must satisfy the documented
 * bounds.
 * @param[in] exact_count The exact count value required by this contract.
 * @param[in] resource Allocator that must outlive storage allocated from it.
 * @param[in] structure_stream The structure stream value required by this
 * contract.
 * @param[in] structure_subsequence The structure subsequence value required by
 * this contract.
 * @param[in] structure_offset The structure offset value required by this
 * contract.
 * @param[in] value_stream The value stream value required by this contract.
 * @param[in] value_subsequence The value subsequence value required by this
 * contract.
 * @param[in] value_offset The value offset value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CudaSparseUniform01Generation<Element, ExtentsType>>
CudaGenerateSparseUniform01(
    const ExecutionContext& context, ExtentsType extents, nnz_t exact_count,
    MemoryResource& resource, RandomStream structure_stream,
    RandomSubsequence structure_subsequence, RandomOffset structure_offset,
    RandomStream value_stream, RandomSubsequence value_subsequence,
    RandomOffset value_offset) {
  auto generation = internal_random_sparse_cuda::GenerateSparseUniform01Erased(
      context, extents.values(), extents.logical_size(), exact_count,
      std::same_as<Element, float>
          ? internal_random_sparse_cuda::ElementKind::kFloat
          : internal_random_sparse_cuda::ElementKind::kDouble,
      resource, structure_stream, structure_subsequence, structure_offset,
      value_stream, value_subsequence, value_offset);
  if (!generation.ok()) {
    return generation.status();
  }
  auto array =
      internal_random_sparse_cuda::Access::MakeArray<Element, ExtentsType>(
          std::move(extents), exact_count, std::move(generation->coordinates),
          std::move(generation->values));
  return CudaSparseUniform01Generation<Element, ExtentsType>{
      .array = std::move(array),
      .completion = std::move(generation->completion),
      .next_structure_offset = generation->next_structure_offset,
      .next_value_offset = generation->next_value_offset};
}

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_
