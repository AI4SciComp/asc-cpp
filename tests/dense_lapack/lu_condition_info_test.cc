#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_aux_info_faults.h"
#include "lu_aux_info_test_support.h"

namespace asc_lu_aux_info_test {
template <typename T>
void ConditionCase(TestContext& test,
                   const asc::ReferenceLapackProvider& provider, int n,
                   int exponent, asc::DenseBlasLayout layout,
                   asc::LapackConditionNorm norm, char scalar) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> matrix(n, n, layout);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      const Real diagonal = std::ldexp(Real{1}, exponent + i);
      if constexpr (asc::DenseBlasComplex<T>) {
        matrix.At(i, j) = i == j ? T{diagonal, diagonal / 2} : T{};
      } else {
        matrix.At(i, j) = i == j ? diagonal : T{};
      }
    }
  }
  const auto before = matrix.data;
  const Real anorm = n == 0 ? Real{0} : std::abs(matrix.At(n - 1, n - 1));
  Real positive = -17;
  for (auto fault : {Fault::kNone, Fault::kWithhold, Fault::kShortZero}) {
    if (fault == Fault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    Real rcond = -17;
    asc_lapack_test::SetLuAuxInfoFault(fault);
    const auto plan = Take(Observe(test, [&] {
      return asc::QueryGeconWorkspace(provider, norm, matrix.ConstView(), anorm,
                                      rcond);
    }));
    ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuAuxInfo().calls, 0U);
    Scratch<T> scratch;
    const auto work = scratch.Workspace(plan);
    auto report = DirtyReport();
    std::printf(
        "LU_AUX_MODE family=condition scalar=%c n=%d exponent=%d layout=%d "
        "norm=%d fault=%d\n",
        scalar, n, exponent, static_cast<int>(layout), static_cast<int>(norm),
        static_cast<int>(fault));
    const auto status = Observe(test, [&] {
      return asc::Gecon(provider, norm, matrix.ConstView(), anorm, rcond, plan,
                        work, report);
    });
    CheckInfo(test, provider, status, report, n != 0, fault,
              Name(scalar, "gecon").data());
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, matrix.data, before);
    scratch.Guards(test);
    const Real expected = n == 0 ? Real{1} : std::ldexp(Real{1}, 1 - n);
    ASC_DENSE_TEST_CHECK(test,
                         std::abs(rcond - expected) <=
                             Real{32} * std::numeric_limits<Real>::epsilon());
    if (fault == Fault::kNone) {
      positive = rcond;
    } else {
      ASC_DENSE_TEST_EQ(test, rcond, positive);
    }
  }
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         char scalar) {
  for (int n : {0, 1, 3}) {
    for (int exponent : {-100, 0, 100}) {
      for (auto layout : {kRow, kColumn}) {
        for (auto norm : {asc::LapackConditionNorm::kOne,
                          asc::LapackConditionNorm::kInfinity}) {
          ConditionCase<T>(test, provider, n, exponent, layout, norm, scalar);
        }
      }
    }
  }
}
}  // namespace asc_lu_aux_info_test
namespace aux_test = asc_lu_aux_info_test;
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const auto provider = aux_test::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  aux_test::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    aux_test::Run<float>(test, provider, 's');
  } else if (scalar == "d") {
    aux_test::Run<double>(test, provider, 'd');
  } else if (scalar == "c") {
    aux_test::Run<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    aux_test::Run<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf("LU_AUX_COMPLETED family=condition scalar=%s abi=%d cases=%zu\n",
              argv[1], ASC_LAPACK_INTEGER_BITS, aux_test::Cases());
  return test.Finish();
}
