#include <type_traits>

#include "asc/sparse/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::SparseCudaContext>);
static_assert(!std::is_copy_assignable_v<asc::SparseCudaContext>);
static_assert(std::is_move_constructible_v<asc::SparseCudaContext>);
static_assert(std::is_move_assignable_v<asc::SparseCudaContext>);
static_assert(!std::is_copy_constructible_v<asc::CudaCsrArray<float>>);
static_assert(std::is_move_constructible_v<asc::CudaCsrArray<float>>);

int main() { return 0; }
