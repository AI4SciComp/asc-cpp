#include <array>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::Fixture;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void Formula(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTriangle triangle,
             const std::array<asc::DenseBlasLayout, 4>& layouts, char mode,
             asc::extent_t n, asc::extent_t nrhs) {
  Fixture<T> fixture(n, 9, nrhs, triangle, layouts, mode);
  const auto plan =
      Take(WithoutAllocation(test, [&] { return fixture.Query(provider); }));
  asc::extent_t packing = 0;
  for (std::size_t operand = 0; operand < 4; ++operand) {
    const bool forced = operand == 0 && asc::DenseBlasComplex<T> && mode != 'F';
    if (forced || layouts[operand] == kRow) {
      packing += operand < 2 ? n * 10 : n * nrhs;
    }
  }
  constexpr auto kScalar =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  constexpr auto kInteger =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, packing);
  ASC_DENSE_TEST_EQ(test, plan.regions[kScalar].minimum_entries,
                    n * (asc::DenseBlasComplex<T> ? 2 : 3));
  ASC_DENSE_TEST_EQ(test, plan.regions[kReal].minimum_entries,
                    asc::DenseBlasComplex<T> ? n : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].minimum_entries,
                    asc::DenseBlasComplex<T> ? 0 : n);
  for (const auto& region : plan.regions) {
    ASC_DENSE_TEST_EQ(test, region.minimum_entries, region.preferred_entries);
  }
}
template <typename T>
void EmptyWideStride(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     asc::DenseBlasTriangle triangle, char mode) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> fixture(0, 2, 2, triangle, {kRow, kRow, kRow, kRow}, mode);
  auto& data = fixture.data;
  const asc::extent_t wide = sizeof(lapack_int) == 4
                                 ? asc::extent_t{2147483648}
                                 : std::numeric_limits<asc::extent_t>::max();
  data.a.ld = wide;
  data.af.ld = wide;
  data.b.ld = wide;
  data.x.ld = wide;
  const auto before_a = data.a.values;
  const auto before_af = data.af.values;
  const auto before_b = data.b.values;
  const auto before_x = data.x.values;
  const auto plan =
      Take(WithoutAllocation(test, [&] { return fixture.Query(provider); }));
  for (const auto& region : plan.regions) {
    ASC_DENSE_TEST_EQ(test, region.minimum_entries, 0);
  }
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return fixture.Execute(provider, plan,
                                                      storage.View(), report);
                             }).ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, fixture.reciprocal_condition, Real{1});
  for (std::size_t i = 1; i <= 2; ++i) {
    ASC_DENSE_TEST_EQ(test, data.ferr[i], Real{0});
    ASC_DENSE_TEST_EQ(test, data.berr[i], Real{0});
  }
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.a.values, before_a));
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.af.values, before_af));
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.b.values, before_b));
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.x.values, before_x));
  storage.Check(test);
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const char mode : {'N', 'E', 'F'}) {
      EmptyWideStride<T>(test, provider, triangle, mode);
      for (int bits = 0; bits < 16; ++bits) {
        const std::array layouts{
            bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
            bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
        for (const int n : {0, 4}) {
          for (const int nrhs : {0, 2}) {
            Formula<T>(test, provider, triangle, layouts, mode, n, nrhs);
          }
        }
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
