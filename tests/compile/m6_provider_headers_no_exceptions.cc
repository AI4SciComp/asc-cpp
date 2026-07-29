#include <type_traits>

#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::CudaMemoryResource>);
static_assert(!std::is_copy_constructible_v<asc::DenseCudaContext>);

int main() { return 0; }
