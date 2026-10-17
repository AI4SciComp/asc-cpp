#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_info_faults.h"

namespace {
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kNone = asc::DenseBlasTranspose::kNone;
constexpr auto kTranspose = asc::DenseBlasTranspose::kTranspose;
constexpr auto kConjugate = asc::DenseBlasTranspose::kConjugateTranspose;
constexpr auto kLu = asc::LapackFactorFamily::kLuPartialPivot;
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "LU metadata setup failed: %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}

template <typename T>
T Value(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
struct Matrix {
  int rows;
  int columns;
  int ld;
  asc::DenseBlasLayout layout;
  std::array<T, 64> data;
  Matrix(int m, int n, asc::DenseBlasLayout order)
      : rows(m), columns(n), ld((order == kRow ? n : m) + 2), layout(order) {
    data.fill(Value<T>(-791, 37));
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return static_cast<std::size_t>(layout == kRow ? i * ld + j : j * ld + i);
  }
  T& At(int i, int j) { return data[Offset(i, j)]; }
  [[nodiscard]] T At(int i, int j) const { return data[Offset(i, j)]; }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data(), rows, columns, layout, ld,
        {data.data(), sizeof(data), kHost}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data(), rows, columns, layout, ld,
        {data.data(), sizeof(data), kHost}));
  }
  void CheckPadding(TestContext& test) const {
    for (std::size_t i = 0; i < data.size(); ++i) {
      if (i / ld >= static_cast<std::size_t>(layout == kRow ? rows : columns) ||
          i % ld >= static_cast<std::size_t>(layout == kRow ? columns : rows)) {
        ASC_DENSE_TEST_EQ(test, data[i], Value<T>(-791, 37));
      }
    }
  }
};

template <typename T>
struct Scratch {
  alignas(std::max_align_t) std::array<std::byte, 128> integers;
  std::array<T, 66> packing;
  std::size_t integer_bytes = 0;
  std::size_t packing_entries = 0;
  Scratch() {
    integers.fill(std::byte{0x5a});
    packing.fill(Value<T>(-797, 41));
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    integer_bytes =
        static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
        plan.regions[kInteger].entry_bytes;
    packing_entries =
        static_cast<std::size_t>(plan.regions[kLayout].minimum_entries);
    result.regions[kInteger] = {integers.data() + 16, integer_bytes, kHost};
    if (packing_entries != 0) {
      result.regions[kLayout] = {packing.data() + 1,
                                 packing_entries * sizeof(T), kHost};
    }
    return result;
  }
  void CheckGuards(TestContext& test) const {
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + integer_bytes) {
        ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
      }
    }
    for (std::size_t i = 0; i < packing.size(); ++i) {
      if (i == 0 || i > packing_entries) {
        ASC_DENSE_TEST_EQ(test, packing[i], Value<T>(-797, 41));
      }
    }
  }
};

auto Pivots(std::array<asc::index_t, 6>& values, int count) {
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      values.data() + 1, count, 1, {values.data(), sizeof(values), kHost}));
}
auto Raw(const std::array<asc::index_t, 6>& values, int count) {
  return Take(asc::RawLapackPivotView::Create(
      values.data() + 1, count, kLu, {values.data(), sizeof(values), kHost}));
}

template <typename Operation>
auto Observe(TestContext& test, Operation operation) {
  asc_dense_test::AllocationProbe cpp;
  asc_lapack_test::BeginAllocationAudit();
  auto result = operation();
  const auto count = asc_lapack_test::EndAllocationAudit();
  ASC_DENSE_TEST_EQ(test, count, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp.count(), 0));
  return result;
}

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

template <typename Operation, typename... Objects>
auto Preserving(TestContext& test, Operation operation,
                const Objects&... objects) {
  const auto before = std::tuple{Snapshot(objects)...};
  auto result = Observe(test, operation);
  std::apply(
      [&](const auto&... bytes) { (Preserved(test, objects, bytes), ...); },
      before);
  return result;
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

void ResetEntry() {
  asc_lapack_test::SetLuInfoFault(asc_lapack_test::LuInfoFault::kNone);
}

void NoEntry(TestContext& test) {
  ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuInfo().calls,
                    std::size_t{0});
}

template <typename T>
auto FactorView(const Matrix<T>& matrix,
                const std::array<asc::index_t, 6>& pivots,
                const asc::LapackReport& report) {
  return Take(asc::LapackLuFactorView<T>::Create(
      matrix.ConstView(), Raw(pivots, matrix.rows), report));
}

template <typename T>
auto MetadataObjects(asc::ReferenceLapackProvider& provider,
                     asc::LapackWorkspacePlan& plan,
                     asc::LapackWorkspace& workspace,
                     asc::LapackReport& report) {
  static_assert(std::is_same_v<asc::index_t, asc::extent_t>);
  static_assert(sizeof(provider) >= sizeof(T));
  static_assert(sizeof(plan) >= sizeof(T));
  static_assert(sizeof(workspace) >= sizeof(T));
  static_assert(sizeof(report) >= sizeof(T));
  static_assert(alignof(asc::ReferenceLapackProvider) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspacePlan) >= alignof(T));
  static_assert(alignof(asc::LapackWorkspace) >= alignof(T));
  static_assert(alignof(asc::LapackReport) >= alignof(T));
  return std::array<void*, 4>{&provider, &plan, &workspace, &report};
}

template <typename T>
void Alias(TestContext& test, bool solve, bool empty, asc::DenseBlasLayout al,
           asc::DenseBlasLayout bl, asc::DenseBlasTranspose trans,
           std::size_t object, std::size_t target) {
  auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const int n = empty ? 0 : 1;
  Matrix<T> matrix(n, n, al);
  Matrix<T> rhs(n, 1, bl);
  if (!empty) {
    matrix.At(0, 0) = Value<T>(4, 2);
    rhs.At(0, 0) = Value<T>(8, 4);
  }
  std::array<asc::index_t, 6> pivots{-97, -97, -97, -97, -97, -97};
  auto a = matrix.View();
  auto p = Pivots(pivots, n);
  auto b = rhs.View();
  auto plan = Take(asc::QueryGetrfWorkspace(provider, a, p));
  Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport provenance;
  ResetEntry();
  ASC_DENSE_TEST_CHECK(
      test, asc::Getrf(provider, a, p, plan, workspace, provenance).ok());
  auto factor = FactorView(matrix, pivots, provenance);
  if (solve) {
    plan = Take(asc::QueryGetrsWorkspace(provider, trans, factor, b));
    workspace = scratch.Workspace(plan);
  }
  auto report = PriorReport();
  report.diagnostic_index = 1;
  const auto objects = MetadataObjects<T>(provider, plan, workspace, report);
  const asc::MutableMemoryView alias{objects[object], sizeof(T), kHost};
  // Live metadata retains its actual C++ type. Numeric descriptors may refer
  // to those bytes but execution must reject before dereferencing them. There
  // are no protected pages, signals, or invalid foreign calls.
  if (target == 0) {
    if (solve) {
      const auto factors = Take(asc::DenseBlasMatrixView<const T>::Create(
          static_cast<const T*>(alias.data()), 1, 1, al, matrix.ld, alias));
      factor = Take(asc::LapackLuFactorView<T>::Create(factors, Raw(pivots, n),
                                                       provenance));
    } else {
      a = Take(asc::DenseBlasMatrixView<T>::Create(
          static_cast<T*>(alias.data()), 1, 1, al, matrix.ld, alias));
    }
  } else if (target == 1) {
    if (solve) {
      // Factor construction validates pivot values. Use a real live signed
      // index subobject equal to one, never reinterpret unrelated metadata
      // bytes as a pivot value. Both aliases must be rejected before reset.
      const asc::index_t* value = object == 1
                                      ? &plan.regions[kInteger].minimum_entries
                                      : &*report.diagnostic_index;
      const auto raw = Take(asc::RawLapackPivotView::Create(
          value, 1, kLu, {value, sizeof(*value), kHost}));
      factor = Take(asc::LapackLuFactorView<T>::Create(matrix.ConstView(), raw,
                                                       provenance));
    } else {
      p = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          static_cast<asc::index_t*>(objects[object]), 1, 1,
          {objects[object], sizeof(asc::index_t), kHost}));
    }
  } else if (target == 2) {
    b = Take(asc::DenseBlasMatrixView<T>::Create(static_cast<T*>(alias.data()),
                                                 1, 1, bl, rhs.ld, alias));
  } else {
    workspace.regions[target - 3] = alias;
  }
  ResetEntry();
  const auto status = Preserving(
      test,
      [&] {
        return solve ? asc::Getrs(provider, trans, factor, b, plan, workspace,
                                  report)
                     : asc::Getrf(provider, a, p, plan, workspace, report);
      },
      provider, plan, workspace, report, matrix.data, rhs.data, pivots,
      scratch);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  NoEntry(test);
}

void CheckComplete(TestContext& test, const asc::LapackReport& report,
                   bool empty) {
  ASC_DENSE_TEST_EQ(test, asc_lapack_test::ObserveLuInfo().calls,
                    empty ? std::size_t{0} : std::size_t{1});
  ASC_DENSE_TEST_EQ(test, report.called_provider, !empty);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), !empty);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
}

template <typename T>
void Controls(TestContext& test, bool solve, asc::DenseBlasLayout al,
              asc::DenseBlasLayout bl, asc::DenseBlasTranspose trans) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (bool empty : {false, true}) {
    const int n = empty ? 0 : 1;
    Matrix<T> matrix(n, n, al);
    Matrix<T> rhs(n, 1, bl);
    const T coefficient = Value<T>(4, 2);
    T op = coefficient;
    if constexpr (asc::DenseBlasComplex<T>) {
      if (trans == kConjugate) {
        op = std::conj(op);
      }
    }
    if (!empty) {
      matrix.At(0, 0) = coefficient;
      rhs.At(0, 0) = op * Value<T>(2);
    }
    std::array<asc::index_t, 6> pivots{-97, -97, -97, -97, -97, -97};
    auto plan = Take(Observe(test, [&] {
      return asc::QueryGetrfWorkspace(provider, matrix.View(),
                                      Pivots(pivots, n));
    }));
    Scratch<T> scratch;
    auto workspace = scratch.Workspace(plan);
    auto report = PriorReport();
    ResetEntry();
    ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                                 return asc::Getrf(provider, matrix.View(),
                                                   Pivots(pivots, n), plan,
                                                   workspace, report);
                               }).ok());
    auto factor = FactorView(matrix, pivots, report);
    if (solve) {
      plan = Take(Observe(test, [&] {
        return asc::QueryGetrsWorkspace(provider, trans, factor, rhs.View());
      }));
      workspace = scratch.Workspace(plan);
      report = PriorReport();
      ResetEntry();
      ASC_DENSE_TEST_CHECK(test, Observe(test, [&] {
                                   return asc::Getrs(provider, trans, factor,
                                                     rhs.View(), plan,
                                                     workspace, report);
                                 }).ok());
    }
    CheckComplete(test, report, empty);
    if (!empty) {
      ASC_DENSE_TEST_EQ(test, report.native_info, 0);
      ASC_DENSE_TEST_EQ(test, matrix.At(0, 0), coefficient);
      ASC_DENSE_TEST_EQ(test, pivots[1], 1);
      if (solve) {
        ASC_DENSE_TEST_EQ(test, rhs.At(0, 0), Value<T>(2));
      }
    }
    matrix.CheckPadding(test);
    rhs.CheckPadding(test);
    scratch.CheckGuards(test);
    // Other structural errors retain the established report-reset behavior,
    // with zero entry and unchanged numeric arrays and workspace storage.
    ++plan.regions[kInteger].minimum_entries;
    report = PriorReport();
    ResetEntry();
    const auto status = Preserving(
        test,
        [&] {
          return solve ? asc::Getrs(provider, trans, factor, rhs.View(), plan,
                                    workspace, report)
                       : asc::Getrf(provider, matrix.View(), Pivots(pivots, n),
                                    plan, workspace, report);
        },
        matrix.data, rhs.data, pivots, scratch);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    NoEntry(test);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  }
}

template <typename T>
void Run(TestContext& test) {
  std::size_t aliases = 0;
  for (auto al : {kRow, kColumn}) {
    for (bool solve : {false, true}) {
      for (auto bl : {kRow, kColumn}) {
        for (auto trans : {kNone, kTranspose, kConjugate}) {
          if (!solve && (bl != kColumn || trans != kNone)) {
            continue;
          }
          for (bool empty : {false, true}) {
            for (std::size_t object = 0; object < 4; ++object) {
              for (std::size_t target = 0; target < 11; ++target) {
                if ((empty && target < 3) || (!solve && target == 2) ||
                    (solve && target == 1 && object != 1 && object != 3)) {
                  continue;
                }
                Alias<T>(test, solve, empty, al, bl, trans, object, target);
                ++aliases;
              }
            }
          }
          Controls<T>(test, solve, al, bl, trans);
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, aliases, std::size_t{1032});
  std::printf("LU_METADATA scalar_bytes=%zu aliases=%zu controls=56\n",
              sizeof(T), aliases);
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
