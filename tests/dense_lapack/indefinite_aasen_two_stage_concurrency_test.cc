#include <algorithm>
#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace aa = asc_aasen_two_stage_test;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;
struct Mode {
  int n;
  asc::DenseBlasTriangle tri;
  asc::DenseBlasLayout layout;
  bool band_preferred;
  bool work_preferred;
};
template <typename T>
struct Operands {
  Mode mode;
  aa::Sample<T> sample;
  std::vector<T> a;
  std::vector<T> tb;
  std::vector<asc::index_t> p;
  std::vector<asc::index_t> q;
  Operands(Mode selected, bool he, int worker)
      : mode(selected),
        sample(mode.n, he, mode.tri == base::kUpper, worker == 1),
        a(sample.a),
        tb(static_cast<std::size_t>(Ltb() + 2), T{-73}),
        p(static_cast<std::size_t>(mode.n + 2), -71),
        q(p) {
    const auto scale =
        std::ldexp(asc::DenseBlasRealType<T>{1}, (worker - 2) * 10);
    for (auto& value : sample.full) {
      value *= scale;
    }
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if (sample.Selected(i, j)) {
          const auto value = sample.full[i * mode.n + j];
          a[Offset(i, j)] = base::Value<T>(value.real(), value.imag());
          if constexpr (asc::DenseBlasComplex<T>) {
            if (he && i == j) {
              a[Offset(i, j)].imag(
                  std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
            }
          }
        }
      }
    }
  }
  [[nodiscard]] int Ltb() const {
    return mode.n * (mode.band_preferred ? 577 : 4);
  }
  [[nodiscard]] int Entries() const {
    return mode.n * (mode.work_preferred ? 192 : 1);
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + static_cast<std::size_t>(mode.layout == base::kColumn
                                             ? j * sample.lda + i
                                             : i * sample.lda + j);
  }
  auto Matrix() {
    return base::Take(asc::DenseBlasMatrixView<T>::Create(
        a.data() + 1, mode.n, mode.n, mode.layout, sample.lda,
        {a.data(), a.size() * sizeof(T), base::kHost}));
  }
  void Guards(TestContext& test, const Operands& before) const {
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < sample.lda; ++i) {
        if (i >= mode.n ||
            !(mode.layout == base::kColumn ? sample.Selected(i, j)
                                           : sample.Selected(j, i))) {
          const int at = 1 + j * sample.lda + i;
          ASC_DENSE_TEST_CHECK(
              test, base::EqualBytes(&a[at], &before.a[at], sizeof(T)));
        }
      }
    }
    ASC_DENSE_TEST_EQ(test, a.front(), before.a.front());
    ASC_DENSE_TEST_EQ(test, a.back(), before.a.back());
    ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
    ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
    ASC_DENSE_TEST_EQ(test, p.front(), -71);
    ASC_DENSE_TEST_EQ(test, p.back(), -71);
    ASC_DENSE_TEST_EQ(test, q.front(), -71);
    ASC_DENSE_TEST_EQ(test, q.back(), -71);
  }
  void Reconstruction(TestContext& test) {
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if (sample.Selected(i, j)) {
          sample.a[1 + j * sample.lda + i] = a[Offset(i, j)];
        }
      }
    }
    const int nb =
        std::min({192, (Ltb() / mode.n - 1) / 3, Entries() / mode.n});
    aa::Reconstruction(test, mode.n, sample.hermitian, sample.upper,
                       sample.a.data() + 1, sample.lda, tb.data() + 1,
                       Ltb() / mode.n, nb, p.data() + 1, q.data() + 1,
                       sample.full);
  }
};
template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    Operands<T>& a, bool stale,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  auto tri = a.mode.tri;
  if (stale) {
    tri = tri == base::kUpper ? base::kLower : base::kUpper;
  }
  return aa::Factor(provider, tri, a.sample.hermitian, a.Matrix(),
                    aa::Vector(a.tb, a.Ltb()), aa::Vector(a.p, a.mode.n),
                    aa::Vector(a.q, a.mode.n), plan, workspace, report);
}
void Report(TestContext& test, const asc::Status& status,
            const asc::LapackReport& report, bool singular, int n) {
  ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), singular ? n : 0);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    singular ? asc::LapackOutputValidity::kDocumentedPartial
                             : asc::LapackOutputValidity::kComplete);
}
template <typename T>
int Worker(const asc::ReferenceLapackProvider& provider, int worker,
           const asc::LapackWorkspacePlan& plan,
           const std::vector<Operands<T>>& original,
           const std::vector<Operands<T>>& expected, std::barrier<>& barrier) {
  const auto mode = original[worker].mode;
  TestContext local;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    auto sample = original[worker];
    const bool stale = worker == 3 && repeat % 2 == 1;
    aa::Scratch<T> scratch(plan, sample.Entries());
    const auto scalar_before = scratch.scalar;
    const auto packed_before = scratch.packed;
    const auto integers_before = scratch.integers;
    asc::LapackReport report;
    barrier.arrive_and_wait();
    const auto status =
        Execute(provider, sample, stale, plan, scratch.workspace, report);
    barrier.arrive_and_wait();
    const auto& reference = stale ? original[worker] : expected[worker];
    ASC_DENSE_TEST_CHECK(local,
                         base::EqualBytes(sample.a.data(), reference.a.data(),
                                          sample.a.size() * sizeof(T)));
    ASC_DENSE_TEST_CHECK(local,
                         base::EqualBytes(sample.tb.data(), reference.tb.data(),
                                          sample.tb.size() * sizeof(T)));
    ASC_DENSE_TEST_EQ(local, sample.p, reference.p);
    ASC_DENSE_TEST_EQ(local, sample.q, reference.q);
    if (stale) {
      ASC_DENSE_TEST_EQ(local, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(
          local, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(local, scratch.scalar, scalar_before);
      ASC_DENSE_TEST_EQ(local, scratch.packed, packed_before);
      ASC_DENSE_TEST_EQ(local, scratch.integers, integers_before);
    } else {
      Report(local, status, report, worker == 1, mode.n);
    }
    sample.Guards(local, original[worker]);
    scratch.Guards(local);
  }
  return local.Finish();
}
template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool he, Mode mode) {
  Operands<T> prototype(mode, he, 0);
  const auto plan = base::Take(aa::Query(
      provider, mode.tri, he, prototype.Matrix(),
      aa::Vector(prototype.tb, prototype.Ltb()),
      aa::Vector(prototype.p, mode.n), aa::Vector(prototype.q, mode.n)));
  std::vector<Operands<T>> original;
  std::vector<Operands<T>> expected;
  for (int worker = 0; worker < kWorkers; ++worker) {
    original.emplace_back(mode, he, worker);
    expected.push_back(original.back());
    auto& baseline = expected.back();
    aa::Scratch<T> scratch(plan, baseline.Entries());
    asc::LapackReport report;
    const auto status =
        Execute(provider, baseline, false, plan, scratch.workspace, report);
    Report(test, status, report, worker == 1, mode.n);
    if (status.ok() || status.code() == asc::ErrorCode::kNumerical) {
      baseline.Reconstruction(test);
    }
    baseline.Guards(test, original.back());
    scratch.Guards(test);
  }
  std::array<int, kWorkers> results{};
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[worker] = std::thread([&, worker] {
      results[worker] =
          Worker(provider, worker, plan, original, expected, barrier);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
}
template <typename T>
int Run(bool he) {
  TestContext test;
  int groups = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const int n : {7, 67}) {
    for (const auto tri : {base::kUpper, base::kLower}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        for (const bool band : {false, true}) {
          for (const bool work : {false, true}) {
            Group<T>(test, provider, he, {n, tri, layout, band, work});
            ++groups;
          }
        }
      }
    }
  }
  std::printf(
      "Two-stage Aasen concurrent groups=%d workers=%d repeats=%d "
      "serial_baselines=%d native_parallel_calls=%d structural_rejections=%d\n",
      groups, kWorkers, kRepeats, groups * 4, groups * 28, groups * 4);
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
