#ifndef ASC_TESTS_DENSE_LAPACK_ROTATION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_ROTATION_TEST_SUPPORT_H_

#include <array>
#include <type_traits>

#include "../../src/dense/lapack/internal_rotation_prototypes.h"
#include "asc/core/memory.h"
#include "asc/dense/blas.h"
#include "installed_lu/rotation_numerical.h"  // IWYU pragma: export
#include "tridiagonal_test_support.h"         // IWYU pragma: export

namespace asc_rotation_test {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;

template <typename T>
struct Output {
  std::array<T, 3> values{T{71}, T{-19}, T{73}};
  auto View() {
    return Take(asc::DenseBlasVectorView<T>::Create(
        values.data() + 1, 1, 1, {values.data(), sizeof(values), kHost}));
  }
};

template <typename T>
void Native(T& f, T& g, T& c, T& s, T& r) {
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_slartg(&f, &g, &c, &s, &r);
  } else {
    ASC_LAPACK_dlartg(&f, &g, &c, &s, &r);
  }
}
}  // namespace asc_rotation_test

#endif  // ASC_TESTS_DENSE_LAPACK_ROTATION_TEST_SUPPORT_H_
