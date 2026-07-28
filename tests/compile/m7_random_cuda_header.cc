#include <type_traits>

#include "asc/random/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::CudaRandomWordGeneration>);
static_assert(!std::is_copy_assignable_v<asc::CudaRandomWordGeneration>);
static_assert(std::is_move_constructible_v<asc::CudaRandomWordGeneration>);
static_assert(std::is_move_assignable_v<asc::CudaRandomWordGeneration>);

int main() { return 0; }
