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
#include "asc/dense/providers/lapack_matrix_permutation.h"
#include "normal_return_guard.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kForward = asc::LapackPermutationDirection::kForward;
constexpr auto kBackward = asc::LapackPermutationDirection::kBackward;
template <typename T>
bool Same(const T& a, const T& b) {
  const auto first = std::as_bytes(std::span(&a, 1));
  const auto second = std::as_bytes(std::span(&b, 1));
  return std::equal(first.begin(), first.end(), second.begin());
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
  std::array<T, 1400> staging;
  alignas(std::max_align_t) std::array<std::byte, 33 * 8 + 32> integer;
  static constexpr std::size_t kScratch =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
  static constexpr std::size_t kInteger =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
  Scratch() {
    staging.fill(T{-109});
    integer.fill(std::byte{0x5a});
  }
  asc::Result<asc::LapackWorkspace> Workspace(
      const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[kScratch].minimum_entries;
    const auto& indices = plan.regions[kInteger];
    if (count < 0 || count > 1398 || indices.minimum_entries < 0 ||
        indices.minimum_entries > 33 || indices.entry_bytes > 8 ||
        indices.entry_bytes == 0 || indices.alignment == 0 ||
        16 % indices.alignment != 0) {
      return asc::Status(asc::ErrorCode::kShape);
    }
    if (count > 0) {
      workspace.regions[kScratch] = {
          staging.data() + 1, static_cast<std::size_t>(count) * sizeof(T),
          kHost};
    }
    if (indices.minimum_entries > 0) {
      workspace.regions[kInteger] = {
          integer.data() + 16,
          static_cast<std::size_t>(indices.minimum_entries) *
              indices.entry_bytes,
          kHost};
    }
    return workspace;
  }
  [[nodiscard]] bool Guards(const asc::LapackWorkspace& workspace) const {
    bool passed = true;
    const auto count = workspace.regions[kScratch].size() / sizeof(T);
    for (std::size_t i = 0; i < staging.size(); ++i) {
      if (i == 0 || i > count) {
        passed = passed && staging[i] == T{-109};
      }
    }
    const auto bytes = workspace.regions[kInteger].size();
    for (std::size_t i = 0; i < integer.size(); ++i) {
      if (i < 16 || i >= 16 + bytes) {
        passed = passed && integer[i] == std::byte{0x5a};
      }
    }
    return passed;
  }
};
template <typename T>
struct Problem {
  std::array<T, 1400> dense;
  std::array<asc::index_t, 35> indices;
  asc::extent_t m, n, leading;
  asc::DenseBlasLayout layout;
  asc::LapackPermutationDirection direction;
  bool rows;
  Problem(asc::extent_t d, asc::extent_t other, asc::extent_t padding,
          asc::DenseBlasLayout storage, bool row_permutation,
          asc::LapackPermutationDirection order, std::size_t seed)
      : m(row_permutation ? d : other),
        n(row_permutation ? other : d),
        leading(std::max(asc::extent_t{1}, storage == kColumn ? m : n) +
                padding),
        layout(storage),
        direction(order),
        rows(row_permutation) {
    dense.fill(T{-97});
    indices.fill(-101);
    for (asc::extent_t i = 0; i < d; ++i) {
      indices[static_cast<std::size_t>(i + 1)] = i + 1;
    }
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < m; ++i) {
        dense[Index(i, j)] = Fixture<T>(static_cast<std::size_t>(i),
                                        static_cast<std::size_t>(j), seed);
      }
    }
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * leading + i : i * leading + j));
  }
  [[nodiscard]] auto Expected() const {
    auto expected = dense;
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < m; ++i) {
        const auto pi = rows ? indices[static_cast<std::size_t>(i + 1)] - 1 : i;
        const auto pj = rows ? j : indices[static_cast<std::size_t>(j + 1)] - 1;
        if (direction == kForward) {
          expected[Index(i, j)] = dense[Index(pi, pj)];
        } else {
          expected[Index(pi, pj)] = dense[Index(i, j)];
        }
      }
    }
    return expected;
  }
};
template <typename T>
bool Case(const asc::ReferenceLapackProvider& provider, Problem<T>& problem) {
  const auto before = problem.dense;
  const auto indices_before = problem.indices;
  const auto expected = problem.Expected();
  const auto matrix_result = asc::DenseBlasMatrixView<T>::Create(
      problem.dense.data() + 1, problem.m, problem.n, problem.layout,
      problem.leading, {problem.dense.data(), sizeof(problem.dense), kHost});
  const auto indices_result =
      asc::DenseBlasVectorView<const asc::index_t>::Create(
          problem.indices.data() + 1, problem.rows ? problem.m : problem.n, 1,
          {problem.indices.data(), sizeof(problem.indices), kHost});
  if (!matrix_result.ok() || !indices_result.ok()) {
    return false;
  }
  const auto matrix = *matrix_result;
  const auto indices = *indices_result;
  const auto plan_result =
      problem.rows ? asc::QueryLapmrWorkspace(provider, problem.direction,
                                              matrix, indices)
                   : asc::QueryLapmtWorkspace(provider, problem.direction,
                                              matrix, indices);
  if (!plan_result.ok()) {
    return false;
  }
  const auto& plan = *plan_result;
  Scratch<T> scratch;
  const auto workspace_result = scratch.Workspace(plan);
  if (!workspace_result.ok()) {
    return false;
  }
  const auto workspace = *workspace_result;
  bool passed = true;
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem.dense = before;
    asc::LapackReport report;
    const auto status = problem.rows
                            ? asc::Lapmr(provider, problem.direction, matrix,
                                         indices, plan, workspace, report)
                            : asc::Lapmt(provider, problem.direction, matrix,
                                         indices, plan, workspace, report);
    passed = passed && status.ok() &&
             report.called_provider == (problem.m > 0 && problem.n > 0) &&
             !report.native_info.has_value() &&
             !report.native_argument.has_value() &&
             report.outcome == asc::LapackOutcome::kSuccess &&
             report.output_validity == asc::LapackOutputValidity::kComplete &&
             Same(problem.dense, expected) && problem.indices == indices_before;
    passed = scratch.Guards(workspace) && passed;
  }
  return passed;
}
template <typename Function>
void Permutations(asc::extent_t d, Function function) {
  std::array<asc::index_t, 33> k{};
  for (asc::extent_t i = 0; i < d; ++i) {
    k[static_cast<std::size_t>(i)] = i + 1;
  }
  std::size_t variant = 0;
  do {
    function(k);
    ++variant;
    if (d <= 5) {
      if (!std::next_permutation(k.begin(), k.begin() + d)) {
        break;
      }
    } else {
      if (variant == 4) {
        break;
      }
      if (variant == 1) {
        std::reverse(k.begin(), k.begin() + d);
      }
      if (variant == 2) {
        for (asc::extent_t i = 0; i < d; ++i) {
          k[static_cast<std::size_t>(i)] = (i + 1) % d + 1;
        }
      }
      if (variant == 3) {
        for (asc::extent_t i = 0; i < d; ++i) {
          k[static_cast<std::size_t>(i)] = i + 1;
        }
        for (asc::extent_t i = 0; i + 2 < d; i += 3) {
          std::rotate(k.begin() + i, k.begin() + i + 1, k.begin() + i + 3);
        }
      }
    }
  } while (true);
}
template <typename T>
int Run(const asc::ReferenceLapackProvider& provider) {
  int failed = 0;
  for (asc::extent_t d : {0, 1, 2, 3, 4, 5, 8, 17, 33}) {
    Permutations(d, [&](const auto& k) {
      for (asc::extent_t other : {0, 1, 3, 7}) {
        for (asc::extent_t padding : {0, 2}) {
          for (auto layout : {kRow, kColumn}) {
            for (bool rows : {false, true}) {
              for (auto direction : {kForward, kBackward}) {
                for (std::size_t seed :
                     std::array<std::size_t, 4>{0, 3, 6, 9}) {
                  Problem<T> problem(d, other, padding, layout, rows, direction,
                                     seed);
                  std::copy_n(k.begin(), d, problem.indices.begin() + 1);
                  failed += !Case(provider, problem);
                }
              }
            }
          }
        }
      }
    });
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
  std::printf("169984 installed permutation cases, failures=%d\n", failed);
  return failed == 0 ? 0 : 1;
}
