#ifndef ASC_TESTS_COMPILE_M3_DENSE_MULTI_TU_H_
#define ASC_TESTS_COMPILE_M3_DENSE_MULTI_TU_H_

#include <array>
#include <cstddef>

#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"

template <std::size_t Rank>
asc::extent_t M3DenseOffset(const std::array<asc::extent_t, Rank>& shape,
                            const std::array<asc::index_t, Rank>& coordinate) {
  const auto mapping =
      asc::DenseLayoutMapping<Rank>::Create(asc::LayoutLeft{}, shape);
  if (!mapping.ok()) {
    return -1;
  }
  const auto offset = mapping->Offset(coordinate);
  return offset.ok() ? *offset : -1;
}

int M3DenseMultiTuA();
int M3DenseMultiTuB();

#endif  // ASC_TESTS_COMPILE_M3_DENSE_MULTI_TU_H_
