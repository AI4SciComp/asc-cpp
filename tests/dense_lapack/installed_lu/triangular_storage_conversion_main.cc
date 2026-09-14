#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_triangular_storage_conversion.h"
#include "normal_return_guard.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;

template <typename T>
bool Same(const T& a, const T& b) {
  const auto first = std::as_bytes(std::span(&a, 1));
  const auto second = std::as_bytes(std::span(&b, 1));
  return std::equal(first.begin(), first.end(), second.begin());
}

std::size_t PackedIndex(std::size_t n, asc::DenseBlasTriangle triangle,
                        asc::DenseBlasLayout layout, std::size_t i,
                        std::size_t j) {
  if (layout == kColumn) {
    return triangle == kUpper ? j * (j + 1) / 2 + i
                              : j * (2 * n - j + 1) / 2 + (i - j);
  }
  return triangle == kLower ? i * (i + 1) / 2 + j
                            : i * (2 * n - i + 1) / 2 + (j - i);
}

template <typename T>
T Fixture(std::size_t i, std::size_t j, std::size_t seed) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array<Real, 12> values{Real{0},
                                    -Real{0},
                                    Real{1},
                                    Real{-3},
                                    Real{0.25},
                                    Real{-0.125},
                                    std::numeric_limits<Real>::denorm_min(),
                                    std::numeric_limits<Real>::min(),
                                    std::numeric_limits<Real>::max(),
                                    std::numeric_limits<Real>::infinity(),
                                    -std::numeric_limits<Real>::infinity(),
                                    std::numeric_limits<Real>::quiet_NaN()};
  const auto real = values[(i + 3 * j + seed) % values.size()];
  if constexpr (std::is_same_v<T, Real>) {
    return real;
  } else {
    return {real, values[(2 * i + j + seed + 5) % values.size()]};
  }
}

template <typename T>
struct Scratch {
  std::array<T, 1280> conversion;
  std::array<T, 1280> staging;
  static constexpr auto kLayout =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
  static constexpr auto kScratch =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
  Scratch() {
    conversion.fill(T{-107});
    staging.fill(T{-109});
  }
  asc::Result<asc::LapackWorkspace> Workspace(
      const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    for (auto role : {kLayout, kScratch}) {
      auto& data = role == kLayout ? conversion : staging;
      const auto count = plan.regions[role].minimum_entries;
      if (count < 0 || static_cast<std::size_t>(count) > data.size() - 2) {
        return asc::Status(asc::ErrorCode::kShape);
      }
      if (count > 0) {
        workspace.regions[role] = {data.data() + 1,
                                   static_cast<std::size_t>(count) * sizeof(T),
                                   kHost};
      }
    }
    return workspace;
  }
  [[nodiscard]] bool Guards(const asc::LapackWorkspace& workspace) const {
    bool passed = true;
    for (auto role : {kLayout, kScratch}) {
      const auto& data = role == kLayout ? conversion : staging;
      const auto count = workspace.regions[role].size() / sizeof(T);
      for (std::size_t i = 0; i < data.size(); ++i) {
        if (i == 0 || i > count) {
          passed = passed && data[i] == (role == kLayout ? T{-107} : T{-109});
        }
      }
    }
    return passed;
  }
};

template <typename T>
bool Case(const asc::ReferenceLapackProvider& provider, std::size_t n,
          std::size_t padding, asc::DenseBlasLayout dl, asc::DenseBlasLayout pl,
          asc::DenseBlasTriangle triangle, bool pack, std::size_t seed) {
  std::array<T, 1280> dense;
  std::array<T, 564> packed;
  dense.fill(T{-97});
  packed.fill(T{-101});
  auto expected_dense = dense;
  auto expected_packed = packed;
  const auto leading = std::max(std::size_t{1}, n) + padding;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      const auto index =
          1 + (dl == kColumn ? j * leading + i : i * leading + j);
      const auto value = Fixture<T>(i, j, seed);
      if (pack) {
        dense[index] = value;
        expected_dense[index] = value;
      }
      if (triangle == kUpper ? i <= j : i >= j) {
        const auto p = 1 + PackedIndex(n, triangle, pl, i, j);
        if (pack) {
          expected_packed[p] = value;
        } else {
          packed[p] = value;
          expected_packed[p] = value;
          expected_dense[index] = value;
        }
      }
    }
  }
  const auto matrix_result = asc::DenseBlasMatrixView<T>::Create(
      dense.data() + 1, static_cast<asc::extent_t>(n),
      static_cast<asc::extent_t>(n), dl, static_cast<asc::stride_t>(leading),
      {dense.data(), sizeof(dense), kHost});
  const auto packed_result = asc::DenseBlasPackedMatrixView<T>::Create(
      packed.data() + 1, static_cast<asc::extent_t>(n), pl,
      {packed.data(), sizeof(packed), kHost});
  if (!matrix_result.ok() || !packed_result.ok()) {
    return false;
  }
  const auto matrix = *matrix_result;
  const auto triangle_storage = *packed_result;
  const asc::DenseBlasMatrixView<const T> input_matrix = matrix;
  const asc::DenseBlasPackedMatrixView<const T> input_packed = triangle_storage;
  const auto plan_result =
      pack ? asc::QueryTrttpWorkspace(provider, triangle, input_matrix,
                                      triangle_storage)
           : asc::QueryTpttrWorkspace(provider, triangle, input_packed, matrix);
  if (!plan_result.ok()) {
    return false;
  }
  const auto plan = *plan_result;
  Scratch<T> scratch;
  const auto workspace_result = scratch.Workspace(plan);
  if (!workspace_result.ok()) {
    return false;
  }
  const auto workspace = *workspace_result;
  bool passed = true;
  for (int reuse = 0; reuse < 2; ++reuse) {
    asc::LapackReport report;
    const auto status =
        pack ? asc::Trttp(provider, triangle, input_matrix, triangle_storage,
                          plan, workspace, report)
             : asc::Tpttr(provider, triangle, input_packed, matrix, plan,
                          workspace, report);
    passed = passed && status.ok() && report.called_provider == (n != 0) &&
             report.native_info.has_value() == (n != 0) &&
             (n == 0 || report.native_info.value_or(-79) == 0) &&
             report.outcome == asc::LapackOutcome::kSuccess &&
             report.output_validity == asc::LapackOutputValidity::kComplete &&
             Same(dense, expected_dense) && Same(packed, expected_packed);
    passed = scratch.Guards(workspace) && passed;
  }
  return passed;
}

template <typename T>
int Run(const asc::ReferenceLapackProvider& provider) {
  int failed = 0;
  for (auto n : std::array<std::size_t, 8>{0, 1, 2, 3, 5, 8, 17, 33}) {
    for (auto padding : std::array<std::size_t, 3>{0, 1, 5}) {
      for (auto dl : {kRow, kColumn}) {
        for (auto pl : {kRow, kColumn}) {
          for (auto triangle : {kUpper, kLower}) {
            for (bool pack : {false, true}) {
              for (std::size_t seed = 0; seed < 4; ++seed) {
                failed += !Case<T>(provider, n, padding, dl, pl, triangle, pack,
                                   seed);
              }
            }
          }
        }
      }
    }
  }
  return failed;
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto result =
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial());
  if (!result.ok()) {
    return 2;
  }
  const auto& provider = *result;
  const auto failed = Run<float>(provider) + Run<double>(provider) +
                      Run<std::complex<float>>(provider) +
                      Run<std::complex<double>>(provider);
  std::printf("6144 installed triangular storage cases, failures=%d\n", failed);
  return failed == 0 ? 0 : 1;
}
