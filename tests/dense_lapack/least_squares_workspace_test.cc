#include <array>
#include <complex>
#include <cstddef>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "least_squares_faults.h"
#include "least_squares_test_support.h"

namespace {
using asc_least_squares_test::Execute;
using asc_least_squares_test::ForeignCalls;
using asc_least_squares_test::kPacking;
using asc_least_squares_test::kScalar;
using asc_least_squares_test::Layout;
using asc_least_squares_test::Matrix;
using asc_least_squares_test::Query;
using asc_least_squares_test::Routine;
using asc_least_squares_test::Scratch;
using asc_least_squares_test::Take;
using asc_least_squares_test::TestContext;
using asc_least_squares_test::WithoutAllocation;

template <typename T>
void Storage(TestContext& test, const asc::ReferenceLapackProvider& provider,
             Routine routine) {
  Matrix<T> a(2, 2, Layout::kRowMajor);
  Matrix<T> b(2, 2, Layout::kColumnMajor);
  const auto before_a = a.bytes();
  const auto before_b = b.bytes();
  asc::LapackReport report;
  const auto plan =
      Take(Query(provider, routine, asc::DenseBlasTranspose::kNone, a.view(),
                 b.view(), report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  alignas(T) std::array<std::byte, 4096> backing{};
  const auto calls = ForeignCalls();
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
      auto changed = workspace;
      changed.regions[role] = {backing.data(), backing.size(), space};
      const auto status = WithoutAllocation(test, [&] {
        return Execute(provider, routine, asc::DenseBlasTranspose::kNone,
                       a.view(), b.view(), plan, changed, report);
      });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
      ASC_DENSE_TEST_CHECK(test, !report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    }
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, asc::DenseBlasTranspose::kNone,
                     a.view(), b.view(space), plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
  }
  for (const auto role : {kScalar, kPacking}) {
    auto changed = workspace;
    changed.regions[role] = {
        backing.data() + 1,
        static_cast<std::size_t>(plan.regions[role].minimum_entries) *
            sizeof(T),
        asc::MemorySpace::kHost};
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, asc::DenseBlasTranspose::kNone,
                     a.view(), b.view(), plan, changed, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  }
  ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
  a.CheckSame(test, before_a);
  b.CheckSame(test, before_b);
}

template <typename T>
void OriginalStride(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    Routine routine) {
  T a{2};
  T b{6};
  const auto av = Take(asc::DenseBlasMatrixView<T>::Create(
      &a, 1, 1, Layout::kRowMajor, 1,
      {&a, sizeof(T), asc::MemorySpace::kHost}));
  const auto bv = Take(asc::DenseBlasMatrixView<T>::Create(
      &b, 1, 1, Layout::kColumnMajor, 1,
      {&b, sizeof(T), asc::MemorySpace::kHost}));
  asc::LapackReport report;
  const auto plan = Take(
      Query(provider, routine, asc::DenseBlasTranspose::kNone, av, bv, report));
  Scratch<T> scratch(plan, true);
  const auto workspace = scratch.view();
  constexpr auto kHuge = std::numeric_limits<asc::extent_t>::max();
  const auto changed_a = Take(asc::DenseBlasMatrixView<T>::Create(
      &a, 1, 1, Layout::kRowMajor, kHuge,
      {&a, sizeof(T), asc::MemorySpace::kHost}));
  const auto changed_b = Take(asc::DenseBlasMatrixView<T>::Create(
      &b, 1, 1, Layout::kColumnMajor, kHuge,
      {&b, sizeof(T), asc::MemorySpace::kHost}));
  const auto calls = ForeignCalls();
  for (const bool change_a : {false, true}) {
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, asc::DenseBlasTranspose::kNone,
                     change_a ? changed_a : av, change_a ? bv : changed_b, plan,
                     workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, ForeignCalls(), calls);
    ASC_DENSE_TEST_EQ(test, a, T{2});
    ASC_DENSE_TEST_EQ(test, b, T{6});
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto routine :
       {Routine::kGels, Routine::kGelst, Routine::kGetsls}) {
    Storage<T>(test, provider, routine);
    OriginalStride<T>(test, provider, routine);
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Run<float>(test, provider);
  Run<double>(test, provider);
  Run<std::complex<float>>(test, provider);
  Run<std::complex<double>>(test, provider);
  return test.Finish();
}
