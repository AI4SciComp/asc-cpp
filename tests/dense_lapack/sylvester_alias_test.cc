#include <array>
#include <complex>
#include <cstddef>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_sylvester.h"
#include "installed_lu/normal_return_guard.h"
#include "sylvester_faults.h"
#include "sylvester_test_support.h"

namespace {
using asc_sylvester_test::ForeignCalls;
using asc_sylvester_test::kPacking;
using asc_sylvester_test::Layout;
using asc_sylvester_test::Operation;
using asc_sylvester_test::SameBits;
using asc_sylvester_test::Take;
using asc_sylvester_test::TestContext;
using asc_sylvester_test::WithoutAllocation;
using Sign = asc::LapackSylvesterSign;

template <typename T>
asc::DenseBlasRealType<T>& ScalarPart(T& value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    // std::complex explicitly permits array-oriented real-component access.
    return reinterpret_cast<asc::DenseBlasRealType<T>*>(&value)[0];
  } else {
    return value;
  }
}

template <typename T>
struct Inputs {
  std::array<T, 20> values{};
  std::array<T, 4> packing{};
  asc::LapackReport report;
  Inputs() {
    values.fill(T{23});
    packing.fill(T{29});
  }
  auto Const(std::size_t index) {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        &values[index], 1, 1, Layout::kColumnMajor, 1,
        {values.data(), sizeof(values), asc::MemorySpace::kHost}));
  }
  auto Mutable(std::size_t index) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        &values[index], 1, 1, Layout::kColumnMajor, 1,
        {values.data(), sizeof(values), asc::MemorySpace::kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return Take(asc::QueryTrsylWorkspace(
        provider, Operation::kNone, Operation::kNone, Sign::kPlus, Const(0),
        Const(4), Mutable(8), report));
  }
  asc::LapackWorkspace Workspace() {
    asc::LapackWorkspace workspace;
    workspace.regions[kPacking] = asc::MutableMemoryView(
        packing.data(), sizeof(packing), asc::MemorySpace::kHost);
    return workspace;
  }
  auto Execute(const asc::ReferenceLapackProvider& provider,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               std::array<std::size_t, 4> offsets = {0, 4, 8, 12}) {
    return asc::Trsyl(provider, Operation::kNone, Operation::kNone, Sign::kPlus,
                      Const(offsets[0]), Const(offsets[1]), Mutable(offsets[2]),
                      ScalarPart(values[offsets[3]]), plan, workspace, report);
  }
  void CheckUnchanged(TestContext& test) const {
    for (const T value : values) {
      ASC_DENSE_TEST_CHECK(test, SameBits(value, T{23}));
    }
    for (const T value : packing) {
      ASC_DENSE_TEST_CHECK(test, SameBits(value, T{29}));
    }
  }
};

template <typename T>
void OperandPairs(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  Inputs<T> f;
  const auto plan = f.Query(provider);
  const auto workspace = f.Workspace();
  const auto before = ForeignCalls();
  for (std::size_t first = 0; first < 4; ++first) {
    for (std::size_t second = first + 1; second < 4; ++second) {
      std::array<std::size_t, 4> offsets{0, 4, 8, 12};
      offsets[second] = offsets[first];
      const auto status = WithoutAllocation(
          test, [&] { return f.Execute(provider, plan, workspace, offsets); });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_CHECK(test,
                           !f.report.called_provider && !f.report.native_info);
      ASC_DENSE_TEST_EQ(test, ForeignCalls(), before);
      f.CheckUnchanged(test);
    }
  }
}

template <typename T>
void OperandWorkspace(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  Inputs<T> f;
  const auto plan = f.Query(provider);
  const auto original = f.Workspace();
  const auto before = ForeignCalls();
  for (std::size_t role = 0; role < original.regions.size(); ++role) {
    for (const std::size_t offset : {0U, 4U, 8U, 12U}) {
      auto workspace = original;
      // Two genuine live T entries meet packing capacity too; rejection must
      // come from aliasing, not a fabricated span or insufficient storage.
      workspace.regions[role] = asc::MutableMemoryView(
          f.values.data() + offset, 2 * sizeof(T), asc::MemorySpace::kHost);
      const auto status = WithoutAllocation(
          test, [&] { return f.Execute(provider, plan, workspace); });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_CHECK(test,
                           !f.report.called_provider && !f.report.native_info);
      ASC_DENSE_TEST_EQ(test, ForeignCalls(), before);
      f.CheckUnchanged(test);
    }
  }
}

template <typename T>
void MetadataWorkspace(TestContext& test,
                       asc::ReferenceLapackProvider provider) {
  Inputs<T> f;
  auto plan = f.Query(provider);
  const auto original = f.Workspace();
  const auto before = ForeignCalls();
  for (std::size_t target = 0; target < 4; ++target) {
    auto workspace = original;
    const std::array storage{
        asc::MutableMemoryView(&provider, sizeof(provider),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(&plan, sizeof(plan), asc::MemorySpace::kHost),
        asc::MutableMemoryView(&workspace, sizeof(workspace),
                               asc::MemorySpace::kHost),
        asc::MutableMemoryView(&f.report, sizeof(f.report),
                               asc::MemorySpace::kHost)};
    // An unused byte role can describe real metadata object storage without
    // inventing scalar object lifetimes. It is still forbidden to alias it.
    workspace.regions[0] = storage[target];
    f.report.native_info = 53;
    f.report.called_provider = true;
    f.report.outcome = asc::LapackOutcome::kAccuracyWarning;
    const auto prior = f.report;
    const auto status = WithoutAllocation(
        test, [&] { return f.Execute(provider, plan, workspace); });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), before);
    ASC_DENSE_TEST_EQ(test, f.report.native_info, prior.native_info);
    ASC_DENSE_TEST_EQ(test, f.report.called_provider, prior.called_provider);
    ASC_DENSE_TEST_EQ(test, f.report.outcome, prior.outcome);
    ASC_DENSE_TEST_EQ(test, f.report.routine, prior.routine);
    ASC_DENSE_TEST_EQ(test, f.report.provider, prior.provider);
    f.CheckUnchanged(test);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  OperandPairs<T>(test, provider);
  OperandWorkspace<T>(test, provider);
  MetadataWorkspace<T>(test, provider);
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
