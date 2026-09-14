#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_copy_entry.h"
#include "matrix_copy_test_support.h"

namespace {
namespace support = asc_copy_test;
using support::Take;
using support::TestContext;
using Part = asc::LapackMatrixPart;

template <typename T>
auto Snapshot(const T& object) {
  std::array<std::byte, sizeof(T)> bytes{};
  const auto representation = std::as_bytes(std::span(&object, 1));
  std::copy(representation.begin(), representation.end(), bytes.begin());
  return bytes;
}

template <typename T>
void Preserved(TestContext& test, const T& object,
               const std::array<std::byte, sizeof(T)>& before) {
  // Compare the same object before/after rejection, including its padding;
  // this is preservation, not semantic equality between distinct objects.
  const auto representation = std::as_bytes(std::span(&object, 1));
  ASC_DENSE_TEST_CHECK(test, std::equal(representation.begin(),
                                        representation.end(), before.begin()));
}

asc::LapackReport PriorReport() {
  asc::LapackReport report;
  report.routine.fill('Q');
  report.called_provider = true;
  report.native_info = 79;
  report.native_argument = 31;
  report.outcome = asc::LapackOutcome::kSuccess;
  report.output_validity = asc::LapackOutputValidity::kComplete;
  return report;
}

template <typename T>
void Alias(TestContext& test, Part part, asc::DenseBlasLayout il,
           asc::DenseBlasLayout ol, std::size_t object, std::size_t target) {
  auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  support::Problem<T> problem(1, 1, il, ol);
  problem.part = part;
  auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  auto report = PriorReport();
  auto input = std::as_const(problem.input).View();
  auto output = problem.output.View();

  // Every metadata object remains a live object of its real type. Only numeric
  // descriptors refer to its bytes; preflight must reject before dereferencing
  // them. No protected pages, signals or foreign invalid accesses are used.
  static_assert(sizeof(provider) >= sizeof(T) && sizeof(plan) >= sizeof(T) &&
                sizeof(workspace) >= sizeof(T) && sizeof(report) >= sizeof(T));
  static_assert(alignof(asc::ReferenceLapackProvider) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspacePlan) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspace) >= alignof(T));
  static_assert(alignof(asc::LapackReport) >= alignof(T));
  const std::array<void*, 4> objects{&provider, &plan, &workspace, &report};
  const asc::MutableMemoryView alias{objects[object], sizeof(T),
                                     support::kHost};
  if (target == 0) {
    input = Take(asc::DenseBlasMatrixView<const T>::Create(
        static_cast<const T*>(alias.data()), 1, 1, il,
        input.leading_dimension(), asc::ConstMemoryView(alias)));
  } else if (target == 1) {
    output = Take(asc::DenseBlasMatrixView<T>::Create(
        static_cast<T*>(alias.data()), 1, 1, ol, output.leading_dimension(),
        alias));
  } else {
    workspace.regions[target == 2 ? support::kScratch : support::kLayout] =
        alias;
  }
  const auto provider_before = Snapshot(provider);
  const auto plan_before = Snapshot(plan);
  const auto workspace_before = Snapshot(workspace);
  const auto report_before = Snapshot(report);
  const auto input_before = Snapshot(problem.input.data);
  const auto output_before = Snapshot(problem.output.data);
  const auto scratch_before = Snapshot(scratch);
  asc_copy_entry::Reset();
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Lacpy(provider, part, input, output, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, asc_copy_entry::Calls(), std::size_t{0});
  Preserved(test, provider, provider_before);
  Preserved(test, plan, plan_before);
  Preserved(test, workspace, workspace_before);
  Preserved(test, report, report_before);
  Preserved(test, problem.input.data, input_before);
  Preserved(test, problem.output.data, output_before);
  Preserved(test, scratch, scratch_before);
}

template <typename T>
void Controls(TestContext& test, Part part, asc::DenseBlasLayout il,
              asc::DenseBlasLayout ol) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  support::Problem<T> problem(1, 1, il, ol);
  problem.part = part;
  problem.input.At(0, 0) = support::Value<T>(3, 2);
  auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  auto report = PriorReport();
  asc_copy_entry::Reset();
  ASC_DENSE_TEST_CHECK(test, support::WithoutAllocation(test, [&] {
                               return problem.Run(provider, plan, workspace,
                                                  report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, asc_copy_entry::Calls(), std::size_t{1});
  ASC_DENSE_TEST_EQ(test, problem.output.At(0, 0), problem.input.At(0, 0));
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());

  // Safe metadata permits the ordinary report reset even when the plan is
  // stale. Numeric arrays and scratch remain unchanged on that rejection.
  ++plan.regions[support::kScratch].minimum_entries;
  report = PriorReport();
  const auto input_before = Snapshot(problem.input.data);
  const auto output_before = Snapshot(problem.output.data);
  const auto scratch_before = Snapshot(scratch);
  asc_copy_entry::Reset();
  const auto status = support::WithoutAllocation(
      test, [&] { return problem.Run(provider, plan, workspace, report); });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, asc_copy_entry::Calls(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  Preserved(test, problem.input.data, input_before);
  Preserved(test, problem.output.data, output_before);
  Preserved(test, scratch, scratch_before);

  support::Problem<T> empty(0, 1, il, ol);
  empty.part = part;
  const auto empty_plan = Take(empty.Query(provider));
  const auto empty_workspace = scratch.Workspace(empty_plan);
  report = PriorReport();
  ASC_DENSE_TEST_CHECK(test, support::WithoutAllocation(test, [&] {
                               return empty.Run(provider, empty_plan,
                                                empty_workspace, report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, asc_copy_entry::Calls(), std::size_t{0});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
}

template <typename T>
void Run(TestContext& test) {
  for (auto part : {Part::kAll, Part::kUpper, Part::kLower}) {
    for (auto il : {support::kRow, support::kColumn}) {
      for (auto ol : {support::kRow, support::kColumn}) {
        for (std::size_t object = 0; object < 4; ++object) {
          for (std::size_t target = 0; target < 4; ++target) {
            Alias<T>(test, part, il, ol, object, target);
          }
        }
        Controls<T>(test, part, il, ol);
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
