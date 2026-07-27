#include <asc/random/providers/dense_cuda.h>

#include <array>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<asc::CudaDenseUniform01Generation>);

int main() {
  constexpr std::array<asc::extent_t, 1> kShape = {0};
  const auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kShape);
  if (!mapping.ok()) {
    return 1;
  }
  const auto view = asc::DenseView<float, 1>::Create(nullptr, *mapping,
                                                     asc::MemorySpace::kDevice);
  if (!view.ok()) {
    return 2;
  }
  const auto generation = asc::CudaFillDenseUniform01(
      asc::ExecutionContext::Serial(), *view, asc::RandomStream{1},
      asc::RandomSubsequence{2}, asc::RandomOffset{3});
  static_cast<void>(generation);
  return 0;
}
