#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_inverse_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_test;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;
struct Mode {
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
};
template <typename T>
struct Sample {
  Mode mode;
  bool hermitian;
  std::array<T, 5000> a{};
  std::array<asc::index_t, 72> p{};
  Sample(Mode selected, bool he, bool singular)
      : mode(selected), hermitian(he) {
    a.fill(base::Value<T>(-601, 17));
    p.fill(-607);
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if (Selected(i, j)) {
          a[Offset(i, j)] = Coefficient(i, j, singular);
        }
      }
    }
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return mode.triangle == base::kUpper ? i <= j : i >= j;
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + static_cast<std::size_t>(mode.layout == base::kColumn
                                             ? j * (mode.n + 2) + i
                                             : i * (mode.n + 2) + j);
  }
  [[nodiscard]] T Coefficient(int i, int j, bool singular) const {
    T value{};
    if (i + j == 1) {
      value = base::Value<T>(3, hermitian && i == 0 ? -4 : 4);
    } else if (i == j && i >= 2 && (!singular || i != mode.n - 1)) {
      value = base::Value<T>(i % 2 == 0 ? 8 : -8, hermitian ? 0 : 6);
    }
    return value *
           std::ldexp(asc::DenseBlasRealType<T>{1}, mode.n == 7 ? -20 : 20);
  }
  auto Matrix(int n) { return base::Matrix(a, n, n, mode.layout, mode.n + 2); }
  void Factor(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool singular) {
    auto matrix = Matrix(mode.n);
    auto pivots = base::Pivots(p, mode.n);
    const auto plan = base::Take(base::QueryFactor(
        provider, mode.triangle, hermitian, true, matrix, pivots));
    base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status = base::Factor(provider, mode.triangle, hermitian, true,
                                     matrix, pivots, plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
    scratch.Guards(test, workspace);
  }
  void Numerical(TestContext& test) const {
    const long double epsilon =
        std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if (!Selected(i, j)) {
          continue;
        }
        base::Wide expected{};
        if (i + j == 1) {
          expected = base::Wide{1} / base::ToWide(Coefficient(j, i, false));
        } else if (i == j && i >= 2) {
          expected = base::Wide{1} / base::ToWide(Coefficient(i, i, false));
        }
        const auto value = base::ToWide(a[Offset(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(value.real()) && std::isfinite(value.imag()));
        ASC_DENSE_TEST_CHECK(test, std::abs(value - expected) <=
                                       64 * epsilon * std::abs(expected));
      }
    }
  }
  void Check(TestContext& test, const Sample& before, bool singular, bool empty,
             bool stale, const asc::Status& status,
             const asc::LapackReport& report) const {
    if (stale) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
    } else if (empty) {
      ASC_DENSE_TEST_CHECK(test, status.ok() && !report.called_provider &&
                                     !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
    } else if (singular) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), mode.n);
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    } else {
      ASC_DENSE_TEST_CHECK(test, status.ok() && report.called_provider &&
                                     report.native_info == 0);
      Numerical(test);
    }
    if (stale || empty || singular) {
      ASC_DENSE_TEST_CHECK(
          test, base::EqualBytes(a.data(), before.a.data(), sizeof(a)));
    }
    for (std::size_t k = 0; k < a.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i = mode.layout == base::kColumn ? relative % (mode.n + 2)
                                                 : relative / (mode.n + 2);
      const int j = mode.layout == base::kColumn ? relative / (mode.n + 2)
                                                 : relative % (mode.n + 2);
      if (k == 0 || i >= mode.n || j >= mode.n || !Selected(i, j)) {
        ASC_DENSE_TEST_EQ(test, a[k], before.a[k]);
      }
    }
    ASC_DENSE_TEST_EQ(test, p, before.p);
  }
};

template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, Mode mode) {
  Sample<T> ordinary(mode, hermitian, false);
  Sample<T> singular(mode, hermitian, true);
  ordinary.Factor(test, provider, false);
  singular.Factor(test, provider, true);
  const auto original_good = ordinary;
  const auto original_bad = singular;
  const auto active = base::Take(asc_inverse_test::Query(
      provider, mode.triangle, hermitian, ordinary.Matrix(mode.n),
      base::Raw(ordinary.p, mode.n)));
  const auto empty_plan = base::Take(
      asc_inverse_test::Query(provider, mode.triangle, hermitian,
                              ordinary.Matrix(0), base::Raw(ordinary.p, 0)));
  std::array<int, kWorkers> results{};
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[worker] = std::thread([&, worker] {
      TestContext local;
      for (int repeat = 0; repeat < kRepeats; ++repeat) {
        for (const bool all_normal : {false, true}) {
          const bool bad = !all_normal && worker == 1;
          const bool empty = !all_normal && worker == 2;
          const bool stale = !all_normal && worker == 3;
          const auto& shared = bad ? singular : ordinary;
          Sample<T> sample = shared;
          const auto before = sample;
          const auto& plan = empty ? empty_plan : active;
          const auto raw = base::Raw(shared.p, empty ? 0 : mode.n);
          auto selected = mode.triangle;
          if (stale) {
            selected =
                mode.triangle == base::kUpper ? base::kLower : base::kUpper;
          }
          base::Scratch<T> scratch;
          const auto workspace = scratch.Workspace(plan);
          asc::LapackReport report;
          barrier.arrive_and_wait();
          const auto status = asc_inverse_test::Inverse(
              provider, selected, hermitian, sample.Matrix(empty ? 0 : mode.n),
              raw, plan, workspace, report);
          barrier.arrive_and_wait();
          sample.Check(local, before, bad, empty, stale, status, report);
          scratch.Guards(local, workspace);
          if (empty || stale) {
            const base::Scratch<T> untouched;
            ASC_DENSE_TEST_EQ(local, scratch.scalar, untouched.scalar);
            ASC_DENSE_TEST_EQ(local, scratch.packed, untouched.packed);
            ASC_DENSE_TEST_EQ(local, scratch.pivot, untouched.pivot);
          }
        }
      }
      results[worker] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const int result : results) {
    ASC_DENSE_TEST_EQ(test, result, 0);
  }
  ASC_DENSE_TEST_EQ(test, ordinary.a, original_good.a);
  ASC_DENSE_TEST_EQ(test, singular.a, original_bad.a);
  ASC_DENSE_TEST_EQ(test, ordinary.p, original_good.p);
  ASC_DENSE_TEST_EQ(test, singular.p, original_bad.p);
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (const int n : {7, 67}) {
    for (const auto triangle : {base::kUpper, base::kLower}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        Group<T>(test, provider, hermitian, {n, triangle, layout});
        ++groups;
      }
    }
  }
  std::printf(
      "classic inverse concurrent groups=%d workers=%d repeats=%d "
      "native_calls=%d "
      "noncalls=%d structural_rejections=%d\n",
      groups, kWorkers, kRepeats, groups * kRepeats * 6, groups * kRepeats,
      groups * kRepeats);
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
