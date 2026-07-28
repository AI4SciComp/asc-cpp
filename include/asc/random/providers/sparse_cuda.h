#ifndef ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_
#define ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_

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
  kFloat,
  kDouble,
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

template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
class CudaCoordinateArray {
 public:
  CudaCoordinateArray(const CudaCoordinateArray&) = delete;
  CudaCoordinateArray& operator=(const CudaCoordinateArray&) = delete;
  CudaCoordinateArray(CudaCoordinateArray&&) noexcept = default;
  CudaCoordinateArray& operator=(CudaCoordinateArray&&) noexcept = default;
  ~CudaCoordinateArray() = default;

  [[nodiscard]] bool valid() const noexcept {
    return coordinates_.valid() && values_.valid();
  }

  [[nodiscard]] const ExtentsType& extents() const noexcept { return extents_; }
  [[nodiscard]] nnz_t nnz() const noexcept { return nonzeros_; }

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

template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaSparseUniform01Generation {
  CudaCoordinateArray<Element, ExtentsType> array;
  CompletionEvent completion;
  RandomOffset next_structure_offset;
  RandomOffset next_value_offset;
};

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
