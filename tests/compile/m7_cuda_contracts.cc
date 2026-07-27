#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/random/providers/cuda.h"
#include "asc/random/providers/dense_cuda.h"
#include "asc/random/providers/sparse_cuda.h"
#include "asc/sparse/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::SparseCudaContext>);
static_assert(!std::is_copy_assignable_v<asc::SparseCudaContext>);
static_assert(std::is_nothrow_move_constructible_v<asc::SparseCudaContext>);
static_assert(std::is_nothrow_move_assignable_v<asc::SparseCudaContext>);
static_assert(noexcept(
    std::declval<const asc::SparseCudaContext&>().execution_context()));

static_assert(!std::is_copy_constructible_v<asc::CompletionEvent>);
static_assert(std::is_nothrow_move_constructible_v<asc::CompletionEvent>);
static_assert(!std::is_copy_constructible_v<asc::CudaRandomWordGeneration>);
static_assert(
    std::is_nothrow_move_constructible_v<asc::CudaRandomWordGeneration>);
static_assert(!std::is_copy_constructible_v<asc::CudaDenseUniform01Generation>);
static_assert(
    std::is_nothrow_move_constructible_v<asc::CudaDenseUniform01Generation>);

using Shape = asc::Extents<2, 3>;
using SparseGeneration = asc::CudaSparseUniform01Generation<float, Shape>;
static_assert(!std::is_copy_constructible_v<SparseGeneration>);
static_assert(std::is_nothrow_move_constructible_v<SparseGeneration>);
static_assert(!std::is_copy_constructible_v<asc::CudaCsrClone<float>>);
static_assert(std::is_nothrow_move_constructible_v<asc::CudaCsrClone<float>>);

using ConstVector = asc::CudaStridedVectorView<const float>;
using MutableVector = asc::CudaStridedVectorView<float>;
static_assert(std::is_trivially_copyable_v<ConstVector>);
static_assert(std::is_trivially_copyable_v<MutableVector>);
static_assert(std::same_as<decltype(std::declval<ConstVector>().extent()),
                           asc::extent_t>);
static_assert(std::same_as<decltype(std::declval<ConstVector>().stride()),
                           asc::stride_t>);
static_assert(noexcept(std::declval<ConstVector>().data()));
static_assert(noexcept(std::declval<ConstVector>().extent()));
static_assert(noexcept(std::declval<ConstVector>().stride()));

static_assert(
    std::same_as<decltype(asc::CudaFillPhilox4x32(
                     std::declval<const asc::ExecutionContext&>(),
                     static_cast<std::uint32_t*>(nullptr), std::uint64_t{0},
                     asc::RandomStream{0}, asc::RandomSubsequence{0},
                     asc::RandomOffset{0})),
                 asc::Result<asc::CudaRandomWordGeneration>>);

template <typename Element>
concept SupportsCudaDenseRankEight = requires(
    const asc::ExecutionContext& context,
    asc::DenseView<Element, 8> destination) {
  {
    asc::CudaFillDenseUniform01(context, destination, asc::RandomStream{0},
                                asc::RandomSubsequence{0}, asc::RandomOffset{0})
  } -> std::same_as<asc::Result<asc::CudaDenseUniform01Generation>>;
};

template <typename Element>
concept SupportsCudaSparseRankEight =
    requires(const asc::ExecutionContext& context,
             const asc::Extents<1, 1, 1, 1, 1, 1, 1, 1>& extents,
             asc::MemoryResource& resource) {
      asc::CudaGenerateSparseUniform01<Element>(
          context, extents, asc::nnz_t{1}, resource, asc::RandomStream{1},
          asc::RandomSubsequence{2}, asc::RandomOffset{3}, asc::RandomStream{4},
          asc::RandomSubsequence{5}, asc::RandomOffset{6});
    };

static_assert(SupportsCudaDenseRankEight<float>);
static_assert(SupportsCudaDenseRankEight<double>);
static_assert(!SupportsCudaDenseRankEight<int>);
static_assert(SupportsCudaSparseRankEight<float>);
static_assert(SupportsCudaSparseRankEight<double>);
static_assert(!SupportsCudaSparseRankEight<int>);

int main() { return 0; }
