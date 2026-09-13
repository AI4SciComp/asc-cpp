#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_native_test_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::CompareBand;
using asc_band_driver_test::CompareRhs;
using asc_band_driver_test::Fixture;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void CheckReport(TestContext& test, const asc::Status& status,
                 const asc::LapackReport& report,
                 const asc_band_driver_test::DirectResult<T>& direct,
                 asc::extent_t n, asc::extent_t nrhs) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), direct.info);
  const bool partial = direct.info > 0 && direct.info <= n;
  bool warning =
      direct.info == n + 1 || !std::isfinite(direct.reciprocal_condition);
  for (asc::extent_t j = 0; j < nrhs && !partial; ++j) {
    const auto at = static_cast<std::size_t>(j + 1);
    warning = warning || !std::isfinite(direct.ferr[at]) ||
              !std::isfinite(direct.berr[at]);
  }
  ASC_DENSE_TEST_EQ(test, status.ok(), !partial && !warning);
  if (partial) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kNotPositiveDefinite);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                      direct.info - 1);
  } else if (warning) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  }
}
template <typename T>
void CheckRhsPadding(TestContext& test, const RhsData<T>& data,
                     const std::vector<T>& before) {
  std::vector<unsigned char> selected(data.values.size());
  for (asc::extent_t j = 0; j < data.count; ++j) {
    for (asc::extent_t i = 0; i < data.n; ++i) {
      selected[data.Index(i, j)] = 1;
    }
  }
  for (std::size_t i = 0; i < selected.size(); ++i) {
    if (selected[i] == 0) {
      ASC_DENSE_TEST_CHECK(test, SameScalarBytes(data.values[i], before[i]));
    }
  }
}
template <typename T>
void Compare(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Fixture<T>& fixture, asc::extent_t expected_info = -1) {
  auto& data = fixture.data;
  const auto before_a = data.a.values;
  const auto before_af = data.af.values;
  const auto before_b = data.b.values;
  const auto before_x = data.x.values;
  const auto direct = asc_band_driver_test::Direct(test, fixture);
  const auto plan =
      Take(WithoutAllocation(test, [&] { return fixture.Query(provider); }));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return fixture.Execute(provider, plan, storage.View(), report);
  });
  if (expected_info >= 0) {
    ASC_DENSE_TEST_EQ(test, direct.info, expected_info);
  }
  CheckReport(test, status, report, direct, data.a.n, data.b.count);
  if (fixture.mode == 'E' && direct.equilibration == 'Y') {
    CompareBand(test, data.a, direct.a);
  } else {
    ASC_DENSE_TEST_CHECK(test, SameBytes(data.a.values, before_a));
  }
  if (fixture.mode == 'F') {
    ASC_DENSE_TEST_CHECK(test, SameBytes(data.af.values, before_af));
  } else {
    CompareBand(test, data.af, direct.af);
  }
  CompareRhs(test, data.b, direct.b);
  CompareRhs(test, data.x, direct.x);
  ASC_DENSE_TEST_CHECK(test, SameBytes(fixture.scales, direct.scales));
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.ferr, direct.ferr));
  ASC_DENSE_TEST_CHECK(test, SameBytes(data.berr, direct.berr));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(fixture.reciprocal_condition,
                                             direct.reciprocal_condition));
  if (fixture.mode == 'E') {
    ASC_DENSE_TEST_EQ(test, fixture.equilibration,
                      direct.equilibration == 'Y'
                          ? asc::LapackCholeskyEquilibration::kDiagonal
                          : asc::LapackCholeskyEquilibration::kNone);
  }
  data.a.CheckPadding(test, before_a);
  data.af.CheckPadding(test, before_af);
  CheckRhsPadding(test, data.b, before_b);
  CheckRhsPadding(test, data.x, before_x);
  storage.Check(test);
}
template <typename T>
void Ordinary(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTriangle triangle,
              const std::array<asc::DenseBlasLayout, 4>& layouts, char mode,
              asc::extent_t n, asc::extent_t nrhs) {
  Fixture<T> fixture(n, 2, nrhs, triangle, layouts, mode);
  auto& data = fixture.data;
  if (mode == 'F') {
    const auto plan = Take(asc::QueryPbtrfWorkspace(provider, data.af.View()));
    Storage<T> storage(plan);
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, asc::Pbtrf(provider, data.af.View(), plan, storage.View(), report)
                  .ok());
    fixture.scale_count = 0;
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    for (asc::extent_t i = 0; i < n; ++i) {
      data.a.values[data.a.Index(i, i)].imag(
          std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
    }
  }
  Compare(test, provider, fixture, 0);
}
template <typename T>
void Tiny(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasTriangle triangle,
          const std::array<asc::DenseBlasLayout, 4>& layouts, char mode) {
  using Real = asc::DenseBlasRealType<T>;
  const Real t = std::ldexp(Real{1}, sizeof(Real) == 4 ? -70 : -520);
  Fixture<T> fixture(1, 0, 1, triangle, layouts, mode);
  auto& data = fixture.data;
  data.a.values[data.a.Index(0, 0)] =
      Value<T>(t * t, std::numeric_limits<Real>::quiet_NaN());
  data.af.values[data.af.Index(0, 0)] = T{t};
  data.b.values[data.b.Index(0, 0)] = T{t * t};
  data.x.values[data.x.Index(0, 0)] = T{-1};
  fixture.scale_count = mode == 'F' ? 0 : 1;
  Compare(test, provider, fixture, 2);
}
template <typename T>
void Partial(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTriangle triangle,
             const std::array<asc::DenseBlasLayout, 4>& layouts, char mode,
             asc::extent_t kd, asc::extent_t failure) {
  using Real = asc::DenseBlasRealType<T>;
  // More than KD future columns remain: old in-place prefix publication
  // cannot accidentally satisfy the stronger PBSVX COPY output contract.
  const auto n = failure + kd + 3;
  Fixture<T> fixture(n, kd, 2, triangle, layouts, mode);
  auto& data = fixture.data;
  const auto nan = std::numeric_limits<Real>::quiet_NaN();
  for (asc::extent_t j = 0; j < n; ++j) {
    for (asc::extent_t i = 0; i < n; ++i) {
      if (data.a.Selected(i, j)) {
        const Real diagonal = i + 1 == failure ? Real{-1} : Real{4};
        data.a.values[data.a.Index(i, j)] =
            i == j ? Value<T>(diagonal, nan) : T{};
        data.af.values[data.af.Index(i, j)] = Value<T>(nan, nan);
      }
    }
  }
  Compare(test, provider, fixture, failure);
  if constexpr (asc::DenseBlasComplex<T>) {
    for (asc::extent_t i = 0; i < n; ++i) {
      ASC_DENSE_TEST_EQ(test, data.af.values[data.af.Index(i, i)].imag(),
                        Real{0});
    }
  }
}
template <typename T>
void ScaledPartial(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle,
                   const std::array<asc::DenseBlasLayout, 4>& layouts) {
  using Real = asc::DenseBlasRealType<T>;
  const Real t = std::ldexp(Real{1}, sizeof(Real) == 4 ? -60 : -510);
  Fixture<T> fixture(4, 1, 2, triangle, layouts, 'E');
  auto& data = fixture.data;
  for (asc::extent_t j = 0; j < 4; ++j) {
    for (asc::extent_t i = 0; i < 4; ++i) {
      if (data.a.Selected(i, j)) {
        data.a.values[data.a.Index(i, j)] =
            i == j ? Value<T>(t * t, std::numeric_limits<Real>::quiet_NaN())
                   : T{};
      }
    }
  }
  const int i = triangle == kLower ? 1 : 0;
  const int j = triangle == kLower ? 0 : 1;
  data.a.values[data.a.Index(i, j)] = T{2 * t * t};
  // Positive diagonals permit PBEQU and finite LAQ scaling, but the leading
  // 2x2 matrix [[1,2],[2,1]] has second Schur pivot -3.
  Compare(test, provider, fixture, 2);
  ASC_DENSE_TEST_EQ(test, fixture.equilibration,
                    asc::LapackCholeskyEquilibration::kDiagonal);
  ASC_DENSE_TEST_EQ(test, data.a.values[data.a.Index(3, 3)], T{1});
}
template <typename T>
void RawFactor(TestContext& test, const asc::ReferenceLapackProvider& provider,
               asc::DenseBlasTriangle triangle,
               const std::array<asc::DenseBlasLayout, 4>& layouts, T raw) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> fixture(1, 3, 1, triangle, layouts, 'F');
  auto& data = fixture.data;
  data.a.values[data.a.Index(0, 0)] =
      Value<T>(1, std::numeric_limits<Real>::quiet_NaN());
  data.af.values[data.af.Index(0, 0)] = raw;
  data.b.values[data.b.Index(0, 0)] = T{1};
  fixture.scales[1] = std::numeric_limits<Real>::quiet_NaN();
  // EQUED=N makes the supplied n-entry S unused. No diagonal normalization
  // or factor provenance certificate may be silently imposed on raw AF.
  Compare(test, provider, fixture);
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      for (const char mode : {'N', 'E', 'F'}) {
        Ordinary<T>(test, provider, triangle, layouts, mode, 0, 2);
        Ordinary<T>(test, provider, triangle, layouts, mode, 6, 0);
        Ordinary<T>(test, provider, triangle, layouts, mode, 6, 2);
        Tiny<T>(test, provider, triangle, layouts, mode);
      }
      ScaledPartial<T>(test, provider, triangle, layouts);
      for (const T raw :
           {T{0}, T{-1}, Value<T>(1, 2),
            Value<T>(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN())}) {
        RawFactor<T>(test, provider, triangle, layouts, raw);
      }
      for (const char mode : {'N', 'E'}) {
        for (const int kd : {0, 2, 65}) {
          for (const int failure : {1, 32, 33, 64, 65}) {
            Partial<T>(test, provider, triangle, layouts, mode, kd, failure);
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
