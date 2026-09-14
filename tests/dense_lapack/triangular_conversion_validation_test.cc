#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>

#include "../../src/dense/lapack/internal_layout.h"
#include "../../src/dense/lapack/internal_precision_conversion_counts.h"
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
#include "installed_lu/normal_return_guard.h"
#include "triangular_conversion_faults.h"
#include "triangular_conversion_test_support.h"
namespace {
namespace tc = asc_triangular_conversion_test;
namespace fault = asc_triangular_conversion_fault;
using asc::ErrorCode;
using tc::TestContext;
template <typename Input, typename Change>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTriangle triangle, asc::DenseBlasLayout il,
              asc::DenseBlasLayout ol, Change change) {
  tc::Problem<Input> p(5, triangle, il, ol);
  tc::Initialize(p);
  auto plan = tc::Take(p.Query(provider));
  tc::Scratch<Input> scratch;
  auto workspace = scratch.Workspace(plan);
  change(p, plan, workspace);
  const auto before = p;
  const auto scratch_before = scratch;
  asc::LapackReport report;
  fault::Reset(fault::Mode::kPass);
  const auto status = tc::WithoutAllocation(
      test, [&] { return p.Run(provider, plan, workspace, report); });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, p.input.data, before.input.data);
  ASC_DENSE_TEST_EQ(test, p.output.data, before.output.data);
  ASC_DENSE_TEST_EQ(test, scratch.packed, scratch_before.packed);
  ASC_DENSE_TEST_EQ(test, scratch.staged, scratch_before.staged);
}
template <typename Input>
void Metadata(TestContext& test, const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasTriangle triangle, asc::DenseBlasLayout il,
              asc::DenseBlasLayout ol) {
  for (int kind = 0; kind < 13; ++kind) {
    Rejected<Input>(
        test, provider, triangle, il, ol, [&](auto& p, auto& plan, auto&) {
          switch (kind) {
            default:
              std::abort();
            case 0:
              p.triangle = static_cast<asc::DenseBlasTriangle>(99);
              break;
            case 1:
              p.triangle = triangle == tc::kUpper ? tc::kLower : tc::kUpper;
              break;
            case 2:
              p.input.columns = 4;
              break;
            case 3:
              p.output.columns = 4;
              break;
            case 4:
              p.input.ld += 1;
              break;
            case 5:
              p.output.ld += 1;
              break;
            case 6:
              p.input.layout = il == tc::kRow ? tc::kColumn : tc::kRow;
              break;
            case 7:
              p.output.layout = ol == tc::kRow ? tc::kColumn : tc::kRow;
              break;
            case 8:
              --plan.total_byte_limit;
              break;
            case 9:
              ++plan.regions[tc::kScratch].minimum_entries;
              break;
            case 10:
              ++plan.regions[tc::kScratch].entry_bytes;
              break;
            case 11:
              ++plan.regions[tc::kScratch].alignment;
              break;
            case 12:
              plan = tc::Take(asc_conversion_test::Query<Input>(
                  provider, std::as_const(p.input).View(), p.output.View()));
              break;
          }
        });
  }
}
template <typename Input>
void Workspaces(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasTriangle triangle, asc::DenseBlasLayout il,
                asc::DenseBlasLayout ol) {
  for (auto role : {tc::kScratch, tc::kLayout}) {
    if (role == tc::kLayout && il == tc::kColumn) {
      continue;
    }
    for (int kind = 0; kind < 7; ++kind) {
      Rejected<Input>(
          test, provider, triangle, il, ol,
          [&](auto& p, auto&, auto& workspace) {
            auto& region = workspace.regions[role];
            switch (kind) {
              default:
                std::abort();
              case 0:
                region = {nullptr, 0, tc::kHost};
                break;
              case 1:
                region = {region.data(), region.size() - 1, tc::kHost};
                break;
              case 2:
                region = {static_cast<std::byte*>(region.data()) + 1,
                          region.size(), tc::kHost};
                break;
              case 3:
                region = {region.data(), region.size(),
                          asc::MemorySpace::kDevice};
                break;
              case 4:
                region = {p.input.data.data(), region.size(), tc::kHost};
                break;
              case 5:
                region = {p.output.data.data(), region.size(), tc::kHost};
                break;
              case 6:
                workspace.regions[static_cast<std::size_t>(
                    asc::LapackWorkspaceKind::kReal)] = region;
                break;
            }
          });
    }
  }
}
template <typename Input>
void OperandSpans(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  tc::Problem<Input> p(5, tc::kLower, tc::kRow, tc::kColumn);
  tc::Initialize(p);
  const auto input = std::as_const(p.input).View();
  auto output = p.output.View();
  const auto overlapping =
      tc::Take(asc::DenseBlasMatrixView<tc::Output<Input>>::Create(
          reinterpret_cast<tc::Output<Input>*>(p.input.data.data()), 5, 5,
          tc::kColumn, 8,
          {p.input.data.data(), sizeof(p.input.data), tc::kHost}));
  ASC_DENSE_TEST_CHECK(
      test, !tc::Query<Input>(provider, tc::kLower, input, overlapping).ok());
  const auto device = tc::Take(asc::DenseBlasMatrixView<const Input>::Create(
      input.data(), 5, 5, tc::kRow, 8,
      {p.input.data.data(), sizeof(p.input.data), asc::MemorySpace::kDevice}));
  ASC_DENSE_TEST_CHECK(
      test, !tc::Query<Input>(provider, tc::kLower, device, output).ok());
  auto plan = tc::Take(p.Query(provider));
  tc::Scratch<Input> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 79;
  workspace.regions[tc::kScratch] = {&report, sizeof(report), tc::kHost};
  const auto status = p.Run(provider, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, status.code(), ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 79);
}
void Counts(TestContext& test) {
  using asc::internal_precision_conversion_counts::Count;
  constexpr auto kMaximum = std::numeric_limits<asc::extent_t>::max();
  for (asc::extent_t limit : {asc::extent_t{2147483647}, kMaximum}) {
    ASC_DENSE_TEST_CHECK(test, Count(0, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Count(1, 1, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !Count(limit, limit, limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !Count(1, 1, 0, limit).ok());
  }
  ASC_DENSE_TEST_EQ(test, *Count(13, 13, 16, 2147483647), 169);
  ASC_DENSE_TEST_CHECK(
      test, Count(3037000499, 3037000499, 3037000499, kMaximum).ok());
  ASC_DENSE_TEST_CHECK(
      test, !Count(3037000500, 3037000500, 3037000500, kMaximum).ok());
  const auto key = tc::Take(asc::LapackPlanIdentity::Create(
      "test", asc::LapackScalarKind::kMixedF64F32,
      std::array<asc::extent_t, 0>{}, std::array<std::int64_t, 0>{}, {}));
  asc::LapackWorkspacePlan plan{key};
  plan.regions[tc::kScratch] = {kMaximum, kMaximum, sizeof(float),
                                alignof(float)};
  ASC_DENSE_TEST_CHECK(test,
                       !asc::internal_lapack_layout::CheckTotal(plan).ok());
}
template <typename Input>
void Run(TestContext& test) {
  const auto provider = tc::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto triangle : {tc::kUpper, tc::kLower}) {
    for (auto il : {tc::kColumn, tc::kRow}) {
      for (auto ol : {tc::kColumn, tc::kRow}) {
        Metadata<Input>(test, provider, triangle, il, ol);
        Workspaces<Input>(test, provider, triangle, il, ol);
      }
    }
  }
  OperandSpans<Input>(test, provider);
  Counts(test);
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
