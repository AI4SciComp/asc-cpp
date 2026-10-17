#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_expert_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rook_condition_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_rook_condition_test::Condition;
using asc_rook_condition_test::Query;
using base::TestContext;

template <typename T>
void Metadata(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array<T, 12> a{};
  const std::array<asc::index_t, 4> p{31, -1, -2, 37};
  const auto view = base::Matrix(a, 2, 2, base::kColumn, 3);
  const auto raw = base::Raw(p, 2);
  Real condition = -13;
  for (const Real norm : {Real{-1}, std::numeric_limits<Real>::infinity(),
                          std::numeric_limits<Real>::quiet_NaN()}) {
    ASC_DENSE_TEST_EQ(
        test,
        Query(provider, base::kUpper, hermitian, view, raw, norm, condition)
            .status()
            .code(),
        asc::ErrorCode::kInvalidArgument);
  }
  const auto wrong = base::Take(asc::RawLapackPivotView::Create(
      p.data() + 1, 2, asc::LapackFactorFamily::kBunchKaufman,
      {p.data(), sizeof(p), base::kHost}));
  ASC_DENSE_TEST_EQ(
      test,
      Query(provider, base::kUpper, hermitian, view, wrong, Real{1}, condition)
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  const auto short_pivots = base::Raw(p, 1);
  ASC_DENSE_TEST_EQ(test,
                    Query(provider, base::kUpper, hermitian, view, short_pivots,
                          Real{1}, condition)
                        .status()
                        .code(),
                    asc::ErrorCode::kShape);
  const auto rectangular = base::Matrix(a, 2, 1, base::kColumn, 3);
  ASC_DENSE_TEST_EQ(test,
                    Query(provider, base::kUpper, hermitian, rectangular, raw,
                          Real{1}, condition)
                        .status()
                        .code(),
                    asc::ErrorCode::kShape);
  // Intentional invalid fixed-underlying enum exercises public rejection.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid = static_cast<asc::DenseBlasTriangle>(99);
  ASC_DENSE_TEST_EQ(
      test,
      Query(provider, invalid, hermitian, view, raw, Real{1}, condition)
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  // std::complex permits access to its real component via the scalar pointer.
  const Real& overlapping = *reinterpret_cast<const Real*>(a.data() + 1);
  ASC_DENSE_TEST_EQ(
      test,
      Query(provider, base::kUpper, hermitian, view, raw, Real{1}, overlapping)
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  const auto inaccessible =
      base::Take(asc::DenseBlasMatrixView<const T>::Create(
          a.data() + 1, 2, 2, base::kColumn, 3,
          {a.data(), sizeof(a), asc::MemorySpace::kPinnedHost}));
  ASC_DENSE_TEST_EQ(test,
                    Query(provider, base::kUpper, hermitian, inaccessible, raw,
                          Real{1}, condition)
                        .status()
                        .code(),
                    asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, condition, Real{-13});
  namespace counts = asc::internal_indefinite_expert_counts;
  constexpr asc::extent_t kLimit = std::numeric_limits<lapack_int>::max();
  ASC_DENSE_TEST_CHECK(test, counts::Estimator(kLimit / 3, kLimit).ok());
  ASC_DENSE_TEST_EQ(test, counts::Estimator(kLimit / 3 + 1, kLimit).code(),
                    asc::ErrorCode::kOverflow);
}

template <typename T>
void Pivots(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool hermitian, asc::DenseBlasTriangle triangle) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 12> a{};
  a[2] = T{1};
  a[4] = T{1};
  const auto view = base::Matrix(std::as_const(a), 2, 2, base::kColumn, 3);
  std::array<asc::index_t, 4> p{31, -1, -2, 37};
  const auto raw = base::Raw(p, 2);
  Real condition = -13;
  const auto plan = base::Take(
      Query(provider, triangle, hermitian, view, raw, Real{1}, condition));
  for (const auto pair :
       {std::array<asc::index_t, 2>{0, 2},
        {-1, 2},
        {-3, -2},
        {-1, -3},
        {std::numeric_limits<asc::index_t>::min(), -2},
        triangle == base::kUpper ? std::array<asc::index_t, 2>{2, 2}
                                 : std::array<asc::index_t, 2>{1, 1}}) {
    p[1] = pair[0];
    p[2] = pair[1];
    const auto saved_a = a;
    const auto saved_p = p;
    base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 91;
    const auto status = base::WithoutAllocation(test, [&] {
      return Condition(provider, triangle, hermitian, view, raw, Real{1},
                       condition, plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, condition, Real{-13});
    ASC_DENSE_TEST_CHECK(
        test,
        base::EqualBytes(a.data(), saved_a.data(), sizeof(a)) && p == saved_p);
    const base::Scratch<T> unused;
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(scratch.scalar.data(), unused.scalar.data(),
                               sizeof(scratch.scalar)));
    ASC_DENSE_TEST_CHECK(test, scratch.pivot == unused.pivot);
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(scratch.packed.data(), unused.packed.data(),
                               sizeof(scratch.packed)));
  }
}

template <typename T>
void Aliases(TestContext& test, const asc::ReferenceLapackProvider& provider,
             bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array<T, 3> a{T{-7}, T{4}, T{-11}};
  const std::array<asc::index_t, 3> p{31, 1, 37};
  const auto view = base::Matrix(a, 1, 1, base::kColumn, 1);
  const auto raw = base::Raw(p, 1);
  Real condition = -13;
  auto plan = base::Take(
      Query(provider, base::kUpper, hermitian, view, raw, Real{4}, condition));
  base::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 97;
  std::array<std::byte, sizeof(report)> saved;
  std::memcpy(saved.data(), &report, sizeof(report));
  // Metadata-only rejection must precede both report reset and operand reads.
  const auto aliased = base::Take(asc::DenseBlasMatrixView<const T>::Create(
      reinterpret_cast<const T*>(&report), 1, 1, base::kColumn, 1,
      {&report, sizeof(report), base::kHost}));
  const auto status = Condition(provider, base::kUpper, hermitian, aliased, raw,
                                Real{4}, condition, plan, workspace, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(&report, saved.data(), sizeof(report)));
  Real& scratch_output = *reinterpret_cast<Real*>(scratch.scalar.data() + 1);
  const Real before = scratch_output;
  ASC_DENSE_TEST_EQ(test,
                    Condition(provider, base::kUpper, hermitian, view, raw,
                              Real{4}, scratch_output, plan, workspace, report)
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, scratch_output, before);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  // A changed zero-norm branch cannot reuse an active-call plan.
  ASC_DENSE_TEST_EQ(test,
                    Condition(provider, base::kUpper, hermitian, view, raw,
                              Real{}, condition, plan, workspace, report)
                        .code(),
                    asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, condition, Real{-13});
}

#if defined(__linux__)
template <typename T>
void NoReads(TestContext& test, const asc::ReferenceLapackProvider& provider,
             bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  const long page_size = sysconf(_SC_PAGESIZE);  // NOLINT(google-runtime-int)
  ASC_DENSE_TEST_CHECK(test, page_size >= 2048);
  if (page_size < 2048) {
    return;
  }
  const auto bytes = static_cast<std::size_t>(page_size);
  void* page =
      mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, page != MAP_FAILED);
  if (page == MAP_FAILED) {
    return;
  }
  const auto* matrix = static_cast<const T*>(page);
  const auto* pivots = reinterpret_cast<const asc::index_t*>(
      static_cast<const std::byte*>(page) + 1024);
  const auto raw = base::Take(asc::RawLapackPivotView::Create(
      pivots, 2, asc::LapackFactorFamily::kRook,
      {pivots, 2 * sizeof(asc::index_t), base::kHost}));
  const auto view = base::Take(asc::DenseBlasMatrixView<const T>::Create(
      matrix, 2, 2, base::kColumn, 2, {matrix, 4 * sizeof(T), base::kHost}));
  Real condition = -13;
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return Query(provider, base::kUpper, hermitian, view, raw, Real{},
                 condition);
  }));
  asc::LapackWorkspace empty;
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, base::WithoutAllocation(test, [&] {
                               return Condition(provider, base::kUpper,
                                                hermitian, view, raw, Real{},
                                                condition, plan, empty, report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, condition, Real{});
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, munmap(page, bytes), 0);
}
#endif

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Metadata<T>(test, provider, hermitian);
  for (const auto triangle : {base::kUpper, base::kLower}) {
    Pivots<T>(test, provider, hermitian, triangle);
  }
  Aliases<T>(test, provider, hermitian);
#if defined(__linux__)
  NoReads<T>(test, provider, hermitian);
#endif
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
