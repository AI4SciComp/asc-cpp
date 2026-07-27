#include <array>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/sparse/coordinate.h"
#include "m4_sparse_multi_tu.h"

double M4CoordinateValue() {
  using Extents = asc::Extents<2>;
  const auto extents = Extents::Create();
  if (!extents.ok()) {
    return -1.0;
  }
  asc::HostMemoryResource resource;
  auto builder =
      asc::CoordinateBuilder<double, Extents>::Create(*extents, 1, resource);
  if (!builder.ok() ||
      !builder->Add(std::array<asc::index_t, 1>{1}, 7.0).ok()) {
    return -1.0;
  }
  auto owner = builder->Finalize(asc::ExecutionContext::Serial(),
                                 asc::DuplicatePolicy::kReject,
                                 asc::ExplicitZeroPolicy::kKeep);
  if (!owner.ok()) {
    return -1.0;
  }
  auto view = owner->view();
  auto value = view->Find(std::array<asc::index_t, 1>{1});
  return value.ok() && *value != nullptr ? **value : -1.0;
}
