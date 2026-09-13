#ifndef ASC_TESTS_DENSE_LAPACK_PRECISION_CONVERSION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_PRECISION_CONVERSION_TEST_SUPPORT_H_
#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_precision_conversion.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export
namespace asc_conversion_test {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kLayout;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::kScratch;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::WithoutAllocation;
template <typename Input>
using Output = std::conditional_t<
    std::is_same_v<Input, float>, double,
    std::conditional_t<
        std::is_same_v<Input, double>, float,
        std::conditional_t<std::is_same_v<Input, std::complex<float>>,
                           std::complex<double>, std::complex<float>>>>;
template <typename Input>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasMatrixView<const Input> input,
           asc::DenseBlasMatrixView<Output<Input>> output) {
  if constexpr (std::is_same_v<Input, float>) {
    return asc::QuerySlag2dWorkspace(provider, input, output);
  } else if constexpr (std::is_same_v<Input, double>) {
    return asc::QueryDlag2sWorkspace(provider, input, output);
  } else if constexpr (std::is_same_v<Input, std::complex<float>>) {
    return asc::QueryClag2zWorkspace(provider, input, output);
  } else {
    return asc::QueryZlag2cWorkspace(provider, input, output);
  }
}
template <typename Input>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasMatrixView<const Input> input,
                    asc::DenseBlasMatrixView<Output<Input>> output,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (std::is_same_v<Input, float>) {
    return asc::Slag2d(provider, input, output, plan, workspace, report);
  } else if constexpr (std::is_same_v<Input, double>) {
    return asc::Dlag2s(provider, input, output, plan, workspace, report);
  } else if constexpr (std::is_same_v<Input, std::complex<float>>) {
    return asc::Clag2z(provider, input, output, plan, workspace, report);
  } else {
    return asc::Zlag2c(provider, input, output, plan, workspace, report);
  }
}
template <typename Input>
struct Problem {
  Rhs<Input> input;
  Rhs<Output<Input>> output;
  Problem(asc::extent_t m, asc::extent_t n, asc::DenseBlasLayout il,
          asc::DenseBlasLayout ol)
      : input(m, n, il), output(m, n, ol) {}
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return asc_conversion_test::Query<Input>(
        provider, std::as_const(input).View(), output.View());
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
    return Execute<Input>(provider, std::as_const(input).View(), output.View(),
                          plan, workspace, report);
  }
};
template <typename Input>
struct Scratch {
  std::array<Input, 256> packed{};
  std::array<Output<Input>, 256> staged{};
  Scratch() {
    packed.fill(Value<Input>(-13, 5));
    staged.fill(Value<Output<Input>>(-17, 7));
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    const auto set = [&](std::size_t role, auto& data) {
      const auto count = plan.regions[role].minimum_entries;
      if (count < 0 || static_cast<std::size_t>(count) > data.size() - 2) {
        std::abort();
      }
      if (count > 0) {
        result.regions[role] = {
            data.data() + 1, static_cast<std::size_t>(count) * sizeof(data[0]),
            kHost};
      }
    };
    set(kLayout, packed);
    set(kScratch, staged);
    return result;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    const auto check = [&](std::size_t role, const auto& data, auto sentinel) {
      const auto end = 1 + workspace.regions[role].size() / sizeof(data[0]);
      for (std::size_t i = 0; i < data.size(); ++i) {
        if (i == 0 || i >= end) {
          ASC_DENSE_TEST_EQ(test, data[i], sentinel);
        }
      }
    };
    check(kLayout, packed, Value<Input>(-13, 5));
    check(kScratch, staged, Value<Output<Input>>(-17, 7));
  }
};
}  // namespace asc_conversion_test
#endif  // ASC_TESTS_DENSE_LAPACK_PRECISION_CONVERSION_TEST_SUPPORT_H_
