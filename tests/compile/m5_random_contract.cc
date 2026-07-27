#include <concepts>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/engine.h"
#include "asc/random/sparse.h"
#include "asc/sparse/coordinate.h"

template <typename View>
concept HasDenseUniformFill = requires(
    const asc::ExecutionContext& context, View view, asc::RandomStream stream,
    asc::RandomSubsequence subsequence, asc::RandomOffset offset) {
  {
    asc::FillDenseUniform01(context, view, stream, subsequence, offset)
  } -> std::same_as<asc::Result<asc::RandomOffset>>;
};

template <typename Element, typename ExtentsType>
concept HasSparseUniformGeneration = requires(
    const asc::ExecutionContext& context, const ExtentsType& extents,
    asc::nnz_t count, asc::MemoryResource& resource, asc::RandomStream stream,
    asc::RandomSubsequence subsequence, asc::RandomOffset offset) {
  {
    asc::GenerateSparseUniform01<Element>(context, extents, count, resource,
                                          stream, subsequence, offset,
                                          stream + 1U, subsequence, offset)
  } -> std::same_as<
        asc::Result<asc::SparseUniform01Generation<Element, ExtentsType>>>;
};

using StaticExtents = asc::Extents<2, 3>;
using DynamicExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using FloatGeneration = asc::SparseUniform01Generation<float, StaticExtents>;

static_assert(HasDenseUniformFill<asc::DenseView<float, 0>>);
static_assert(HasDenseUniformFill<asc::DenseView<double, 3>>);
static_assert(!HasDenseUniformFill<asc::DenseView<const float, 2>>);
static_assert(!HasDenseUniformFill<asc::DenseView<int, 2>>);
static_assert(HasSparseUniformGeneration<float, StaticExtents>);
static_assert(HasSparseUniformGeneration<double, DynamicExtents>);
static_assert(!HasSparseUniformGeneration<int, StaticExtents>);
static_assert(!std::copy_constructible<FloatGeneration>);
static_assert(!std::is_copy_assignable_v<FloatGeneration>);
static_assert(std::move_constructible<FloatGeneration>);
static_assert(std::is_same_v<decltype(std::declval<FloatGeneration&>().array),
                             asc::CoordinateArray<float, StaticExtents>>);
static_assert(std::is_same_v<
              decltype(std::declval<FloatGeneration&>().next_structure_offset),
              asc::RandomOffset>);
static_assert(
    std::is_same_v<decltype(std::declval<FloatGeneration&>().next_value_offset),
                   asc::RandomOffset>);

int main() { return 0; }
