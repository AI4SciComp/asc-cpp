#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_refinement_native_test_support.h"
#include "band_refinement_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_estimation_test::WorkspaceStorage;
using asc_band_refinement_test::Direct;
using asc_band_refinement_test::Fixture;

template <typename T>
void Compare(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Fixture<T>& data) {
  const auto before_a = data.a.values;
  const auto before_af = data.af.values;
  const auto before_b = data.b.values;
  const auto direct = Direct(test, data);
  const auto plan = Take(data.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, plan, storage.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, direct.info, 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), direct.info);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  bool quality = false;
  for (asc::extent_t j = 0; j < data.x.count; ++j) {
    const auto at = static_cast<std::size_t>(j + 1);
    ASC_DENSE_TEST_CHECK(test, SameScalarBytes(data.ferr[at], direct.ferr[at]));
    ASC_DENSE_TEST_CHECK(test, SameScalarBytes(data.berr[at], direct.berr[at]));
    quality = quality || !std::isfinite(direct.ferr[at]) ||
              direct.ferr[at] < 0 || !std::isfinite(direct.berr[at]) ||
              direct.berr[at] < 0;
    for (asc::extent_t i = 0; i < data.x.n; ++i) {
      const auto direct_at =
          1 + static_cast<std::size_t>(j) * static_cast<std::size_t>(data.x.n) +
          static_cast<std::size_t>(i);
      ASC_DENSE_TEST_CHECK(test,
                           SameScalarBytes(data.x.values[data.x.Index(i, j)],
                                           direct.solution[direct_at]));
    }
  }
  ASC_DENSE_TEST_EQ(test, status.ok(), !quality);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    quality ? asc::LapackOutcome::kAccuracyWarning
                            : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before_a, data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(before_af, data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(before_b, data.b.values));
  storage.Check(test);
}
template <typename T>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTriangle triangle,
              const std::array<asc::DenseBlasLayout, 4>& layouts) {
  Fixture<T> data(6, 2, 2, triangle, layouts);
  const auto plan = Take(asc::QueryPbtrfWorkspace(provider, data.af.View()));
  Storage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Pbtrf(provider, data.af.View(), plan, storage.View(), report).ok());
  for (asc::extent_t i = 0; i < data.x.n; ++i) {
    for (asc::extent_t j = 0; j < data.x.count; ++j) {
      const auto goal =
          data.x.expected[static_cast<std::size_t>(i * data.x.count + j)] *
          0.99L;
      data.x.values[data.x.Index(i, j)] = Value<T>(goal.real(), goal.imag());
    }
  }
  Compare(test, provider, data);
}
template <typename T>
void Raw(TestContext& test, const asc::ReferenceLapackProvider& provider,
         asc::DenseBlasTriangle triangle,
         const std::array<asc::DenseBlasLayout, 4>& layouts, T raw) {
  Fixture<T> data(1, 3, 1, triangle, layouts);
  data.a.values[data.a.Index(0, 0)] = T{1};
  data.af.values[data.af.Index(0, 0)] = raw;
  data.b.values[data.b.Index(0, 0)] = T{1};
  data.x.values[data.x.Index(0, 0)] = T{1};
  Compare(test, provider, data);
}

template <typename T>
void Tiny(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasTriangle triangle,
          const std::array<asc::DenseBlasLayout, 4>& layouts) {
  using Real = asc::DenseBlasRealType<T>;
  const Real factor = std::ldexp(Real{1}, sizeof(Real) == 4 ? -70 : -520);
  Fixture<T> data(1, 0, 1, triangle, layouts);
  data.a.values[data.a.Index(0, 0)] =
      Value<T>(factor * factor, std::numeric_limits<Real>::quiet_NaN());
  data.af.values[data.af.Index(0, 0)] = T{factor};
  data.b.values[data.b.Index(0, 0)] = T{factor * factor};
  data.x.values[data.x.Index(0, 0)] = T{1};
  Compare(test, provider, data);
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(0, 0)], T{1});
  ASC_DENSE_TEST_CHECK(test, std::isinf(data.ferr[1]));
  const long double safe1 =
      2 * static_cast<long double>(std::numeric_limits<Real>::min());
  const long double expected =
      safe1 / (2 * static_cast<long double>(factor * factor) + safe1);
  ASC_DENSE_TEST_CHECK(test, std::abs(data.berr[1] - expected) <=
                                 std::numeric_limits<Real>::epsilon());
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      Ordinary<T>(test, provider, triangle, layouts);
      Tiny<T>(test, provider, triangle, layouts);
      for (const T raw : {Value<T>(2, 0.5), Value<T>(-2, 0.5), T{0},
                          T{std::numeric_limits<Real>::infinity()},
                          T{std::numeric_limits<Real>::quiet_NaN()}}) {
        Raw(test, provider, triangle, layouts, raw);
      }
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
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
