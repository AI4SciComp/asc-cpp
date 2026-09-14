#ifndef ASC_TESTS_DENSE_LAPACK_VECTOR_CONJUGATION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_VECTOR_CONJUGATION_TEST_SUPPORT_H_
#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "scalar_norm_test_support.h"  // IWYU pragma: export

namespace asc_vector_conjugation_test {
using asc_scalar_norm_test::Bits;
using asc_scalar_norm_test::Take;
using asc_scalar_norm_test::TestContext;
using asc_scalar_norm_test::Values;
using asc_scalar_norm_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
template <typename T>
void Native(const lapack_int* n, std::complex<T>* data,
            const lapack_int* increment) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_clacgv(n, data, increment);
  } else {
    LAPACK_zlacgv(n, data, increment);
  }
}
template <typename T>
auto ConjugateBits(std::complex<T> value) {
  using Unsigned = decltype(Bits(T{}));
  constexpr Unsigned kSign = Unsigned{1} << (sizeof(T) * 8 - 1);
  return std::complex<T>{value.real(),
                         std::bit_cast<T>(Bits(value.imag()) ^ kSign)};
}
template <typename T>
bool Same(std::complex<T> a, std::complex<T> b) {
  return Bits(a.real()) == Bits(b.real()) && Bits(a.imag()) == Bits(b.imag());
}
template <typename T>
struct Buffer {
  using Complex = std::complex<T>;
  std::array<Complex, 128> values{};
  asc::extent_t size = 0;
  Buffer(asc::extent_t n, asc::stride_t increment, Complex value) {
    values.fill(Complex{71, 73});
    const auto magnitude = increment < 0 ? -increment : increment;
    size = n == 0 ? 0 : 1 + (n - 1) * magnitude;
    // Initialize only selected entries; holes retain a distinct sentinel.
    for (asc::extent_t i = 0; i < n; ++i) {
      values[static_cast<std::size_t>(i * magnitude) + 1] = value;
    }
  }
  auto View() {
    return Take(asc::DenseBlasVectorView<Complex>::Create(
        values.data() + 1, size, 1, {values.data(), sizeof(values), kHost}));
  }
  [[nodiscard]] auto Expected(asc::extent_t n, asc::stride_t increment) const {
    auto result = values;
    const auto magnitude = increment < 0 ? -increment : increment;
    for (asc::extent_t i = 0; i < n; ++i) {
      const auto index = static_cast<std::size_t>(
                             (increment < 0 ? n - 1 - i : i) * magnitude) +
                         1;
      result[index] = ConjugateBits(result[index]);
    }
    return result;
  }
};
}  // namespace asc_vector_conjugation_test
#endif  // ASC_TESTS_DENSE_LAPACK_VECTOR_CONJUGATION_TEST_SUPPORT_H_
