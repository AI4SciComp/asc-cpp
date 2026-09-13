#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "band_cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_band_test;
using support::Take;
using support::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 2;

struct Mode {
  asc::extent_t n;
  asc::extent_t kd;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  bool blocked;
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           support::BandData<T>& band, bool blocked) {
  return Take(blocked ? asc::QueryPbtrfWorkspace(provider, band.View())
                      : asc::QueryPbtf2Workspace(provider, band.View()));
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   support::BandData<T>& band, bool blocked,
                   const asc::LapackWorkspacePlan& plan,
                   support::Storage<T>& scratch, asc::LapackReport& report) {
  return blocked
             ? asc::Pbtrf(provider, band.View(), plan, scratch.View(), report)
             : asc::Pbtf2(provider, band.View(), plan, scratch.View(), report);
}

void CheckSuccess(TestContext& test, const asc::LapackReport& report,
                  const asc::ReferenceLapackProvider& provider,
                  std::string_view routine) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()).substr(1),
                    routine);
  ASC_DENSE_TEST_CHECK(test,
                       !report.native_argument && !report.diagnostic_index);
}

template <typename T>
void CheckFactor(TestContext& test, const support::BandData<T>& band) {
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      if (band.Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(support::ToWide(
                                       band.values[band.Index(i, j)]))));
      }
    }
  }
  band.CheckFactor(test);
}

template <typename T>
void Solve(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const support::BandData<T>& band, asc::DenseBlasLayout layout,
           std::barrier<>& rendezvous) {
  support::RhsData<T> rhs(band, 2, layout);
  const auto before = rhs.values;
  const auto factors = band.values;
  rendezvous.arrive_and_wait();
  const auto plan =
      Take(asc::QueryPbtrsWorkspace(provider, band.ConstView(), rhs.View()));
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(before, rhs.values));
  support::Storage<T> scratch(plan);
  asc::LapackReport report;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    rhs.values = before;
    rendezvous.arrive_and_wait();
    ASC_DENSE_TEST_CHECK(
        test, asc::Pbtrs(provider, band.ConstView(), rhs.View(), plan,
                         scratch.View(), report)
                  .ok());
    CheckSuccess(test, report, provider, "pbtrs");
    rhs.Check(test, band, before);
    scratch.Check(test);
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(factors, band.values));
  }
}

template <typename T>
void FinalOutcome(TestContext& test, int worker,
                  const asc::ReferenceLapackProvider& provider,
                  support::BandData<T>& band, bool blocked,
                  const asc::LapackWorkspacePlan& plan,
                  std::barrier<>& rendezvous) {
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      if (band.Selected(i, j)) {
        band.values[band.Index(i, j)] = i == j ? T{4} : T{};
      }
    }
  }
  if (worker == 1 || worker == 2) {
    band.values[band.Index(worker - 1, worker - 1)] = T{-1};
  }
  const auto before = band.values;
  const auto selected_plan =
      worker == 0 ? Query(provider, band, !blocked) : plan;
  support::Storage<T> scratch(selected_plan);
  asc::LapackReport report;
  rendezvous.arrive_and_wait();
  const auto status =
      Factor(provider, band, blocked, selected_plan, scratch, report);
  if (worker == 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(before, band.values));
  } else if (worker == 1 || worker == 2) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, worker);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, worker - 1);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kNotPositiveDefinite);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    CheckSuccess(test, report, provider, blocked ? "pbtrf" : "pbtf2");
  }
  band.CheckPadding(test, before);
  scratch.Check(test);
}

template <typename T>
void Worker(TestContext& test, int worker, const Mode& mode,
            const asc::ReferenceLapackProvider& provider,
            const asc::LapackWorkspacePlan& shared_plan,
            const support::BandData<T>& shared_factor,
            std::barrier<>& rendezvous) {
  support::BandData<T> band(mode.n, mode.kd, mode.triangle, mode.layout,
                            worker - 2);
  const auto before = band.values;
  support::Storage<T> scratch(shared_plan);
  asc::LapackReport report;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    band.values = before;
    rendezvous.arrive_and_wait();
    const auto local_plan = Query(provider, band, mode.blocked);
    ASC_DENSE_TEST_EQ(test, local_plan.identity, shared_plan.identity);
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(before, band.values));
    rendezvous.arrive_and_wait();
    ASC_DENSE_TEST_CHECK(
        test, Factor(provider, band, mode.blocked, shared_plan, scratch, report)
                  .ok());
    CheckSuccess(test, report, provider, mode.blocked ? "pbtrf" : "pbtf2");
    CheckFactor(test, band);
    band.CheckPadding(test, before);
    scratch.Check(test);
  }
  for (const auto layout : {support::kColumn, support::kRow}) {
    Solve(test, provider, band, layout, rendezvous);
    // All workers read this one actual factor; each RHS, plan, workspace and
    // report belongs to its worker. No shared allocation observer is active.
    Solve(test, provider, shared_factor, layout, rendezvous);
  }
  FinalOutcome(test, worker, provider, band, mode.blocked, shared_plan,
               rendezvous);
}

template <typename T>
void Exercise(TestContext& test, const asc::ReferenceLapackProvider& provider,
              const Mode& mode) {
  support::BandData<T> shared_factor(mode.n, mode.kd, mode.triangle,
                                     mode.layout);
  const auto plan = Query(provider, shared_factor, mode.blocked);
  support::Storage<T> scratch(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, Factor(provider, shared_factor, mode.blocked, plan, scratch, report)
                .ok());
  CheckSuccess(test, report, provider, mode.blocked ? "pbtrf" : "pbtf2");
  CheckFactor(test, shared_factor);
  const auto factors = shared_factor.values;
  std::barrier rendezvous(kWorkers);
  std::array<std::thread, kWorkers> workers;
  std::array<int, kWorkers> results{};
  for (int worker = 0; worker < kWorkers; ++worker) {
    workers[worker] = std::thread([&, worker] {
      TestContext local;
      Worker(local, worker, mode, provider, plan, shared_factor, rendezvous);
      results[worker] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(factors, shared_factor.values));
  scratch.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {support::kUpper, support::kLower}) {
    for (const auto layout : {support::kColumn, support::kRow}) {
      for (const bool blocked : {false, true}) {
        Exercise<T>(test, provider, {3, 2, triangle, layout, blocked});
        Exercise<T>(test, provider, {96, 65, triangle, layout, blocked});
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
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  TestContext test;
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
