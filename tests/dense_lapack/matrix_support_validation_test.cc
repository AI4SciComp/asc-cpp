#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_matrix_copy_counts.h"
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
#include "matrix_support_faults.h"
#include "matrix_support_test_support.h"
namespace {
namespace support = asc_matrix_support_test;
namespace fault = asc_matrix_support_fault;
namespace numerical = asc_matrix_support_numerical;
using support::TestContext;

template <typename T, bool Rows>
struct Sample {
  asc::ReferenceLapackProvider provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  support::Matrix<T> matrix{3, 5, true, 2};
  asc::DenseBlasMatrixView<const T> view = matrix.View();
  asc::LapackWorkspacePlan plan =
      support::Take(support::Query<T, Rows>(provider, view));
  support::Scratch<T> scratch;
  asc::LapackWorkspace workspace = scratch.Workspace(plan);
  std::array<asc::index_t, 3> extent{71, -19, 73};
  asc::LapackReport report;
  void Seed() {
    report.called_provider = true;
    report.native_info = 79;
    report.diagnostic_index = 7;
    report.outcome = asc::LapackOutcome::kSuccess;
    report.output_validity = asc::LapackOutputValidity::kComplete;
  }
};
template <typename T, bool Rows>
void Rejected(TestContext& test, Sample<T, Rows>& s, asc::index_t& result,
              asc::ErrorCode expected, bool preserve_report) {
  s.Seed();
  const auto provider = support::Bytes(s.provider);
  const auto plan = support::Bytes(s.plan);
  const auto workspace = support::Bytes(s.workspace);
  const auto report = support::Bytes(s.report);
  const auto matrix = numerical::Bits(s.matrix.data);
  const auto scratch = numerical::Bits(s.scratch.data);
  const auto extent = s.extent;
  fault::Reset(fault::Mode::kPass);
  const auto status = support::WithoutAllocation(test, [&] {
    return support::Execute<T, Rows>(s.provider, s.view, result, s.plan,
                                     s.workspace, s.report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_EQ(test, support::Bytes(s.provider), provider);
  ASC_DENSE_TEST_EQ(test, support::Bytes(s.plan), plan);
  ASC_DENSE_TEST_EQ(test, support::Bytes(s.workspace), workspace);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(s.matrix.data), matrix);
  ASC_DENSE_TEST_EQ(test, numerical::Bits(s.scratch.data), scratch);
  ASC_DENSE_TEST_EQ(test, s.extent, extent);
  if (preserve_report) {
    ASC_DENSE_TEST_EQ(test, support::Bytes(s.report), report);
  } else {
    ASC_DENSE_TEST_CHECK(test, !s.report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !s.report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !s.report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, s.report.outcome, asc::LapackOutcome::kNotRun);
    ASC_DENSE_TEST_EQ(test, s.report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
  }
}
template <typename T, bool Rows>
void Metadata(TestContext& test) {
  Sample<T, Rows> s;
  const auto original_workspace = s.workspace;
  const auto original_view = s.view;
  const std::array memories{
      asc::MutableMemoryView{&s.provider, sizeof(s.provider), support::kHost},
      asc::MutableMemoryView{&s.plan, sizeof(s.plan), support::kHost},
      asc::MutableMemoryView{&s.workspace, sizeof(s.workspace), support::kHost},
      asc::MutableMemoryView{&s.report, sizeof(s.report), support::kHost}};
  std::size_t count = 0;
  for (const auto memory : memories) {
    auto* bytes = static_cast<std::byte*>(memory.data());
    // Live metadata is addressed only by descriptors/references. The operation
    // must reject their overlap before any numeric load or report reset.
    for (std::size_t offset = 0; offset + sizeof(T) <= memory.size();
         offset += alignof(T)) {
      s.view = support::Take(asc::DenseBlasMatrixView<const T>::Create(
          reinterpret_cast<const T*>(bytes + offset), 1, 1, support::kRow, 1,
          asc::ConstMemoryView(memory)));
      Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, true);
      s.view = original_view;
      ++count;
    }
    for (std::size_t offset = 0; offset + sizeof(asc::index_t) <= memory.size();
         offset += alignof(asc::index_t)) {
      auto& result = *reinterpret_cast<asc::index_t*>(bytes + offset);
      Rejected(test, s, result, asc::ErrorCode::kInvalidArgument, true);
      ++count;
    }
    for (std::size_t role = 0; role < s.workspace.regions.size(); ++role) {
      s.workspace.regions[role] = memory;
      Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, true);
      s.workspace = original_workspace;
      ++count;
    }
  }
  std::printf("matrix support metadata aliases=%zu\n", count);
}
template <typename T, bool Rows>
void StalePlans(TestContext& test) {
  Sample<T, Rows> s;
  const auto original = s.plan;
  for (std::size_t role = 0; role < s.plan.regions.size(); ++role) {
    ++s.plan.regions[role].minimum_entries;
    Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidState, false);
    s.plan = original;
  }
  --s.plan.total_byte_limit;
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidState, false);
  s.plan = original;
  s.plan.identity =
      support::Take(support::Query<T, !Rows>(s.provider, s.view)).identity;
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidState, false);
  for (bool row : {false, true}) {
    support::Matrix<T> other(2, 5, row, 2);
    s.plan = support::Take(support::Query<T, Rows>(s.provider, other.View()));
    Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidState, false);
  }
}
template <typename T, bool Rows>
void StaleIdentity(TestContext& test) {
  Sample<T, Rows> s;
  const auto original = s.plan;
  for (int defect = 0; defect < 6; ++defect) {
    std::array<asc::extent_t, 3> dimensions{3, 5, s.matrix.leading};
    std::array<std::int64_t, 1> options{
        static_cast<std::int64_t>(support::kRow)};
    auto provider = s.provider.identity();
    auto scalar = original.identity.scalar();
    if (defect < 3) {
      ++dimensions[static_cast<std::size_t>(defect)];
    } else if (defect == 3) {
      options[0] = static_cast<std::int64_t>(support::kColumn);
    } else if (defect == 4) {
      ++provider.version[0];
    } else {
      scalar = scalar == asc::LapackScalarKind::kF32
                   ? asc::LapackScalarKind::kF64
                   : asc::LapackScalarKind::kF32;
    }
    s.plan.identity = support::Take(asc::LapackPlanIdentity::Create(
        original.identity.routine(), scalar, dimensions, options, provider));
    Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidState, false);
  }
}
template <typename T, bool Rows>
void Storage(TestContext& test) {
  Sample<T, Rows> s;
  const auto original = s.workspace;
  auto& region = s.workspace.regions[support::kLayout];
  const auto bytes = region.size();
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    region = {s.scratch.data.data() + 1, bytes, space};
    Rejected(test, s, s.extent[1], asc::ErrorCode::kMemoryAccess, false);
    s.workspace = original;
    s.view = support::Take(asc::DenseBlasMatrixView<const T>::Create(
        s.matrix.data.data() + 2, 3, 5, support::kRow, s.matrix.leading,
        {s.matrix.data.data(), sizeof(s.matrix.data), space}));
    ASC_DENSE_TEST_EQ(
        test, (support::Query<T, Rows>(s.provider, s.view)).status().code(),
        asc::ErrorCode::kMemoryAccess);
    Rejected(test, s, s.extent[1], asc::ErrorCode::kMemoryAccess, false);
    s.view = s.matrix.View();
  }
  region = {s.scratch.data.data() + 1, bytes - 1, support::kHost};
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, false);
  region = {reinterpret_cast<std::byte*>(s.scratch.data.data()) + 1, bytes,
            support::kHost};
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, false);
  region = {s.matrix.data.data() + 2, bytes, support::kHost};
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, false);
  s.workspace = original;
  s.workspace.regions[0] = {s.extent.data(), sizeof(s.extent), support::kHost};
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, false);
  s.workspace = original;
  s.workspace.regions[0] = region;
  Rejected(test, s, s.extent[1], asc::ErrorCode::kInvalidArgument, false);
  s.workspace = original;
  // Real index backing, descriptor-only T input; overlap rejects before load.
  s.view = support::Take(asc::DenseBlasMatrixView<const T>::Create(
      reinterpret_cast<const T*>(s.extent.data()), 1, 1, support::kRow, 1,
      {s.extent.data(), sizeof(s.extent), support::kHost}));
  Rejected(test, s, s.extent[0], asc::ErrorCode::kInvalidArgument, false);
}
void Counts(TestContext& test) {
  namespace counts = asc::internal_matrix_copy_counts;
  for (const std::int64_t limit :
       {std::int64_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<std::int64_t>::max()}) {
    ASC_DENSE_TEST_EQ(test, *counts::Count(1, limit - 1, 1, limit), limit - 1);
    ASC_DENSE_TEST_EQ(test, *counts::Count(limit - 1, 1, limit, limit),
                      limit - 1);
    ASC_DENSE_TEST_EQ(test, *counts::Count(limit, 0, 1, limit), 0);
    ASC_DENSE_TEST_EQ(test, counts::Count(limit, 1, 1, limit).status().code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Count(1, limit, 1, limit).status().code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, counts::Count(-1, 1, 1, limit).status().code(),
                      asc::ErrorCode::kInvalidArgument);
  }
  ASC_DENSE_TEST_EQ(
      test,
      counts::Count(4, std::numeric_limits<asc::extent_t>::max() / 4 + 1, 4,
                    std::numeric_limits<asc::extent_t>::max())
          .status()
          .code(),
      asc::ErrorCode::kOverflow);
}
template <typename T, bool Rows>
void Controls(TestContext& test) {
  Sample<T, Rows> s;
  for (int m : {0, 1, 3}) {
    for (int n : {0, 1, 3}) {
      for (bool row : {false, true}) {
        support::Matrix<T> matrix(m, n, row, 2);
        matrix.Fill(m * n + 1, T{-3});
        const auto plan =
            support::Take(support::Query<T, Rows>(s.provider, matrix.View()));
        const auto workspace = s.scratch.Workspace(plan);
        fault::Reset(fault::Mode::kPass);
        ASC_DENSE_TEST_CHECK(test, (support::WithoutAllocation(test, [&] {
                                     return support::Execute<T, Rows>(
                                         s.provider, matrix.View(), s.extent[1],
                                         plan, workspace, s.report);
                                   })).ok());
        ASC_DENSE_TEST_EQ(test, fault::Calls(), m != 0 && n != 0 ? 1U : 0U);
        ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
        ASC_DENSE_TEST_EQ(test, s.extent[1],
                          m == 0 || n == 0 ? 0 : (Rows ? m : n));
      }
    }
  }
}
template <typename T, bool Rows>
void Run(TestContext& test) {
  Counts(test);
  Metadata<T, Rows>(test);
  StalePlans<T, Rows>(test);
  StaleIdentity<T, Rows>(test);
  Storage<T, Rows>(test);
  Controls<T, Rows>(test);
}

template <typename T>
void Dispatch(TestContext& test, std::string_view operation) {
  if (operation == "ilalc") {
    Run<T, false>(test);
  } else {
    Run<T, true>(test);
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view operation(argv[1]);
  const std::string_view scalar(argv[2]);
  if (operation != "ilalc" && operation != "ilalr") {
    return 2;
  }
  TestContext test;
  if (scalar == "s") {
    Dispatch<float>(test, operation);
  } else if (scalar == "d") {
    Dispatch<double>(test, operation);
  } else if (scalar == "c") {
    Dispatch<std::complex<float>>(test, operation);
  } else if (scalar == "z") {
    Dispatch<std::complex<double>>(test, operation);
  } else {
    return 2;
  }
  return test.Finish();
}
