#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
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
#include "lapack_build_config.h"
#include "lu_info_faults.h"

namespace {
using asc_dense_test::TestContext;
using asc_lapack_test::LuInfoFault;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kLu = asc::LapackFactorFamily::kLuPartialPivot;
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr std::int64_t kSentinel =
    ASC_LAPACK_INTEGER_BITS == 64 ? std::numeric_limits<std::int64_t>::min()
                                  : std::numeric_limits<std::int32_t>::min();
static_assert(std::endian::native == std::endian::little);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "LU INFO setup failed: %d\n",
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
auto Wide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::complex<long double>{value.real(), value.imag()};
  } else {
    return static_cast<long double>(value);
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

void CheckReport(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 const asc::Status& status, const asc::LapackReport& report,
                 LuInfoFault fault, std::string_view routine, bool factor) {
  const auto observation = asc_lapack_test::ObserveLuInfo();
  ASC_DENSE_TEST_EQ(test, observation.calls, 1U);
  ASC_DENSE_TEST_EQ(test, observation.actual, 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()), routine);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.factor_family.has_value(), factor);
  if (factor) {
    ASC_DENSE_TEST_EQ(test, report.factor_family, kLu);
  }
  if (fault == LuInfoFault::kNone) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  } else {
    // The old adapter falsely succeeds here after the exact same real call.
    ASC_DENSE_TEST_EQ(test, observation.incoming, kSentinel);
    ASC_DENSE_TEST_EQ(test, observation.outgoing, kSentinel);
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.native_info, kSentinel);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kProviderArgument);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
  }
  std::printf(
      "LU_INFO routine=%.*s fault=%d native_actual=%lld incoming=%lld "
      "outgoing=%lld status=%d\n",
      static_cast<int>(routine.size()), routine.data(), static_cast<int>(fault),
      static_cast<long long>(observation.actual),
      static_cast<long long>(observation.incoming),
      static_cast<long long>(observation.outgoing),
      static_cast<int>(status.code()));
}

template <typename T>
void Fill(Matrix<T>& matrix) {
  for (int i = 0; i < matrix.rows; ++i) {
    for (int j = 0; j < matrix.columns; ++j) {
      matrix.At(i, j) = Value<T>((i == j ? 11 : 0) + (i * 3 + j * 2) % 5 - 2,
                                 (i + j * 2) % 3 - 1);
    }
  }
  if (matrix.rows > 1) {
    matrix.At(0, 0) = T{};
    matrix.At(matrix.rows - 1, 0) = Value<T>(21, 2);
  }
}

template <typename T>
void Reconstruction(TestContext& test, const Matrix<T>& original,
                    const Matrix<T>& factor,
                    const std::array<asc::index_t, 6>& pivots) {
  auto permuted = original;
  const int k = std::min(original.rows, original.columns);
  for (int i = 0; i < k; ++i) {
    ASC_DENSE_TEST_CHECK(
        test, pivots[i + 1] >= i + 1 && pivots[i + 1] <= original.rows);
    if (pivots[i + 1] < i + 1 || pivots[i + 1] > original.rows) {
      return;
    }
    for (int j = 0; j < original.columns; ++j) {
      std::swap(permuted.At(i, j),
                permuted.At(static_cast<int>(pivots[i + 1]) - 1, j));
    }
  }
  for (int i = 0; i < original.rows; ++i) {
    for (int j = 0; j < original.columns; ++j) {
      decltype(Wide(T{})) sum{};
      for (int x = 0; x < k; ++x) {
        T l{};
        if (i == x) {
          l = T{1};
        } else if (i > x) {
          l = factor.At(i, x);
        }
        const T u = x <= j ? factor.At(x, j) : T{};
        sum += Wide(l) * Wide(u);
      }
      ASC_DENSE_TEST_CHECK(
          test,
          std::abs(sum - Wide(permuted.At(i, j))) <=
              512.L *
                  std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
    }
  }
}

template <typename T>
Matrix<T> Rhs(const Matrix<T>& original, asc::DenseBlasLayout rhs_layout,
              asc::DenseBlasTranspose trans, int nrhs) {
  Matrix<T> rhs_original(original.rows, nrhs, rhs_layout);
  for (int i = 0; i < original.rows; ++i) {
    for (int j = 0; j < nrhs; ++j) {
      T sum{};
      for (int k = 0; k < original.rows; ++k) {
        T a = trans == asc::DenseBlasTranspose::kNone ? original.At(i, k)
                                                      : original.At(k, i);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (trans == asc::DenseBlasTranspose::kConjugateTranspose) {
            a = std::conj(a);
          }
        }
        sum += a * Value<T>(k + j + 1, k - j);
      }
      rhs_original.At(i, j) = sum;
    }
  }
  return rhs_original;
}

template <typename T>
void SolveCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               const Matrix<T>& original, const Matrix<T>& good,
               const std::array<asc::index_t, 6>& good_pivots,
               asc::LapackLuFactorView<T> factor_view,
               asc::DenseBlasLayout rhs_layout, asc::DenseBlasTranspose trans,
               int nrhs, const std::array<char, 7>& getrs) {
  const auto rhs_original = Rhs(original, rhs_layout, trans, nrhs);
  const auto immutable = good.data;
  const auto immutable_pivots = good_pivots;
  auto good_rhs = rhs_original;
  for (auto fault :
       {LuInfoFault::kNone, LuInfoFault::kWithhold, LuInfoFault::kShortZero}) {
    if (fault == LuInfoFault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    std::printf(
        "LU_INFO_MODE routine=%s m=%d n=%d layout_a=%d layout_b=%d trans=%d "
        "nrhs=%d fault=%d\n",
        getrs.data(), original.rows, original.columns,
        static_cast<int>(good.layout), static_cast<int>(rhs_layout),
        static_cast<int>(trans), nrhs, static_cast<int>(fault));
    auto rhs = rhs_original;
    const auto plan = Take(
        asc::QueryGetrsWorkspace(provider, trans, factor_view, rhs.View()));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    report.native_argument = 199;
    report.diagnostic_index = 200;
    report.factor_family = kLu;
    asc_lapack_test::SetLuInfoFault(fault);
    const auto status = Observe(test, [&] {
      return asc::Getrs(provider, trans, factor_view, rhs.View(), plan,
                        workspace, report);
    });
    CheckReport(test, provider, status, report, fault, getrs.data(), false);
    scratch.CheckGuards(test);
    rhs.CheckPadding(test);
    ASC_DENSE_TEST_EQ(test, good.data, immutable);
    ASC_DENSE_TEST_EQ(test, good_pivots, immutable_pivots);
    if (fault == LuInfoFault::kNone) {
      for (int i = 0; i < original.rows; ++i) {
        for (int j = 0; j < nrhs; ++j) {
          ASC_DENSE_TEST_CHECK(
              test,
              std::abs(Wide(rhs.At(i, j)) - Wide(Value<T>(i + j + 1, i - j))) <=
                  1024.L * std::numeric_limits<
                               asc::DenseBlasRealType<T>>::epsilon());
        }
      }
      good_rhs = rhs;
    } else {
      ASC_DENSE_TEST_EQ(test, rhs.data,
                        rhs_layout == kRow ? rhs_original.data : good_rhs.data);
    }
  }
}

template <typename T>
void FactorCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                const Matrix<T>& original, char scalar) {
  const std::array<char, 7> getrf{scalar, 'g', 'e', 't', 'r', 'f', '\0'};
  const std::array<char, 7> getrs{scalar, 'g', 'e', 't', 'r', 's', '\0'};
  const auto shape = std::array{original.rows, original.columns};
  const auto layout = original.layout;
  auto good = original;
  std::array<asc::index_t, 6> good_pivots;
  good_pivots.fill(-503);
  asc::LapackReport good_report;
  for (auto fault :
       {LuInfoFault::kNone, LuInfoFault::kWithhold, LuInfoFault::kShortZero}) {
    if (fault == LuInfoFault::kShortZero && ASC_LAPACK_INTEGER_BITS != 64) {
      continue;
    }
    std::printf("LU_INFO_MODE routine=%s m=%d n=%d layout_a=%d fault=%d\n",
                getrf.data(), original.rows, original.columns,
                static_cast<int>(layout), static_cast<int>(fault));
    auto factor = original;
    std::array<asc::index_t, 6> pivots;
    pivots.fill(-503);
    const auto before = pivots;
    auto pivot_view = Pivots(pivots, std::min(shape[0], shape[1]));
    const auto plan =
        Take(asc::QueryGetrfWorkspace(provider, factor.View(), pivot_view));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    report.native_argument = 197;
    report.diagnostic_index = 198;
    asc_lapack_test::SetLuInfoFault(fault);
    const auto status = Observe(test, [&] {
      return asc::Getrf(provider, factor.View(), pivot_view, plan, workspace,
                        report);
    });
    CheckReport(test, provider, status, report, fault, getrf.data(), true);
    scratch.CheckGuards(test);
    factor.CheckPadding(test);
    if (fault == LuInfoFault::kNone) {
      Reconstruction(test, original, factor, pivots);
      ASC_DENSE_TEST_EQ(test, pivots.front(), -503);
      for (std::size_t i =
               static_cast<std::size_t>(std::min(shape[0], shape[1])) + 1;
           i < pivots.size(); ++i) {
        ASC_DENSE_TEST_EQ(test, pivots[i], -503);
      }
      good = factor;
      good_pivots = pivots;
      good_report = report;
    } else {
      ASC_DENSE_TEST_EQ(test, pivots, before);
      ASC_DENSE_TEST_EQ(test, factor.data,
                        layout == kRow ? original.data : good.data);
    }
  }
  if (shape[0] != shape[1]) {
    return;
  }
  const auto factor_view = Take(asc::LapackLuFactorView<T>::Create(
      good.ConstView(), Raw(good_pivots, shape[0]), good_report));
  for (auto rhs_layout : {kRow, kColumn}) {
    for (auto trans :
         {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
          asc::DenseBlasTranspose::kConjugateTranspose}) {
      for (int nrhs : {1, 2}) {
        SolveCase(test, provider, original, good, good_pivots, factor_view,
                  rhs_layout, trans, nrhs, getrs);
      }
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider,
         char scalar) {
  for (const auto shape : {std::array{1, 1}, std::array{3, 3}, std::array{4, 3},
                           std::array{3, 4}}) {
    for (const auto layout : {kRow, kColumn}) {
      Matrix<T> original(shape[0], shape[1], layout);
      Fill(original);
      FactorCase(test, provider, original, scalar);
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
    Run<float>(test, provider, 's');
  } else if (scalar == "d") {
    Run<double>(test, provider, 'd');
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider, 'c');
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider, 'z');
  } else {
    return 2;
  }
  std::printf("LU_INFO_COMPLETED scalar=%s abi=%d cases=%d\n", argv[1],
              ASC_LAPACK_INTEGER_BITS,
              ASC_LAPACK_INTEGER_BITS == 64 ? 168 : 112);
  return test.Finish();
}
