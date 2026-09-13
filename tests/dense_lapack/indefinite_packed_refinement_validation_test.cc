#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

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
#include "indefinite_packed_refinement_operands.h"
#include "indefinite_packed_refinement_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"

namespace {
namespace base = asc_indefinite_test;
namespace refinement = asc_packed_refinement_test;
using base::TestContext;

template <typename T>
auto Packed(const T* data, asc::extent_t n, asc::DenseBlasLayout layout,
            asc::MemorySpace space = base::kHost) {
  return base::Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      data, n, layout, {data, 32 * sizeof(T), space}));
}
template <typename T>
auto Matrix(T* data, asc::extent_t n, asc::extent_t nrhs,
            asc::DenseBlasLayout layout, asc::extent_t leading,
            asc::MemorySpace space = base::kHost) {
  return base::Take(asc::DenseBlasMatrixView<T>::Create(
      data, n, nrhs, layout, leading, {data, 32 * sizeof(T), space}));
}
template <typename Real>
auto Error(Real* data, asc::extent_t n = 2, asc::stride_t increment = 1,
           asc::MemorySpace space = base::kHost) {
  return base::Take(asc::DenseBlasVectorView<Real>::Create(
      data, n, increment, {data, 4 * sizeof(Real), space}));
}
auto Pivots(
    const asc::index_t* data, asc::extent_t n,
    asc::LapackFactorFamily family = asc::LapackFactorFamily::kBunchKaufman,
    asc::MemorySpace space = base::kHost) {
  return base::Take(asc::RawLapackPivotView::Create(
      data, n, family, {data, 4 * sizeof(asc::index_t), space}));
}
asc::DenseBlasLayout Other(asc::DenseBlasLayout layout) {
  return layout == base::kRow ? base::kColumn : base::kRow;
}

template <typename T>
void MetadataLayout(int bad, refinement::Operands<T>& v) {
  switch (bad) {
    case 19:
      v.a = Packed(v.a.data(), 2, Other(v.a.layout()));
      break;
    case 20:
      v.af = Packed(v.af.data(), 2, Other(v.af.layout()));
      break;
    case 21:
      v.b = Matrix(v.b.data(), 2, 2, Other(v.b.layout()), 4);
      break;
    case 22:
      v.x = Matrix(v.x.data(), 2, 2, Other(v.x.layout()), 4);
      break;
    case 23:
      v.b = Matrix(v.b.data(), 2, 2, v.b.layout(), 5);
      break;
    case 24:
      v.x = Matrix(v.x.data(), 2, 2, v.x.layout(), 5);
      break;
    case 25:
      v.triangle = v.triangle == base::kUpper ? base::kLower : base::kUpper;
      break;
    default:
      break;
  }
}

template <typename T>
void Metadata(int bad, refinement::Operands<T>& v) {
  constexpr auto kInaccessible = asc::MemorySpace::kPinnedHost;
  switch (bad) {
    case 0:
      v.a = Packed(v.a.data(), 3, v.a.layout());
      break;
    case 1:
      v.af = Packed(v.af.data(), 3, v.af.layout());
      break;
    case 2:
      v.b = Matrix(v.b.data(), 3, 2, v.b.layout(), 4);
      break;
    case 3:
      v.x = Matrix(v.x.data(), 2, 1, v.x.layout(), 4);
      break;
    case 4:
      v.ferr = Error(v.ferr.data(), 1);
      break;
    case 5:
      v.berr = Error(v.berr.data(), 3);
      break;
    case 6:
      v.ferr = Error(v.ferr.data(), 2, 2);
      break;
    case 7:
      v.berr = Error(v.berr.data(), 2, 2);
      break;
    case 8:
      v.pivots = Pivots(v.pivots.values().data(), 1);
      break;
    case 9:
      v.pivots = Pivots(v.pivots.values().data(), 3);
      break;
    case 10:
      v.pivots =
          Pivots(v.pivots.values().data(), 2, asc::LapackFactorFamily::kRook);
      break;
    case 11:
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      v.triangle = static_cast<asc::DenseBlasTriangle>(99);
      break;
    case 12:
      v.a = Packed(v.a.data(), 2, v.a.layout(), kInaccessible);
      break;
    case 13:
      v.af = Packed(v.af.data(), 2, v.af.layout(), kInaccessible);
      break;
    case 14:
      v.b = Matrix(v.b.data(), 2, 2, v.b.layout(), 4, kInaccessible);
      break;
    case 15:
      v.x = Matrix(v.x.data(), 2, 2, v.x.layout(), 4, kInaccessible);
      break;
    case 16:
      v.ferr = Error(v.ferr.data(), 2, 1, kInaccessible);
      break;
    case 17:
      v.berr = Error(v.berr.data(), 2, 1, kInaccessible);
      break;
    case 18:
      v.pivots = Pivots(v.pivots.values().data(), 2,
                        asc::LapackFactorFamily::kBunchKaufman, kInaccessible);
      break;

    default:
      break;
  }
}

// Every unordered pair of reachable operand ranges is tested for overlap.
template <typename T>
void Alias(int pair, refinement::Operands<T>& v, T* storage) {
  using Real = asc::DenseBlasRealType<T>;
  int count = 0;
  for (int i = 0; i < 7; ++i) {
    for (int j = i + 1; j < 7; ++j) {
      if (count++ != pair) {
        continue;
      }
      // The shared aligned storage is large enough for either complete view.
      // Rejection precedes accessing these deliberately overlapping types.
      for (const int operand : {i, j}) {
        switch (operand) {
          case 0:
            v.a = Packed(storage, 2, v.a.layout());
            break;
          case 1:
            v.af = Packed(storage, 2, v.af.layout());
            break;
          case 2:
            v.b = Matrix(static_cast<const T*>(storage), 2, 2, v.b.layout(), 3);
            break;
          case 3:
            v.x = Matrix(storage, 2, 2, v.x.layout(), 4);
            break;
          case 4:
            v.ferr = Error(reinterpret_cast<Real*>(storage));
            break;
          case 5:
            v.berr = Error(reinterpret_cast<Real*>(storage));
            break;
          case 6:
            v.pivots =
                Pivots(reinterpret_cast<const asc::index_t*>(storage), 2);
            break;
          default:
            break;
        }
      }
    }
  }
}

template <typename T>
void PivotFault(int bad, refinement::Fixture<T>& sample) {
  auto& p = sample.system.a.pivots;
  const auto triangle = sample.system.a.triangle;
  switch (bad) {
    case 0:
      p[1] = 0;
      break;
    case 1:
      p[1] = -1;
      break;
    case 2:
      p[2] = 3;
      break;
    case 3:
      p[triangle == base::kUpper ? 1U : 2U] = triangle == base::kUpper ? 2 : 1;
      break;
    case 4:
      p[1] = -1;
      p[2] = -2;
      break;
    case 5:
      p[1] = triangle == base::kUpper ? -2 : -1;
      p[2] = p[1];
      break;
    case 6:
      p[1] = std::numeric_limits<asc::index_t>::min();
      break;
    case 7:
      p[2] = -1;
      break;
    default:
      break;
  }
}

template <typename T>
void WorkspaceFault(int bad, std::size_t role, refinement::Operands<T>& v,
                    asc::LapackWorkspacePlan& plan,
                    asc::LapackWorkspace& work) {
  const auto original = work.regions[role];
  switch (bad) {
    case 0:
      work.regions[role] = {nullptr, 0, base::kHost};
      break;
    case 1:
      work.regions[role] = {original.data(), original.size() - 1, base::kHost};
      break;
    case 2:
      work.regions[role] = {static_cast<std::byte*>(original.data()) + 1,
                            original.size(), base::kHost};
      break;
    case 3:
      work.regions[role] = {original.data(), original.size(),
                            asc::MemorySpace::kPinnedHost};
      break;
    case 4:
      work.regions[role] = {const_cast<T*>(v.a.data()), original.size(),
                            base::kHost};
      break;
    case 5:
      work.regions[role] =
          work.regions[role == base::kScalar ? refinement::kReal
                                             : base::kScalar];
      break;
    case 6:
      ++plan.regions[role].minimum_entries;
      break;
    case 7:
      ++plan.regions[role].preferred_entries;
      break;
    case 8:
      ++plan.regions[role].entry_bytes;
      break;
    case 9:
      ++plan.regions[role].alignment;
      break;
    default:
      break;
  }
}

template <typename T>
void Unchanged(TestContext& test, const refinement::Fixture<T>& sample,
               const refinement::Fixture<T>& saved,
               const refinement::Scratch<T>& scratch,
               const refinement::Scratch<T>& saved_scratch) {
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.original.data(), saved.original.data(),
                             sizeof(sample.original)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.system.a.a.data(), saved.system.a.a.data(),
                             sizeof(sample.system.a.a)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.system.rhs.data(), saved.system.rhs.data(),
                             sizeof(sample.system.rhs)));
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.x.data(), saved.x.data(),
                                              sizeof(sample.x)));
  ASC_DENSE_TEST_EQ(test, sample.system.a.pivots, saved.system.a.pivots);
  ASC_DENSE_TEST_EQ(test, sample.ferr, saved.ferr);
  ASC_DENSE_TEST_EQ(test, sample.berr, saved.berr);
  ASC_DENSE_TEST_EQ(test, scratch.scalar, saved_scratch.scalar);
  ASC_DENSE_TEST_EQ(test, scratch.real, saved_scratch.real);
  ASC_DENSE_TEST_EQ(test, scratch.packed, saved_scratch.packed);
  ASC_DENSE_TEST_EQ(test, scratch.pivot, saved_scratch.pivot);
}

template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool hermitian, asc::DenseBlasTriangle triangle, int layouts,
               int category, int bad, std::size_t role = base::kScalar) {
  refinement::Fixture<T> sample(
      2, 2, hermitian, triangle, (layouts & 1) ? base::kRow : base::kColumn,
      (layouts & 2) ? base::kRow : base::kColumn,
      (layouts & 4) ? base::kRow : base::kColumn,
      (layouts & 8) ? base::kRow : base::kColumn, 0, 3);
  const auto nan = base::Value<T>(std::numeric_limits<double>::quiet_NaN(), 13);
  sample.original.fill(nan);
  sample.system.a.a.fill(nan);
  sample.system.rhs.fill(nan);
  sample.x.fill(nan);
  sample.system.a.pivots[1] = 1;
  sample.system.a.pivots[2] = 2;
  refinement::Operands<T> values(sample);
  auto plan = base::Take(
      base::WithoutAllocation(test, [&] { return values.Query(provider); }));
  refinement::Scratch<T> scratch;
  auto work = scratch.Workspace(plan);
  if (category == 3 && work.regions[role].size() == 0) {
    return;
  }
  if (category == 0) {
    Metadata(bad, values);
    MetadataLayout(bad, values);
  }
  alignas(16) std::array<T, 64> alias_storage{};
  if (category == 1) {
    Alias(bad, values, alias_storage.data());
  }
  const auto saved_alias = alias_storage;
  if (category == 2) {
    PivotFault(bad, sample);
  }
  if (category == 3) {
    WorkspaceFault(bad, role, values, plan, work);
  }
  if (category == 0 || category == 1) {
    const auto query =
        base::WithoutAllocation(test, [&] { return values.Query(provider); });
    ASC_DENSE_TEST_EQ(test, query.ok(), category == 0 && bad >= 19);
  }
  const auto saved = sample;
  const auto saved_scratch = scratch;
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 97;
  const auto status = base::WithoutAllocation(
      test, [&] { return values.Execute(provider, plan, work, report); });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  Unchanged(test, sample, saved, scratch, saved_scratch);
  ASC_DENSE_TEST_EQ(test, alias_storage, saved_alias);
}

template <typename T>
void Divisor(TestContext& test, const asc::ReferenceLapackProvider& provider,
             bool hermitian, asc::DenseBlasTriangle triangle, int layouts,
             int kind) {
  refinement::Fixture<T> sample(
      3, 2, hermitian, triangle, (layouts & 1) ? base::kRow : base::kColumn,
      (layouts & 2) ? base::kRow : base::kColumn,
      (layouts & 4) ? base::kRow : base::kColumn,
      (layouts & 8) ? base::kRow : base::kColumn, 0, 3);
  auto& a = sample.system.a;
  a.pivots[1] = 1;
  a.pivots[2] = 2;
  a.pivots[3] = 3;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (a.Selected(i, j)) {
        a.a[a.Offset(i, j)] = i == j ? T{1} : T{};
      }
    }
  }
  if (kind == 0 || kind == 4) {
    a.a[a.Offset(0, 0)] = base::Value<T>(0, kind == 4 ? 1 : 0);
  } else {
    a.pivots[1] = triangle == base::kUpper ? -1 : -2;
    a.pivots[2] = a.pivots[1];
    const auto off = triangle == base::kUpper ? a.Offset(0, 1) : a.Offset(1, 0);
    a.a[off] = kind == 2 ? T{1} : T{};
    a.a[a.Offset(2, 2)] = kind == 3 ? T{} : T{4};
  }
  refinement::Operands<T> values(sample);
  const auto plan = base::Take(values.Query(provider));
  refinement::Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  const auto saved = sample;
  const auto saved_scratch = scratch;
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(
      test, [&] { return values.Execute(provider, plan, work, report); });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  Unchanged(test, sample, saved, scratch, saved_scratch);
}

template <typename T>
void ObjectAliases(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  for (int bad = 0; bad < 8; ++bad) {
    refinement::Fixture<T> sample(2, 2, hermitian, base::kUpper, base::kRow,
                                  base::kRow, base::kRow, base::kRow, 0, 3);
    refinement::Operands<T> values(sample);
    auto plan = base::Take(values.Query(provider));
    refinement::Scratch<T> scratch;
    auto work = scratch.Workspace(plan);
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 97;
    const std::array<void*, 4> objects{
        &report, &plan, &work,
        const_cast<asc::ReferenceLapackProvider*>(&provider)};
    auto* address = objects[bad % 4];
    if (bad < 4) {
      values.ferr = Error(static_cast<Real*>(address));
    } else {
      work.regions[refinement::kReal] = {
          address, work.regions[refinement::kReal].size(), base::kHost};
    }
    std::array<std::byte, sizeof(report)> before_report;
    std::array<std::byte, sizeof(plan)> before_plan;
    std::array<std::byte, sizeof(work)> before_work;
    std::array<std::byte, sizeof(provider)> before_provider;
    std::memcpy(before_report.data(), &report, sizeof(report));
    std::memcpy(before_plan.data(), &plan, sizeof(plan));
    std::memcpy(before_work.data(), &work, sizeof(work));
    std::copy_n(reinterpret_cast<const std::byte*>(&provider),
                before_provider.size(), before_provider.begin());
    const auto saved = sample;
    const auto saved_scratch = scratch;
    const auto status = base::WithoutAllocation(
        test, [&] { return values.Execute(provider, plan, work, report); });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&report, before_report.data(), sizeof(report)));
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&plan, before_plan.data(), sizeof(plan)));
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(&work, before_work.data(), sizeof(work)));
    ASC_DENSE_TEST_CHECK(
        test,
        base::EqualBytes(&provider, before_provider.data(), sizeof(provider)));
    Unchanged(test, sample, saved, scratch, saved_scratch);
  }
}

template <typename T>
void Unread(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool hermitian) {
#if defined(__linux__)
  using Real = asc::DenseBlasRealType<T>;
  const auto page = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, page > 0);
  if (page <= 0) {
    return;
  }
  const auto bytes = static_cast<std::size_t>(page);
  void* memory =
      mmap(nullptr, 5 * bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, memory != MAP_FAILED);
  if (memory == MAP_FAILED) {
    return;
  }
  auto* address = static_cast<std::byte*>(memory);
  for (auto triangle : {base::kUpper, base::kLower}) {
    for (auto layout : {base::kColumn, base::kRow}) {
      for (int n : {0, 1, 2}) {
        for (int nrhs : {0, 1, 3}) {
          refinement::Fixture<T> sample(n, nrhs, hermitian, triangle, layout,
                                        layout, layout, layout, 0, 3);
          refinement::Operands<T> v(sample);
          v.a = Packed(reinterpret_cast<const T*>(address), n, layout);
          v.af = Packed(reinterpret_cast<const T*>(address + bytes), n, layout);
          v.pivots = Pivots(
              reinterpret_cast<const asc::index_t*>(address + 2 * bytes), n);
          v.b = Matrix(reinterpret_cast<const T*>(address + 3 * bytes), n, nrhs,
                       layout, 4);
          v.x = Matrix(reinterpret_cast<T*>(address + 4 * bytes), n, nrhs,
                       layout, 4);
          const auto plan = base::Take(
              base::WithoutAllocation(test, [&] { return v.Query(provider); }));
          ASC_DENSE_TEST_EQ(test, sample.ferr[1], Real{-293});
          ASC_DENSE_TEST_EQ(test, sample.berr[1], Real{-307});
          asc::LapackReport report;
          const auto status = base::WithoutAllocation(
              test, [&] { return v.Execute(provider, plan, {}, report); });
          const bool empty = n == 0 || nrhs == 0;
          ASC_DENSE_TEST_EQ(test, status.ok(), empty);
          ASC_DENSE_TEST_CHECK(
              test, !report.called_provider && !report.native_info.has_value());
          for (int j = 0; j < nrhs; ++j) {
            ASC_DENSE_TEST_EQ(test, sample.ferr[j + 1],
                              empty ? Real{0} : Real{-293});
            ASC_DENSE_TEST_EQ(test, sample.berr[j + 1],
                              empty ? Real{0} : Real{-307});
          }
          sample.Padding(test);
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, 5 * bytes), 0);
#else
  (void)test;
  (void)provider;
  (void)hermitian;
#endif
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto triangle : {base::kUpper, base::kLower}) {
    for (int layouts = 0; layouts < 16; ++layouts) {
      for (int category = 0; category < 4; ++category) {
        const int count = std::array{26, 21, 8, 10}[category];
        for (int bad = 0; bad < count; ++bad) {
          if (category == 3) {
            for (auto role : {base::kScalar, base::kPivot, refinement::kReal,
                              refinement::kLayout}) {
              Rejection<T>(test, provider, hermitian, triangle, layouts,
                           category, bad, role);
            }
          } else {
            Rejection<T>(test, provider, hermitian, triangle, layouts, category,
                         bad);
          }
        }
      }
      for (int kind = 0; kind < (hermitian ? 5 : 4); ++kind) {
        Divisor<T>(test, provider, hermitian, triangle, layouts, kind);
      }
    }
  }
  ObjectAliases<T>(test, provider, hermitian);
  Unread<T>(test, provider, hermitian);
  std::printf(
      "Packed refinement validation: 32 layout/triangle combinations; "
      "metadata, all 21 operand pairs, pivot encodings, work regions, "
      "divisors, protected nonreads\n");
  return test.Finish();
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
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
