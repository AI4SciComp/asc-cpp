#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_indefinite_rook_test;
using support::Take;
using support::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 2;

struct Mode {
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  bool blocked;
  asc::extent_t scalar_entries;
};

template <typename T, bool Hermitian>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  Mode mode;
  Real scale;
  std::array<T, 5000> a{};
  std::array<T, 5000> original{};
  std::array<asc::index_t, 72> pivots{};

  explicit Sample(const Mode& selected, int exponent = 0)
      : mode(selected), scale(std::ldexp(Real{1}, exponent)) {
    a.fill(support::Value<T>(-31, 17));
    pivots.fill(-37);
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        T value = Original(i, j);
        if (!Selected(i, j)) {
          value = support::Value<T>(std::numeric_limits<Real>::quiet_NaN());
        }
        if constexpr (Hermitian) {
          if (i == j) {
            value.imag(std::numeric_limits<Real>::quiet_NaN());
          }
        }
        a[Index(i, j)] = value;
      }
    }
    original = a;
  }
  [[nodiscard]] int Ld() const { return mode.n + 2; }
  [[nodiscard]] bool Selected(int i, int j) const {
    return mode.triangle == support::kUpper ? i <= j : i >= j;
  }
  [[nodiscard]] std::size_t Index(int i, int j) const {
    return 1U + static_cast<std::size_t>(mode.layout == support::kColumn
                                             ? j * Ld() + i
                                             : i * Ld() + j);
  }
  [[nodiscard]] T Original(int i, int j) const {
    if (i == j && i >= 2) {
      return scale *
             support::Value<T>(i % 2 == 0 ? 4 : -4, Hermitian ? 0 : 0.125L);
    }
    if (i + j == 1) {
      T value = scale * support::Value<T>(2, 0.5L);
      if constexpr (Hermitian) {
        if (i == 0) {
          value = std::conj(value);
        }
      }
      return value;
    }
    return T{};
  }
  auto View() { return support::Matrix(a, mode.n, mode.n, mode.layout, Ld()); }
  [[nodiscard]] auto ConstView() const {
    return support::Matrix(a, mode.n, mode.n, mode.layout, Ld());
  }
  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i =
          mode.layout == support::kColumn ? relative % Ld() : relative / Ld();
      const int j =
          mode.layout == support::kColumn ? relative / Ld() : relative % Ld();
      if (k == 0 || i >= mode.n || j >= mode.n || !Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(
            test, support::EqualBytes(&a[k], &original[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -37);
    for (std::size_t k = static_cast<std::size_t>(mode.n) + 1;
         k < pivots.size(); ++k) {
      ASC_DENSE_TEST_EQ(test, pivots[k], -37);
    }
  }
  void CheckFactor(TestContext& test) const {
    // This block diagonal input has one nonsingular 2x2 D block and remaining
    // 1x1 D blocks. U/L are identity; no external factorization is the oracle.
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if (!Selected(i, j)) {
          continue;
        }
        const auto value = support::ToWide(a[Index(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(value.real()) && std::isfinite(value.imag()));
        ASC_DENSE_TEST_EQ(test, a[Index(i, j)], Original(i, j));
      }
      const int paired = -j - 1;
      const int expected = j >= 2 ? j + 1 : paired;
      ASC_DENSE_TEST_EQ(test, pivots[static_cast<std::size_t>(j) + 1],
                        expected);
    }
    Guards(test);
  }
};

template <typename T>
T Expected(int i, int j) {
  return support::Value<T>((i % 5 - 2 + j) / 8.0L, (j + 1) / 16.0L);
}

template <typename T, bool Hermitian>
struct Rhs {
  const Sample<T, Hermitian>& sample;
  asc::DenseBlasLayout layout;
  int ld;
  std::array<T, 500> b{};
  std::array<T, 500> original{};

  Rhs(const Sample<T, Hermitian>& factor, asc::DenseBlasLayout storage)
      : sample(factor),
        layout(storage),
        ld(storage == support::kColumn ? factor.mode.n + 2 : 4) {
    b.fill(support::Value<T>(-41, 19));
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < sample.mode.n; ++i) {
        support::Wide value{};
        for (int k = 0; k < sample.mode.n; ++k) {
          value += support::ToWide(sample.Original(i, k)) *
                   support::ToWide(Expected<T>(k, j));
        }
        b[Index(i, j)] = support::Value<T>(value.real(), value.imag());
      }
    }
    original = b;
  }
  [[nodiscard]] std::size_t Index(int i, int j) const {
    return 1U + static_cast<std::size_t>(
                    layout == support::kColumn ? j * ld + i : i * ld + j);
  }
  auto View() { return support::Matrix(b, sample.mode.n, 2, layout, ld); }
  void Check(TestContext& test) const {
    using Real = asc::DenseBlasRealType<T>;
    const auto epsilon = std::numeric_limits<Real>::epsilon();
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < sample.mode.n; ++i) {
        const auto x = support::ToWide(b[Index(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(x.real()) && std::isfinite(x.imag()));
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(x - support::ToWide(Expected<T>(i, j))) <=
                                 64 * sample.mode.n * epsilon);
        support::Wide product{};
        long double bound = std::abs(support::ToWide(original[Index(i, j)]));
        for (int k = 0; k < sample.mode.n; ++k) {
          const auto a = support::ToWide(sample.Original(i, k));
          const auto value = support::ToWide(b[Index(k, j)]);
          product += a * value;
          bound += std::abs(a) * std::abs(value);
        }
        ASC_DENSE_TEST_CHECK(
            test, std::abs(product - support::ToWide(original[Index(i, j)])) <=
                      32 * sample.mode.n * epsilon * bound);
      }
    }
    for (std::size_t k = 0; k < b.size(); ++k) {
      const int position = static_cast<int>(k) - 1;
      const int i = layout == support::kColumn ? position % ld : position / ld;
      const int j = layout == support::kColumn ? position / ld : position % ld;
      if (k == 0 || i >= sample.mode.n || j >= 2) {
        ASC_DENSE_TEST_EQ(test, b[k], original[k]);
      }
    }
  }
};

std::string_view FactorRoutine(bool hermitian, bool blocked) {
  if (hermitian) {
    return blocked ? "hetrf_rook" : "hetf2_rook";
  }
  return blocked ? "sytrf_rook" : "sytf2_rook";
}

void CheckSuccess(TestContext& test, const asc::LapackReport& report,
                  const asc::ReferenceLapackProvider& provider,
                  std::string_view routine) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()).substr(1),
                    routine);
  ASC_DENSE_TEST_CHECK(test,
                       !report.native_argument && !report.diagnostic_index);
}

template <typename T, bool Hermitian>
auto Query(const asc::ReferenceLapackProvider& provider,
           Sample<T, Hermitian>& sample, bool blocked) {
  return Take(support::QueryFactor(
      provider, sample.mode.triangle, Hermitian, blocked, sample.View(),
      support::Pivots(sample.pivots, sample.mode.n)));
}

template <typename T, bool Hermitian>
auto Factor(const asc::ReferenceLapackProvider& provider,
            Sample<T, Hermitian>& sample, const asc::LapackWorkspacePlan& plan,
            const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  return support::Factor(provider, sample.mode.triangle, Hermitian,
                         sample.mode.blocked, sample.View(),
                         support::Pivots(sample.pivots, sample.mode.n), plan,
                         workspace, report);
}

template <typename T, bool Hermitian>
auto Borrow(const asc::ReferenceLapackProvider& provider,
            const Sample<T, Hermitian>& sample,
            const asc::LapackReport& report) {
  return Take(asc::ReferenceRookFactorView<T>::Create(
      provider, sample.ConstView(), sample.mode.triangle,
      Hermitian ? support::kHermitian : support::kSymmetric,
      support::Raw(sample.pivots, sample.mode.n), report));
}

template <typename T, bool Hermitian>
void Solve(TestContext& test, const asc::ReferenceLapackProvider& provider,
           const Sample<T, Hermitian>& sample,
           const asc::ReferenceRookFactorView<T>& factor,
           asc::DenseBlasLayout layout, std::barrier<>& rendezvous) {
  Rhs<T, Hermitian> rhs(sample, layout);
  const auto saved = sample.a;
  const auto saved_pivots = sample.pivots;
  rendezvous.arrive_and_wait();
  const auto plan =
      Take(support::QuerySolve(provider, Hermitian, factor, rhs.View()));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    rhs.b = rhs.original;
    rendezvous.arrive_and_wait();
    ASC_DENSE_TEST_CHECK(
        test, support::Solve(provider, Hermitian, factor, rhs.View(), plan,
                             workspace, report)
                  .ok());
    CheckSuccess(test, report, provider,
                 Hermitian ? "hetrs_rook" : "sytrs_rook");
    rhs.Check(test);
    scratch.Guards(test, workspace);
    ASC_DENSE_TEST_CHECK(
        test,
        support::EqualBytes(sample.a.data(), saved.data(), sizeof(saved)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, saved_pivots);
  }
}

template <typename T, bool Hermitian>
void FinalOutcome(TestContext& test, int worker,
                  const asc::ReferenceLapackProvider& provider,
                  Sample<T, Hermitian>& sample,
                  const asc::LapackWorkspacePlan& plan,
                  std::barrier<>& rendezvous) {
  for (int j = 0; j < sample.mode.n; ++j) {
    for (int i = 0; i < sample.mode.n; ++i) {
      if (sample.Selected(i, j)) {
        sample.a[sample.Index(i, j)] = i == j ? T{4} : T{};
      }
    }
  }
  const int singular = worker == 1 ? 0 : sample.mode.n - 1;
  if (worker == 1 || worker == 2) {
    sample.a[sample.Index(singular, singular)] = T{};
  }
  const auto saved = sample.a;
  const auto pivots = sample.pivots;
  const auto selected_plan =
      worker == 0 ? Query(provider, sample, !sample.mode.blocked) : plan;
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(
      selected_plan, worker == 0 ? -1 : sample.mode.scalar_entries);
  asc::LapackReport report;
  rendezvous.arrive_and_wait();
  const auto status =
      Factor(provider, sample, selected_plan, workspace, report);
  rendezvous.arrive_and_wait();
  if (worker == 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_CHECK(
        test,
        support::EqualBytes(sample.a.data(), saved.data(), sizeof(saved)));
    ASC_DENSE_TEST_EQ(test, sample.pivots, pivots);
  } else if (worker == 1 || worker == 2) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info, singular + 1);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, singular);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    CheckSuccess(test, report, provider,
                 FactorRoutine(Hermitian, sample.mode.blocked));
  }
  sample.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T, bool Hermitian>
void Worker(TestContext& test, int worker, const Mode& mode,
            const asc::ReferenceLapackProvider& provider,
            const asc::LapackWorkspacePlan& shared_plan,
            const Sample<T, Hermitian>& shared_sample,
            const asc::ReferenceRookFactorView<T>& shared_factor,
            std::barrier<>& rendezvous) {
  Sample<T, Hermitian> local(mode, worker - 2);
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(shared_plan, mode.scalar_entries);
  asc::LapackReport report;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    local.a = local.original;
    rendezvous.arrive_and_wait();
    const auto plan = Query(provider, local, mode.blocked);
    ASC_DENSE_TEST_EQ(test, plan.identity, shared_plan.identity);
    rendezvous.arrive_and_wait();
    ASC_DENSE_TEST_CHECK(
        test, Factor(provider, local, shared_plan, workspace, report).ok());
    CheckSuccess(test, report, provider,
                 FactorRoutine(Hermitian, mode.blocked));
    local.CheckFactor(test);
    scratch.Guards(test, workspace);
  }
  const auto local_factor = Borrow(provider, local, report);
  for (const auto layout : {support::kColumn, support::kRow}) {
    Solve(test, provider, local, local_factor, layout, rendezvous);
    // All workers reuse this same nominal view and underlying immutable data.
    // RHS, workspace, plans and reports remain private to each worker.
    Solve(test, provider, shared_sample, shared_factor, layout, rendezvous);
  }
  FinalOutcome(test, worker, provider, local, shared_plan, rendezvous);
}

template <typename T, bool Hermitian>
void Exercise(TestContext& test, const asc::ReferenceLapackProvider& provider,
              const Mode& mode) {
  Sample<T, Hermitian> shared_sample(mode);
  const auto plan = Query(provider, shared_sample, mode.blocked);
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan, mode.scalar_entries);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, Factor(provider, shared_sample, plan, workspace, report).ok());
  shared_sample.CheckFactor(test);
  const auto shared_factor = Borrow(provider, shared_sample, report);
  const auto saved = shared_sample.a;
  const auto pivots = shared_sample.pivots;
  std::barrier rendezvous(kWorkers);
  std::array<std::thread, kWorkers> workers;
  std::array<int, kWorkers> results{};
  for (int worker = 0; worker < kWorkers; ++worker) {
    workers[worker] = std::thread([&, worker] {
      TestContext local;
      Worker(local, worker, mode, provider, plan, shared_sample, shared_factor,
             rendezvous);
      results[worker] = local.Finish();
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  ASC_DENSE_TEST_CHECK(test, support::EqualBytes(shared_sample.a.data(),
                                                 saved.data(), sizeof(saved)));
  ASC_DENSE_TEST_EQ(test, shared_sample.pivots, pivots);
  scratch.Guards(test, workspace);
}

template <typename T, bool Hermitian = false>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const int n : {7, 67}) {
    for (const auto triangle : {support::kUpper, support::kLower}) {
      for (const auto layout : {support::kColumn, support::kRow}) {
        for (const bool blocked : {false, true}) {
          const asc::extent_t threshold = Hermitian ? 2 : 8;
          const std::array<asc::extent_t, 4> capacities{
              blocked ? 1 : 0, -1, threshold * n, (threshold - 1) * n};
          for (std::size_t choice = 0; choice < capacities.size(); ++choice) {
            if ((!blocked && choice > 0) || (n < 65 && choice > 1)) {
              continue;
            }
            Exercise<T, Hermitian>(
                test, provider,
                {n, triangle, layout, blocked, capacities[choice]});
          }
        }
      }
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
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else if (scalar == "ch") {
    Run<std::complex<float>, true>(test, provider);
  } else if (scalar == "zh") {
    Run<std::complex<double>, true>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
