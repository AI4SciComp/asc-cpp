#ifndef ASC_TESTS_DENSE_LAPACK_TRIANGULAR_CONVERSION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_TRIANGULAR_CONVERSION_TEST_SUPPORT_H_
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

#include "../../src/dense/lapack/internal_triangular_conversion_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack_precision_conversion.h"
#include "precision_conversion_test_support.h"  // IWYU pragma: export
namespace asc_triangular_conversion_test {
using asc_conversion_test::kColumn;
using asc_conversion_test::kHost;
using asc_conversion_test::kLayout;
using asc_conversion_test::kRow;
using asc_conversion_test::kScratch;
using asc_conversion_test::Output;
using asc_conversion_test::Rhs;
using asc_conversion_test::Scratch;
using asc_conversion_test::Take;
using asc_conversion_test::TestContext;
using asc_conversion_test::ToWide;
using asc_conversion_test::Value;
using asc_conversion_test::WithoutAllocation;
inline constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
inline constexpr auto kLower = asc::DenseBlasTriangle::kLower;
inline bool Selected(asc::DenseBlasTriangle triangle, asc::extent_t i,
                     asc::extent_t j) {
  return triangle == kUpper ? i <= j : i >= j;
}
template <typename Input>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle,
           asc::DenseBlasMatrixView<const Input> input,
           asc::DenseBlasMatrixView<Output<Input>> output) {
  if constexpr (std::is_same_v<Input, double>) {
    return asc::QueryDlat2sWorkspace(provider, triangle, input, output);
  } else {
    return asc::QueryZlat2cWorkspace(provider, triangle, input, output);
  }
}
template <typename Input>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle,
                    asc::DenseBlasMatrixView<const Input> input,
                    asc::DenseBlasMatrixView<Output<Input>> output,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (std::is_same_v<Input, double>) {
    return asc::Dlat2s(provider, triangle, input, output, plan, workspace,
                       report);
  } else {
    return asc::Zlat2c(provider, triangle, input, output, plan, workspace,
                       report);
  }
}
template <typename Input>
struct Problem {
  Rhs<Input> input;
  Rhs<Output<Input>> output;
  asc::DenseBlasTriangle triangle;
  Problem(asc::extent_t n, asc::DenseBlasTriangle uplo, asc::DenseBlasLayout il,
          asc::DenseBlasLayout ol)
      : input(n, n, il), output(n, n, ol), triangle(uplo) {}
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return asc_triangular_conversion_test::Query<Input>(
        provider, triangle, std::as_const(input).View(), output.View());
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
    return Execute<Input>(provider, triangle, std::as_const(input).View(),
                          output.View(), plan, workspace, report);
  }
};
inline double Component(std::size_t index) {
  constexpr double kTiny = std::numeric_limits<float>::denorm_min();
  constexpr double kSmall = std::numeric_limits<float>::min();
  constexpr double kLarge = std::numeric_limits<float>::max();
  const std::array<double, 16> values{
      0,
      -0.0,
      0.125,
      -0.5,
      kTiny,
      -kTiny,
      kSmall,
      -kSmall,
      kLarge,
      -kLarge,
      2 * kTiny,
      0.5 * kTiny,
      1.5 * kTiny,
      -1.5 * kTiny,
      1 + std::numeric_limits<float>::epsilon() / 2,
      1 + 1.5 * std::numeric_limits<float>::epsilon()};
  return values[index % values.size()];
}
template <typename Input>
void Initialize(Problem<Input>& p, std::size_t shift = 0) {
  for (asc::extent_t i = 0; i < p.input.rows; ++i) {
    for (asc::extent_t j = 0; j < p.input.columns; ++j) {
      const auto index = static_cast<std::size_t>(i + 3 * j) + shift;
      // Out-of-range values in the unused triangle must not affect INFO.
      p.input.At(i, j) =
          Selected(p.triangle, i, j)
              ? Value<Input>(Component(index), Component(index + 7))
              : Value<Input>(std::numeric_limits<double>::max(),
                             -std::numeric_limits<double>::max());
    }
  }
}
template <typename Input>
void Native(char triangle, lapack_int n, const Input* input, lapack_int lda,
            Output<Input>* output, lapack_int ldout, lapack_int& info) {
  if constexpr (std::is_same_v<Input, double>) {
    LAPACK_dlat2s_base(&triangle, &n, input, &lda, output, &ldout, &info,
                       std::size_t{1});
  } else {
    LAPACK_zlat2c_base(&triangle, &n, input, &lda, output, &ldout, &info,
                       std::size_t{1});
  }
}
template <typename T>
bool Same(T actual, T expected) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return Same(actual.real(), expected.real()) &&
           Same(actual.imag(), expected.imag());
  } else {
    return (std::isnan(expected) && std::isnan(actual)) ||
           (actual == expected &&
            std::signbit(actual) == std::signbit(expected));
  }
}
}  // namespace asc_triangular_conversion_test
#endif  // ASC_TESTS_DENSE_LAPACK_TRIANGULAR_CONVERSION_TEST_SUPPORT_H_
