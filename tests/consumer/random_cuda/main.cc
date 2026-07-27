#include <asc/random/providers/cuda.h>

#include <cstdint>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<asc::CudaRandomWordGeneration>);

int main() {
  const auto generation = asc::CudaFillPhilox4x32(
      asc::ExecutionContext::Serial(), static_cast<std::uint32_t*>(nullptr), 0,
      asc::RandomStream{1}, asc::RandomSubsequence{2}, asc::RandomOffset{3});
  static_cast<void>(generation);
  return 0;
}
