#ifndef ASC_TESTS_COMPILE_M5_RANDOM_FACETS_ODR_H_
#define ASC_TESTS_COMPILE_M5_RANDOM_FACETS_ODR_H_

#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"

inline asc::Result<asc::RandomOffset> FillM5OdrDense(
    std::span<float, 4> storage, asc::RandomOffset offset) {
  constexpr std::array<asc::extent_t, 1> kExtents{4};
  auto mapping =
      asc::DenseLayout<1>::Create(std::span<const asc::extent_t, 1>(kExtents));
  if (!mapping.ok()) {
    return mapping.status();
  }
  auto view = asc::DenseView<float, 1>::Create(storage.data(), *mapping,
                                               asc::MemorySpace::kHost);
  if (!view.ok()) {
    return view.status();
  }
  return asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2,
                                 offset);
}

using M5OdrShape = asc::Extents<2, 2>;

inline asc::Result<asc::SparseUniform01Generation<float, M5OdrShape>>
GenerateM5OdrSparse(asc::MemoryResource& resource, asc::RandomOffset offset) {
  auto shape = M5OdrShape::Create();
  if (!shape.ok()) {
    return shape.status();
  }
  return asc::GenerateSparseUniform01<float>(asc::ExecutionContext::Serial(),
                                             *shape, 2, resource, 3, 4, offset,
                                             5, 6, offset);
}

#endif  // ASC_TESTS_COMPILE_M5_RANDOM_FACETS_ODR_H_
