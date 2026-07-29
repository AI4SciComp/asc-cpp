#include <concepts>
#include <type_traits>

#include "asc/random/sparse.h"

namespace {

using Shape = asc::Extents<2, 3>;

template <typename Element>
concept HasSparseGeneration = requires(
    const asc::ExecutionContext& context, Shape shape, asc::nnz_t count,
    asc::MemoryResource& resource, asc::RandomStream stream,
    asc::RandomSubsequence subsequence, asc::RandomOffset offset) {
  {
    asc::GenerateSparseUniform01<Element>(context, shape, count, resource,
                                          stream, subsequence, offset,
                                          stream + 1, subsequence, offset)
  }
  -> std::same_as<asc::Result<asc::SparseUniform01Generation<Element, Shape>>>;
};

static_assert(HasSparseGeneration<float>);
static_assert(HasSparseGeneration<double>);
static_assert(!HasSparseGeneration<int>);
static_assert(!HasSparseGeneration<const float>);
static_assert(!std::is_copy_constructible_v<
              asc::SparseUniform01Generation<float, Shape>>);
static_assert(
    std::is_move_constructible_v<asc::SparseUniform01Generation<float, Shape>>);

}  // namespace

int main() {
  auto shape = Shape::Create();
  asc::HostMemoryResource resource;
  if (!shape.ok()) {
    return 1;
  }
  auto generated = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *shape, 2, resource, 1, 2, 3, 4, 5, 6);
  return generated.ok() ? 0 : 2;
}
