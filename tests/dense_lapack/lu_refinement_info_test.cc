#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_aux_info_faults.h"
#include "lu_aux_info_test_support.h"
#include "lu_refinement_test_support.h"
namespace asc_refinement_test {
namespace aux = asc_lu_aux_info_test;
using Fault = asc_lapack_test::LuAuxInfoFault;
template <typename T>
void CheckNumericalOutputs(TestContext& test, const Sample<T>& before,
                           const Sample<T>& after,
                           asc::DenseBlasTranspose trans) {
  using Real = asc::DenseBlasRealType<T>;
  const auto epsilon = std::numeric_limits<Real>::epsilon();
  ASC_DENSE_TEST_EQ(test, before.a, after.a);
  ASC_DENSE_TEST_EQ(test, before.af, after.af);
  ASC_DENSE_TEST_EQ(test, before.b, after.b);
  ASC_DENSE_TEST_EQ(test, before.pivots, after.pivots);
  for (asc::extent_t column = 0; column < after.nrhs; ++column) {
    const auto initial_error = BackwardError(before, trans, column);
    const auto final_error = BackwardError(after, trans, column);
    ASC_DENSE_TEST_CHECK(test, final_error < initial_error / 8);
    ASC_DENSE_TEST_CHECK(test, final_error < 16 * epsilon);
    const Real ferr = after.ferr[1 + column];
    const Real berr = after.berr[1 + column];
    ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr) && ferr > epsilon / 32);
    ASC_DENSE_TEST_CHECK(test, ferr < 128 * epsilon);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(berr) && berr >= 0);
    ASC_DENSE_TEST_CHECK(test, std::abs(berr - final_error) < 8 * epsilon);
    long double solution_norm = 0;
    long double forward_error = 0;
    for (asc::extent_t i = 0; i < after.n; ++i) {
      const auto actual = ToWide(after.x[after.Offset(3, i, column)]);
      const auto expected = ToWide(TrueSolution<T>(i, column));
      solution_norm = std::max(solution_norm, Magnitude(actual));
      forward_error = std::max(forward_error, Magnitude(actual - expected));
    }
    ASC_DENSE_TEST_CHECK(test, forward_error / solution_norm < 8 * ferr);
  }
  for (std::size_t i = 0; i < after.x.size(); ++i) {
    bool value = false;
    for (asc::extent_t row = 0; row < after.n; ++row) {
      for (asc::extent_t column = 0; column < after.nrhs; ++column) {
        value |= i == after.Offset(3, row, column);
      }
    }
    if (!value) {
      ASC_DENSE_TEST_EQ(test, after.x[i], before.x[i]);
    }
  }
}

template <typename T>
void InfoCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
              int n, int nrhs, unsigned int layouts,
              asc::DenseBlasTranspose trans, char scalar) {
  Sample<T> original(n, nrhs, layouts);
  Prepare(test, provider, trans, 0, original);
  auto positive = original;
  const bool called = n != 0 && nrhs != 0;
  for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
    if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    auto sample = original;
    asc_lapack_test::SetLuAuxInfoFault(fault);
    const auto plan = Take(
        WithoutAllocation(test, [&] { return sample.Query(provider, trans); }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuAuxInfo().calls, 0U);
    const auto work = sample.Workspace(plan);
    auto report = aux::DirtyReport();
    std::printf(
        "LU_AUX_MODE family=refinement scalar=%c n=%d nrhs=%d layouts=%u "
        "trans=%d fault=%d\n",
        scalar, n, nrhs, layouts, static_cast<int>(trans),
        static_cast<int>(fault));
    const auto status = WithoutAllocation(test, [&] {
      return sample.Execute(provider, trans, plan, work, report);
    });
    aux::CheckInfo(test, provider, status, report, called, fault,
                   aux::Name(scalar, "gerfs").data());
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, sample.a, original.a);
    ASC_DENSE_TEST_EQ(test, sample.af, original.af);
    ASC_DENSE_TEST_EQ(test, sample.b, original.b);
    ASC_DENSE_TEST_EQ(test, sample.pivots, original.pivots);
    sample.Guards(test, plan);
    if (fault == Fault::kNone) {
      if (called) {
        CheckNumericalOutputs(test, original, sample, trans);
      }
      positive = sample;
    } else {
      ASC_DENSE_TEST_EQ(
          test, sample.x,
          called && sample.Layout(3) == kRow ? original.x : positive.x);
    }
    ASC_DENSE_TEST_EQ(test, sample.ferr, positive.ferr);
    ASC_DENSE_TEST_EQ(test, sample.berr, positive.berr);
    for (std::size_t i = static_cast<std::size_t>(nrhs) + 1;
         i < sample.ferr.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, sample.ferr[i], original.ferr[i]);
      ASC_DENSE_TEST_EQ(test, sample.berr[i], original.berr[i]);
    }
  }
}
template <typename T>
void RunInfo(TestContext& test, const asc::ReferenceLapackProvider& provider,
             char scalar) {
  for (int n : {0, 1, 3}) {
    for (int nrhs : {0, 1, 2}) {
      for (unsigned int layouts = 0; layouts < 16; ++layouts) {
        for (auto trans : {kNone, kTranspose, kConjugate}) {
          InfoCase<T>(test, provider, n, nrhs, layouts, trans, scalar);
        }
      }
    }
  }
}
}  // namespace asc_refinement_test
namespace refinement = asc_refinement_test;
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = refinement::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  refinement::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    refinement::RunInfo<float>(test, provider, 's');
  } else if (scalar == "d") {
    refinement::RunInfo<double>(test, provider, 'd');
  } else if (scalar == "c") {
    refinement::RunInfo<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    refinement::RunInfo<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf("LU_AUX_COMPLETED family=refinement scalar=%s abi=%d cases=%zu\n",
              argv[1], ASC_LAPACK_INTEGER_BITS, asc_lu_aux_info_test::Cases());
  return test.Finish();
}
