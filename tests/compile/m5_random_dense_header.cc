#include <array>
#include <concepts>
#include <span>
#include <type_traits>

#include "asc/random/dense.h"

namespace {

template <typename Element>
concept HasDenseFill =
    requires(const asc::ExecutionContext& context,
             asc::DenseView<Element, 1> view, asc::RandomStream stream,
             asc::RandomSubsequence subsequence, asc::RandomOffset offset) {
      {
        asc::FillDenseUniform01(context, view, stream, subsequence, offset)
      } -> std::same_as<asc::Result<asc::RandomOffset>>;
    };

static_assert(HasDenseFill<float>);
static_assert(HasDenseFill<double>);
static_assert(!HasDenseFill<int>);
static_assert(!HasDenseFill<const float>);

}  // namespace

int main() {
  constexpr std::array<asc::extent_t, 1> kExtents{1};
  auto mapping =
      asc::DenseLayout<1>::Create(std::span<const asc::extent_t, 1>(kExtents));
  float value = 0.0F;
  if (!mapping.ok()) {
    return 1;
  }
  auto view = asc::DenseView<float, 1>::Create(&value, *mapping,
                                               asc::MemorySpace::kHost);
  if (!view.ok()) {
    return 2;
  }
  return asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2,
                                 3)
                 .ok()
             ? 0
             : 3;
}
