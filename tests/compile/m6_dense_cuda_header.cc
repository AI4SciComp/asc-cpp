#include <type_traits>

#include "asc/dense/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::DenseCudaContext>);
static_assert(!std::is_copy_assignable_v<asc::DenseCudaContext>);
static_assert(std::is_move_constructible_v<asc::DenseCudaContext>);
static_assert(std::is_move_assignable_v<asc::DenseCudaContext>);

int main() { return 0; }
