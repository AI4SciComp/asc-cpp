#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/array.h"
#include "asc/dense/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::CudaMemoryResource>);
static_assert(!std::is_move_constructible_v<asc::CudaMemoryResource>);
static_assert(std::is_copy_constructible_v<asc::ExecutionContext>);
static_assert(std::is_copy_assignable_v<asc::ExecutionContext>);
static_assert(!std::is_copy_constructible_v<asc::CompletionEvent>);
static_assert(std::is_nothrow_move_constructible_v<asc::CompletionEvent>);
static_assert(!std::is_copy_constructible_v<asc::DenseCudaContext>);
static_assert(std::is_move_constructible_v<asc::DenseCudaContext>);

int main() {
  const auto count = asc::CudaDeviceCount();
  return count.ok() && *count > 0 ? 0 : 1;
}
