#include <type_traits>

#include "asc/random/providers/dense_cuda.h"

using Generation = asc::CudaDenseUniform01Generation<float, 2>;

static_assert(!std::is_copy_constructible_v<Generation>);
static_assert(!std::is_copy_assignable_v<Generation>);
static_assert(std::is_move_constructible_v<Generation>);
static_assert(std::is_move_assignable_v<Generation>);

int main() { return 0; }
