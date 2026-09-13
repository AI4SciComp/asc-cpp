#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_condition.h"
#include "asc/dense/providers/lapack_cholesky_band_driver.h"
#include "asc/dense/providers/lapack_cholesky_band_equilibration.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_test_support.h"
#include "band_refinement_test_support.h"
#include "band_unwritten_info_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_estimation_test::WorkspaceStorage;
using Layout = asc::DenseBlasLayout;
using Triangle = asc::DenseBlasTriangle;
using Provider = asc::ReferenceLapackProvider;
std::size_t g_profiles = 0;

template <typename T>
std::string Routine(std::string_view suffix) {
  char scalar = 'z';
  if constexpr (std::is_same_v<T, float>) {
    scalar = 's';
  } else if constexpr (std::is_same_v<T, double>) {
    scalar = 'd';
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    scalar = 'c';
  }
  return std::string(1, scalar) + std::string(suffix);
}

void CheckCall(TestContext& test, const Provider& provider,
               const asc::Status& status, const asc::LapackReport& report,
               bool omitted, std::string_view routine) {
  ASC_DENSE_TEST_EQ(test, asc_band_unwritten_test::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, asc_band_unwritten_test::NativeSucceeded());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()), routine);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  if (omitted) {
    constexpr auto kMinimum = std::numeric_limits<lapack_int>::min();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(1), kMinimum);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kProviderArgument);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    if constexpr (sizeof(lapack_int) == sizeof(std::int64_t)) {
      ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
    } else {
      ASC_DENSE_TEST_EQ(
          test, report.native_argument.value_or(0),
          -static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()));
    }
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(1), 0);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  }
  std::printf(
      "{\"routine\":\"%.*s\",\"omitted_info\":%s,"
      "\"actual_native_success\":%s,\"asc_status\":%d}\n",
      static_cast<int>(routine.size()), routine.data(),
      omitted ? "true" : "false",
      asc_band_unwritten_test::NativeSucceeded() ? "true" : "false",
      static_cast<int>(status.code()));
  ++g_profiles;
}

template <typename T>
void FillFactor(BandData<T>& factor) {
  for (asc::extent_t i = 0; i < factor.n; ++i) {
    for (asc::extent_t j = 0; j < factor.n; ++j) {
      if (factor.Selected(i, j)) {
        const auto value =
            factor.expected[static_cast<std::size_t>(i * factor.n + j)];
        factor.values[factor.Index(i, j)] =
            Value<T>(value.real(), value.imag());
      }
    }
  }
}

template <typename T>
void CheckRhsPadding(TestContext& test, const RhsData<T>& rhs,
                     const std::vector<T>& before) {
  std::vector<bool> logical(rhs.values.size());
  for (asc::extent_t i = 0; i < rhs.n; ++i) {
    for (asc::extent_t j = 0; j < rhs.count; ++j) {
      logical[rhs.Index(i, j)] = true;
    }
  }
  for (std::size_t i = 0; i < before.size(); ++i) {
    if (!logical[i]) {
      ASC_DENSE_TEST_CHECK(test, SameScalarBytes(rhs.values[i], before[i]));
    }
  }
}

template <typename T>
void Driver(TestContext& test, const Provider& provider, Triangle triangle,
            Layout layout, Layout rhs_layout) {
  BandData<T> original(3, 1, triangle, layout);
  RhsData<T> original_rhs(original, 2, rhs_layout);
  auto expected = original;
  auto expected_rhs = original_rhs;
  const auto routine = Routine<T>("pbsv");
  for (const bool omitted : {false, true}) {
    auto band = original;
    auto rhs = original_rhs;
    const auto plan =
        Take(asc::QueryPbsvWorkspace(provider, band.View(), rhs.View()));
    WorkspaceStorage<T> storage(plan);
    asc_band_unwritten_test::Reset(routine, omitted);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Pbsv(provider, band.View(), rhs.View(), plan, storage.View(),
                       report);
    });
    CheckCall(test, provider, status, report, omitted, routine);
    if (!omitted) {
      band.CheckFactor(test);
      rhs.Check(test, original, original_rhs.values);
      expected = band;
      expected_rhs = rhs;
    } else {
      ASC_DENSE_TEST_CHECK(
          test, SameBytes(band.values,
                          layout == kRow ? original.values : expected.values));
      ASC_DENSE_TEST_CHECK(
          test,
          SameBytes(rhs.values, rhs_layout == kRow ? original_rhs.values
                                                   : expected_rhs.values));
    }
    band.CheckPadding(test, original.values);
    CheckRhsPadding(test, rhs, original_rhs.values);
    storage.Check(test);
  }
}

template <typename T>
void Equilibration(TestContext& test, const Provider& provider,
                   Triangle triangle, Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> band(3, 1, triangle, layout);
  const auto before = band.values;
  std::array<Real, 5> expected{};
  asc::LapackBandEquilibrationStatistics<Real> expected_statistics;
  const auto routine = Routine<T>("pbequ");
  for (const bool omitted : {false, true}) {
    std::array<Real, 5> scales{Real{-13}, Real{-17}, Real{-19}, Real{-23},
                               Real{-29}};
    auto output = Take(asc::DenseBlasVectorView<Real>::Create(
        scales.data() + 1, 3, 1, {scales.data(), sizeof(scales), kHost}));
    asc::LapackBandEquilibrationStatistics<Real> statistics;
    const auto plan = Take(asc::QueryPbequWorkspace(provider, band.ConstView(),
                                                    output, statistics));
    WorkspaceStorage<T> storage(plan);
    asc_band_unwritten_test::Reset(routine, omitted);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Pbequ(provider, band.ConstView(), output, statistics, plan,
                        storage.View(), report);
    });
    CheckCall(test, provider, status, report, omitted, routine);
    if (!omitted) {
      expected = scales;
      expected_statistics = statistics;
    } else {
      ASC_DENSE_TEST_EQ(test, scales, expected);
      ASC_DENSE_TEST_EQ(test, statistics.scale_condition,
                        expected_statistics.scale_condition);
      ASC_DENSE_TEST_EQ(test, statistics.absolute_maximum,
                        expected_statistics.absolute_maximum);
    }
    ASC_DENSE_TEST_EQ(test, scales.front(), Real{-13});
    ASC_DENSE_TEST_EQ(test, scales.back(), Real{-29});
    ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
    storage.Check(test);
  }
}

template <typename T>
void Condition(TestContext& test, const Provider& provider, Triangle triangle,
               Layout layout) {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> band(3, 1, triangle, layout);
  FillFactor(band);
  const auto before = band.values;
  const Real norm =
      static_cast<Real>(asc_band_estimation_test::OneNorm(band.original, 3));
  Real expected = -1;
  const auto routine = Routine<T>("pbcon");
  for (const bool omitted : {false, true}) {
    Real rcond = -1;
    const auto plan =
        Take(asc::QueryPbconWorkspace(provider, band.ConstView(), norm, rcond));
    WorkspaceStorage<T> storage(plan);
    asc_band_unwritten_test::Reset(routine, omitted);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Pbcon(provider, band.ConstView(), norm, rcond, plan,
                        storage.View(), report);
    });
    CheckCall(test, provider, status, report, omitted, routine);
    if (!omitted) {
      expected = rcond;
      ASC_DENSE_TEST_CHECK(test, rcond > 0 && rcond <= 1);
    } else {
      ASC_DENSE_TEST_EQ(test, rcond, expected);
    }
    ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
    storage.Check(test);
  }
}

template <typename T>
void Refinement(TestContext& test, const Provider& provider, Triangle triangle,
                const std::array<Layout, 4>& layouts) {
  asc_band_refinement_test::Fixture<T> original(3, 1, 2, triangle, layouts);
  FillFactor(original.af);
  auto expected = original;
  const auto routine = Routine<T>("pbrfs");
  for (const bool omitted : {false, true}) {
    auto data = original;
    const auto plan = Take(data.Query(provider));
    WorkspaceStorage<T> storage(plan);
    asc_band_unwritten_test::Reset(routine, omitted);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return data.Execute(provider, plan, storage.View(), report);
    });
    CheckCall(test, provider, status, report, omitted, routine);
    if (!omitted) {
      data.x.Check(test, data.a, original.x.values);
      expected = data;
    } else {
      ASC_DENSE_TEST_CHECK(
          test,
          SameBytes(data.x.values, layouts[3] == kRow ? original.x.values
                                                      : expected.x.values));
      ASC_DENSE_TEST_EQ(test, data.ferr, expected.ferr);
      ASC_DENSE_TEST_EQ(test, data.berr, expected.berr);
    }
    ASC_DENSE_TEST_CHECK(test, SameBytes(data.a.values, original.a.values));
    ASC_DENSE_TEST_CHECK(test, SameBytes(data.af.values, original.af.values));
    ASC_DENSE_TEST_CHECK(test, SameBytes(data.b.values, original.b.values));
    CheckRhsPadding(test, data.x, original.x.values);
    storage.Check(test);
  }
}

template <typename T>
void ExpertPublication(TestContext& test,
                       const asc_band_driver_test::Fixture<T>& actual,
                       const asc_band_driver_test::Fixture<T>& original,
                       const asc_band_driver_test::Fixture<T>& expected) {
  const auto& data = actual.data;
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.a.values, original.data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.b.values, original.data.b.values));
  ASC_DENSE_TEST_CHECK(
      test,
      SameBytes(data.af.values, data.af.layout == kRow || actual.mode == 'F'
                                    ? original.data.af.values
                                    : expected.data.af.values));
  ASC_DENSE_TEST_CHECK(
      test,
      SameBytes(data.x.values, data.x.layout == kRow ? original.data.x.values
                                                     : expected.data.x.values));
  ASC_DENSE_TEST_EQ(test, actual.reciprocal_condition,
                    expected.reciprocal_condition);
  ASC_DENSE_TEST_EQ(test, data.ferr, expected.data.ferr);
  ASC_DENSE_TEST_EQ(test, data.berr, expected.data.berr);
  ASC_DENSE_TEST_EQ(test, actual.scales, expected.scales);
  ASC_DENSE_TEST_EQ(test, actual.equilibration, original.equilibration);
}

template <typename T>
void Expert(TestContext& test, const Provider& provider, Triangle triangle,
            const std::array<Layout, 4>& layouts, char mode) {
  asc_band_driver_test::Fixture<T> original(3, 1, 2, triangle, layouts, mode);
  if (mode == 'F') {
    FillFactor(original.data.af);
  }
  auto expected = original;
  const auto routine = Routine<T>("pbsvx");
  for (const bool omitted : {false, true}) {
    auto fixture = original;
    const auto plan = Take(fixture.Query(provider));
    WorkspaceStorage<T> storage(plan);
    asc_band_unwritten_test::Reset(routine, omitted);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return fixture.Execute(provider, plan, storage.View(), report);
    });
    CheckCall(test, provider, status, report, omitted, routine);
    if (!omitted) {
      fixture.data.x.Check(test, fixture.data.a, original.data.x.values);
      expected = fixture;
    } else {
      ExpertPublication(test, fixture, original, expected);
    }
    fixture.data.a.CheckPadding(test, original.data.a.values);
    fixture.data.af.CheckPadding(test, original.data.af.values);
    CheckRhsPadding(test, fixture.data.b, original.data.b.values);
    CheckRhsPadding(test, fixture.data.x, original.data.x.values);
    storage.Check(test);
  }
}

template <typename T>
void Run(TestContext& test, const Provider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      Equilibration<T>(test, provider, triangle, layout);
      Condition<T>(test, provider, triangle, layout);
      for (const auto rhs_layout : {kColumn, kRow}) {
        Driver<T>(test, provider, triangle, layout, rhs_layout);
      }
    }
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      Refinement<T>(test, provider, triangle, layouts);
      for (const char mode : {'N', 'E', 'F'}) {
        Expert<T>(test, provider, triangle, layouts, mode);
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
  const auto provider = Take(Provider::Create(asc::ExecutionContext::Serial()));
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
  ASC_DENSE_TEST_EQ(test, g_profiles, 288U);
  std::printf("band unwritten INFO profiles=%zu\n", g_profiles);
  return test.Finish();
}
