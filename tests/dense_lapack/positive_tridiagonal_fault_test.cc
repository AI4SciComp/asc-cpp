#include <array>
#include <complex>
#include <cstddef>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "installed_lu/normal_return_guard.h"
#include "positive_tridiagonal_faults.h"
#include "tridiagonal_test_support.h"

namespace {
namespace support = asc_tridiagonal_test;
namespace fault = asc_pt_fault;
using support::Take;
using support::TestContext;
using support::Vector;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto mode :
       {fault::Mode::kNegative, fault::Mode::kExcess, fault::Mode::kUnwritten,
        fault::Mode::kPartialWidth, fault::Mode::kWriteThenNegative}) {
    std::array<Real, 5> d{-37, 2, 3, 5, -37};
    std::array<T, 4> e{T{-41}, T{}, T{}, T{-41}};
    const auto matrix =
        Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(
            Vector(d, 3), Vector(e, 2)));
    const auto plan = Take(asc::QueryPttrfWorkspace(provider, matrix));
    asc::LapackReport report;
    fault::Reset(mode);
    ASC_DENSE_TEST_EQ(test,
                      asc::Pttrf(provider, matrix, plan, {}, report).code(),
                      asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
    ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(
        test, d[1],
        mode == fault::Mode::kWriteThenNegative ? Real{-17} : Real{2});
    ASC_DENSE_TEST_EQ(test, d.front(), Real{-37});
    ASC_DENSE_TEST_EQ(test, d.back(), Real{-37});
    d[1] = 2;
    const auto factor =
        Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::FromRaw(
            provider, kLower, Vector(std::as_const(d), 3),
            Vector(std::as_const(e), 2)));
    for (const auto layout : {support::kRow, support::kColumn}) {
      support::Rhs<T> rhs(3, 2, layout);
      rhs.At(0, 0) = T{7};
      const auto before = rhs;
      const auto solve =
          Take(asc::QueryPttrsWorkspace(provider, factor, rhs.View()));
      support::Scratch<T> scratch;
      fault::Reset(mode);
      ASC_DENSE_TEST_EQ(test,
                        asc::Pttrs(provider, factor, rhs.View(), solve,
                                   scratch.Workspace(solve), report)
                            .code(),
                        asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
      ASC_DENSE_TEST_CHECK(test, fault::LengthsValid());
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      if (layout == support::kRow || mode != fault::Mode::kWriteThenNegative) {
        ASC_DENSE_TEST_CHECK(test, rhs.data == before.data);
      } else {
        ASC_DENSE_TEST_EQ(test, rhs.At(0, 0), T{-19});
      }
      ASC_DENSE_TEST_EQ(test, d[1], Real{2});
      rhs.Guards(test);
      // The same configured wrapper proves rejection occurs before entry.
      auto wrong = solve;
      ++wrong.regions[support::kLayout].minimum_entries;
      const auto rejected_before = rhs;
      fault::Reset(mode);
      ASC_DENSE_TEST_CHECK(test,
                           !asc::Pttrs(provider, factor, rhs.View(), wrong,
                                       scratch.Workspace(solve), report)
                                .ok());
      ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
      ASC_DENSE_TEST_CHECK(test, rhs.data == rejected_before.data);
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
