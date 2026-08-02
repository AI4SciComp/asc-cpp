#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"

int main() {
  constexpr std::array<asc::extent_t, 1> kExtents{4};
  auto mapping =
      asc::DenseLayout<1>::Create(std::span<const asc::extent_t, 1>(kExtents));
  if (!mapping.ok()) {
    return 1;
  }
  std::array<float, 4> storage{};
  auto view = asc::DenseView<float, 1>::Create(storage.data(), *mapping,
                                               asc::MemorySpace::kHost);
  if (!view.ok()) {
    return 2;
  }
  auto next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2, 3);
  if (!next.ok() || *next != 7) {
    return 3;
  }
  for (asc::RandomOffset position = 0; position < 4; ++position) {
    const float expected =
        asc::Uniform01<float>(asc::GeneratePhilox4x32Word(1, 2, 3 + position));
    if (storage[static_cast<std::size_t>(position)] != expected) {
      return 4;
    }
  }
  constexpr std::array<asc::extent_t, 2> kPointExtents{2, 2};
  constexpr std::array<asc::extent_t, 1> kWorkspaceExtents{2};
  auto point_mapping =
      asc::DenseLayout<2>::Create(kPointExtents, asc::LayoutRight{});
  auto workspace_mapping = asc::DenseLayout<1>::Create(kWorkspaceExtents);
  std::array<double, 4> points{};
  std::array<double, 2> workspace{};
  auto point_view = asc::DenseView<double, 2>::Create(
      points.data(), *point_mapping, asc::MemorySpace::kHost);
  auto workspace_view = asc::DenseView<double, 1>::Create(
      workspace.data(), *workspace_mapping, asc::MemorySpace::kHost);
  if (!point_view.ok() || !workspace_view.ok() ||
      !asc::FillDenseSobol(asc::ExecutionContext::Serial(), *point_view, 0,
                           *workspace_view)
           .ok() ||
      points != std::array<double, 4>{0.0, 0.0, 0.5, 0.5}) {
    return 5;
  }
  return 0;
}
