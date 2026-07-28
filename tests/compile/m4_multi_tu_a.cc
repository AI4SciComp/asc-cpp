#include <array>
#include <span>
#include <utility>

#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/sparse/coordinate.h"
#include "m4_multi_tu.h"

double M4CoordinateFromFirstTranslationUnit() {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  asc::HostMemoryResource resource;
  auto extents = Shape::Create(2, 2);
  if (!extents.ok()) {
    return -1.0;
  }
  auto builder =
      asc::CoordinateBuilder<double, Shape>::Create(resource, *extents, 2);
  if (!builder.ok()) {
    return -2.0;
  }
  constexpr std::array<asc::index_t, 2> kFirst{1, 1};
  constexpr std::array<asc::index_t, 2> kSecond{0, 0};
  if (!builder->Add(kFirst, 5.0).ok() || !builder->Add(kSecond, 3.0).ok()) {
    return -3.0;
  }
  auto owner = std::move(*builder).Finalize(asc::ExecutionContext::Serial(),
                                            asc::DuplicatePolicy::kReject,
                                            asc::ExplicitZeroPolicy::kKeep);
  if (!owner.ok()) {
    return -4.0;
  }
  auto view = owner->view();
  if (!view.ok()) {
    return -5.0;
  }
  auto value = view->Lookup(kFirst);
  return value.ok() ? *value : -6.0;
}
