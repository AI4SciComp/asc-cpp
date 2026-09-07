#include <concepts>
#include <cstdint>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/view.h"
#include "asc/random/engine.h"
#include "asc/random/providers/cuda.h"
#include "asc/random/providers/dense_cuda.h"
#include "asc/random/providers/sparse_cuda.h"
#include "asc/sparse/providers/cuda.h"

static_assert(
    std::same_as<decltype(asc::CudaFillPhilox4x32(
                     std::declval<const asc::ExecutionContext&>(),
                     std::declval<asc::MutableMemoryView>(), std::uint64_t{1},
                     asc::RandomStream{2}, asc::RandomSubsequence{3},
                     asc::RandomOffset{4})),
                 asc::Result<asc::CudaRandomWordGeneration>>);

using DenseGeneration = asc::CudaDenseUniform01Generation<float, 2>;
static_assert(std::same_as<decltype(asc::CudaFillDenseUniform01(
                               std::declval<const asc::ExecutionContext&>(),
                               std::declval<asc::DenseView<float, 2>>(),
                               asc::RandomStream{2}, asc::RandomSubsequence{3},
                               asc::RandomOffset{4})),
                           asc::Result<DenseGeneration>>);

using Shape = asc::Extents<2, 3>;
using SparseGeneration = asc::CudaSparseUniform01Generation<double, Shape>;
static_assert(
    std::same_as<decltype(asc::CudaGenerateSparseUniform01<double>(
                     std::declval<const asc::ExecutionContext&>(),
                     std::declval<Shape>(), 1,
                     std::declval<asc::MemoryResource&>(), asc::RandomStream{2},
                     asc::RandomSubsequence{3}, asc::RandomOffset{4},
                     asc::RandomStream{5}, asc::RandomSubsequence{6},
                     asc::RandomOffset{7})),
                 asc::Result<SparseGeneration>>);

static_assert(std::same_as<decltype(asc::SparseCudaContext::Create(
                               std::declval<asc::ExecutionContext>())),
                           asc::Result<asc::SparseCudaContext>>);

int main() { return 0; }
