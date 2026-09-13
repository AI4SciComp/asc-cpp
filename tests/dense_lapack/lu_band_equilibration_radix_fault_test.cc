#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band_equilibration_radix.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_equilibration_radix_faults.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
namespace faults = asc_gb_equb_faults;
using faults::Fault;
using support::Take;
using support::TestContext;
template <typename T>
struct Case {
  using Real = asc::DenseBlasRealType<T>;
  support::Compact<T> matrix;
  support::Vector<Real> rows;
  support::Vector<Real> columns;
  Real stats_prefix = Real{-461};
  asc::LapackEquilibrationStatistics<Real> stats{Real{-401}, Real{-409},
                                                 Real{-419}};
  Real stats_suffix = Real{-463};
  explicit Case(bool empty = false)
      : matrix(asc_lu_band_test::Band<T>(empty ? 0 : 3, 3, 1, 2)),
        rows(empty ? 0 : 3),
        columns(3) {}
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return asc::QueryGbequbWorkspace(provider, matrix.ConstView(), rows.View(),
                                     columns.View(), stats);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    return asc::Gbequb(provider, matrix.ConstView(), rows.View(),
                       columns.View(), stats, plan, workspace, report);
  }
};
template <typename T>
void Unchanged(TestContext& test, const Case<T>& after, const Case<T>& before) {
  ASC_DENSE_TEST_CHECK(
      test, support::SameBytes(after.matrix.values, before.matrix.values));
  ASC_DENSE_TEST_EQ(test, after.rows.values, before.rows.values);
  ASC_DENSE_TEST_EQ(test, after.columns.values, before.columns.values);
  ASC_DENSE_TEST_EQ(test, after.stats_prefix, before.stats_prefix);
  ASC_DENSE_TEST_EQ(test, after.stats_suffix, before.stats_suffix);
  ASC_DENSE_TEST_EQ(test, after.stats.row_condition,
                    before.stats.row_condition);
  ASC_DENSE_TEST_EQ(test, after.stats.column_condition,
                    before.stats.column_condition);
  ASC_DENSE_TEST_EQ(test, after.stats.absolute_maximum,
                    before.stats.absolute_maximum);
}
template <typename T>
void NativeFailure(TestContext& test,
                   const asc::ReferenceLapackProvider& provider, Fault fault,
                   bool empty) {
  Case<T> sample(empty);
  const auto before = sample;
  const auto plan = Take(sample.Query(provider));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  faults::Select(fault);
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, plan, workspace, report); });
  const auto minimum =
      provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
          ? std::numeric_limits<std::int32_t>::min()
          : std::numeric_limits<std::int64_t>::min();
  auto info = minimum;
  if (fault == Fault::kNegative) {
    info = -4;
  }
  if (fault == Fault::kLargePositive) {
    info = sample.matrix.m + sample.matrix.n + 1;
  }
  if (fault == Fault::kOne) {
    info = 1;
  }
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  ASC_DENSE_TEST_CHECK(test, faults::SawInfoSentinel());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, info);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                    fault == Fault::kNegative);
  if (fault == Fault::kNegative) {
    ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
  }
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  Unchanged(test, sample, before);
  scratch.Check(test);
}
template <typename T>
void AlterIdentity(const asc::ReferenceLapackProvider& provider,
                   const Case<T>& sample, int selected,
                   asc::LapackWorkspacePlan& plan) {
  std::array<asc::extent_t, 9> dimensions{3, 3, 1, 2, sample.matrix.ld,
                                          3, 1, 3, 1};
  auto identity = provider.identity();
  auto routine = plan.identity.routine();
  if (selected == 8) {
    routine = "sgbequ";
  }
  if (selected == 9) {
    identity.build_sha256[0] ^= std::byte{1};
  }
  if (selected == 10) {
    ++dimensions[4];
  }
  plan.identity = Take(asc::LapackPlanIdentity::Create(
      routine, plan.identity.scalar(), dimensions,
      std::array<std::int64_t, 0>{}, identity));
}
template <typename T>
void Structural(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Case<T> sample;
  const auto before = sample;
  const auto plan = Take(sample.Query(provider));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  // Every scalar descriptor below retains its complete live Real backing.
  for (int selected = 0; selected < 11; ++selected) {
    auto current = plan;
    auto work = workspace;
    auto rows = sample.rows.View();
    auto columns = sample.columns.View();
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 777;
    report.diagnostic_index = 2;
    auto expected = asc::ErrorCode::kInvalidArgument;
    bool metadata_alias = false;
    if (selected == 0) {
      --current.total_byte_limit;
      expected = asc::ErrorCode::kInvalidState;
    } else if (selected == 1) {
      // This is a workspace-byte/plan alias; R/C remain live Real arrays.
      work.regions[1] = {&current, sizeof(current), asc::MemorySpace::kHost};
      metadata_alias = true;
    } else if (selected == 2) {
      columns = rows;
    } else if (selected == 3) {
      rows = Take(asc::DenseBlasVectorView<Real>::Create(
          sample.rows.values.data(), 3, 2,
          {sample.rows.values.data(), sample.rows.values.size() * sizeof(Real),
           asc::MemorySpace::kHost}));
      expected = asc::ErrorCode::kShape;
    } else if (selected == 4) {
      rows = Take(asc::DenseBlasVectorView<Real>::Create(
          sample.rows.values.data() + 1, 2, 1,
          {sample.rows.values.data(), sample.rows.values.size() * sizeof(Real),
           asc::MemorySpace::kHost}));
      expected = asc::ErrorCode::kShape;
    } else if (selected == 5) {
      rows = Take(asc::DenseBlasVectorView<Real>::Create(
          sample.rows.values.data() + 1, 3, 1,
          {sample.rows.values.data(), sample.rows.values.size() * sizeof(Real),
           asc::MemorySpace::kDevice}));
      expected = asc::ErrorCode::kMemoryAccess;
    } else if (selected == 6) {
      work.regions[0] = {sample.rows.values.data() + 1, 3 * sizeof(Real),
                         asc::MemorySpace::kHost};
    } else if (selected == 7) {
      work.regions[0] = {&current, sizeof(current), asc::MemorySpace::kHost};
      metadata_alias = true;
    } else {
      AlterIdentity(provider, sample, selected, current);
      expected = asc::ErrorCode::kInvalidState;
    }
    faults::Select(Fault::kNegative);
    const auto status = support::WithoutAllocation(test, [&] {
      return asc::Gbequb(provider, sample.matrix.ConstView(), rows, columns,
                         sample.stats, current, work, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    if (metadata_alias) {
      ASC_DENSE_TEST_CHECK(test, report.called_provider &&
                                     report.native_info == 777 &&
                                     report.diagnostic_index == 2);
    } else {
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
    }
    Unchanged(test, sample, before);
    scratch.Check(test);
  }
}
template <typename T>
void Cold(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  Case<T> sample;
  const auto before = sample;
  faults::Select(Fault::kPass);
  const auto plan = Take(
      support::WithoutAllocation(test, [&] { return sample.Query(provider); }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
  Unchanged(test, sample, before);
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, plan, workspace, report); });
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  ASC_DENSE_TEST_CHECK(test, faults::SawInfoSentinel());
  ASC_DENSE_TEST_CHECK(
      test, support::SameBytes(sample.matrix.values, before.matrix.values));
  ASC_DENSE_TEST_EQ(test, sample.stats_prefix, before.stats_prefix);
  ASC_DENSE_TEST_EQ(test, sample.stats_suffix, before.stats_suffix);
  sample.rows.Check(test);
  sample.columns.Check(test);
  scratch.Check(test);
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "cold" && mode != "fault") {
    return 2;
  }
  const auto run = [&]<typename T>() {
    if (mode == "cold") {
      Cold<T>(test, provider);
      return;
    }
    for (const auto fault :
         {Fault::kNegative, Fault::kMinimum, Fault::kLargePositive,
          Fault::kNoInfo, Fault::kPartialInfo}) {
      NativeFailure<T>(test, provider, fault, false);
      NativeFailure<T>(test, provider, fault, true);
    }
    NativeFailure<T>(test, provider, Fault::kOne, true);
    Structural<T>(test, provider);
  };
  if (scalar == "s") {
    run.template operator()<float>();
  } else if (scalar == "d") {
    run.template operator()<double>();
  } else if (scalar == "c") {
    run.template operator()<std::complex<float>>();
  } else if (scalar == "z") {
    run.template operator()<std::complex<double>>();
  } else {
    return 2;
  }
  std::printf(
      "GBEQUB %.*s %.*s: exact matching-plan INFO and preflight/noncall "
      "contracts\n",
      static_cast<int>(scalar.size()), scalar.data(),
      static_cast<int>(mode.size()), mode.data());
  return test.Finish();
}
