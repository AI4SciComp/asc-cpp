#include <type_traits>

#include "asc/core/extents.h"
#include "asc/random/providers/sparse_cuda.h"

using Shape = asc::Extents<2, 3>;
using Array = asc::CudaCoordinateArray<double, Shape>;
using Generation = asc::CudaSparseUniform01Generation<double, Shape>;

static_assert(!std::is_copy_constructible_v<Array>);
static_assert(!std::is_copy_assignable_v<Array>);
static_assert(std::is_move_constructible_v<Array>);
static_assert(std::is_move_assignable_v<Array>);
static_assert(!std::is_copy_constructible_v<Generation>);
static_assert(!std::is_copy_assignable_v<Generation>);
static_assert(std::is_move_constructible_v<Generation>);
static_assert(std::is_move_assignable_v<Generation>);

int main() { return 0; }
