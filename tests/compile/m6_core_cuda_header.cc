#include <cstdint>
#include <memory>
#include <type_traits>

#include "asc/core/providers/cuda.h"

static_assert(!std::is_copy_constructible_v<asc::CudaMemoryResource>);
static_assert(!std::is_copy_assignable_v<asc::CudaMemoryResource>);
static_assert(!std::is_move_constructible_v<asc::CudaMemoryResource>);
static_assert(!std::is_move_assignable_v<asc::CudaMemoryResource>);
static_assert(std::is_copy_constructible_v<asc::ExecutionContext>);
static_assert(std::is_copy_assignable_v<asc::ExecutionContext>);
static_assert(!std::is_copy_constructible_v<asc::CompletionEvent>);
static_assert(!std::is_copy_assignable_v<asc::CompletionEvent>);
static_assert(std::is_move_constructible_v<asc::CompletionEvent>);
static_assert(std::is_move_assignable_v<asc::CompletionEvent>);

int main() {
  const asc::Result<std::int32_t> count = asc::CudaDeviceCount();
  return count.ok() ? 0 : static_cast<int>(count.status().code());
}
