#include <asc/random/providers/sparse_cuda.h>

#include <type_traits>

using Shape = asc::Extents<>;
using Generation = asc::CudaSparseUniform01Generation<float, Shape>;

static_assert(!std::is_copy_constructible_v<Generation>);
static_assert(std::is_nothrow_move_constructible_v<Generation>);

int main() { return 0; }
