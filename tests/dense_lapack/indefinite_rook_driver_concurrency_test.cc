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
#include "indefinite_rook_driver_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;
struct Mode {
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  asc::DenseBlasLayout rhs_layout;
  bool preferred;
};
template <typename T>
struct Sample {
  Mode mode;
  bool hermitian;
  std::array<T, 5000> a{};
  std::array<T, 500> b{};
  std::array<asc::index_t, 72> p{};
  explicit Sample(Mode selected, bool he) : mode(selected), hermitian(he) {
    Reset(false);
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
  [[nodiscard]] std::size_t AOffset(int i, int j) const {
    return 1U + (mode.layout == base::kColumn
                     ? static_cast<std::size_t>(j) * (mode.n + 2) + i
                     : static_cast<std::size_t>(i) * (mode.n + 2) + j);
  }
  [[nodiscard]] int Ldb() const {
    return mode.rhs_layout == base::kColumn ? mode.n + 2 : 4;
  }
  [[nodiscard]] std::size_t BOffset(int i, int j) const {
    return 1U + (mode.rhs_layout == base::kColumn
                     ? static_cast<std::size_t>(j) * Ldb() + i
                     : static_cast<std::size_t>(i) * Ldb() + j);
  }
  static T Expected(int i, int j) {
    return base::Value<T>((i % 3 - 1) / 4.0L, (j + 1) / 8.0L);
  }
  void Reset(bool singular) {
    a.fill(base::Value<T>(-31, 17));
    b.fill(base::Value<T>(-37, 19));
    p.fill(-41);
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if (mode.triangle == base::kUpper ? i <= j : i >= j) {
          a[AOffset(i, j)] = Coefficient(i, j, singular);
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian && i == j) {
              a[AOffset(i, j)].imag(
                  std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
            }
          }
        }
      }
    }
    for (int j = 0; j < 2; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        base::Wide sum{};
        for (int k = 0; k < mode.n; ++k) {
          sum += base::ToWide(Coefficient(i, k, singular)) *
                 base::ToWide(Expected(k, j));
        }
        b[BOffset(i, j)] = base::Value<T>(sum.real(), sum.imag());
      }
    }
  }
  auto Matrix() {
    return base::Matrix(a, mode.n, mode.n, mode.layout, mode.n + 2);
  }
  auto Rhs(int nrhs) {
    return base::Matrix(b, mode.n, nrhs, mode.rhs_layout, Ldb());
  }
  auto Pivots() { return base::Pivots(p, mode.n); }
  void Check(TestContext& test, const Sample& before, int nrhs, bool singular,
             bool stale, const asc::Status& status,
             const asc::LapackReport& report) const {
    if (stale) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_CHECK(
          test, base::EqualBytes(a.data(), before.a.data(), sizeof(a)));
      ASC_DENSE_TEST_EQ(test, b, before.b);
      ASC_DENSE_TEST_EQ(test, p, before.p);
      return;
    }
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1),
                      singular ? mode.n : 0);
    if (singular) {
      ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kDocumentedPartial);
    } else {
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
    }
    if (singular || nrhs == 0) {
      ASC_DENSE_TEST_EQ(test, b, before.b);
    } else {
      for (int j = 0; j < nrhs; ++j) {
        for (int i = 0; i < mode.n; ++i) {
          const auto actual = base::ToWide(b[BOffset(i, j)]);
          ASC_DENSE_TEST_CHECK(test, std::isfinite(actual.real()) &&
                                         std::isfinite(actual.imag()));
          ASC_DENSE_TEST_CHECK(
              test, std::abs(actual - base::ToWide(Expected(i, j))) <=
                        64 * std::numeric_limits<
                                 asc::DenseBlasRealType<T>>::epsilon());
        }
      }
    }
    for (std::size_t k = 0; k < a.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i = mode.layout == base::kColumn ? relative % (mode.n + 2)
                                                 : relative / (mode.n + 2);
      const int j = mode.layout == base::kColumn ? relative / (mode.n + 2)
                                                 : relative % (mode.n + 2);
      if (k == 0 || i >= mode.n || j >= mode.n ||
          (mode.triangle == base::kUpper ? i > j : i < j)) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&a[k], &before.a[k], sizeof(T)));
      }
    }
    for (std::size_t k = 0; k < b.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i = mode.rhs_layout == base::kColumn ? relative % Ldb()
                                                     : relative / Ldb();
      const int j = mode.rhs_layout == base::kColumn ? relative / Ldb()
                                                     : relative % Ldb();
      if (k == 0 || i >= mode.n || j >= nrhs) {
        ASC_DENSE_TEST_EQ(test, b[k], before.b[k]);
      }
    }
    ASC_DENSE_TEST_EQ(test, p.front(), -41);
    for (std::size_t k = static_cast<std::size_t>(mode.n) + 1; k < p.size();
         ++k) {
      ASC_DENSE_TEST_EQ(test, p[k], -41);
    }
  }
};

template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, Mode mode) {
  Sample<T> prototype(mode, hermitian);
  const auto active = base::Take(asc_rook_driver_test::Query(
      provider, mode.triangle, hermitian, prototype.Matrix(),
      prototype.Pivots(), prototype.Rhs(2)));
  const auto factor_only = base::Take(asc_rook_driver_test::Query(
      provider, mode.triangle, hermitian, prototype.Matrix(),
      prototype.Pivots(), prototype.Rhs(0)));
  const auto old_prototype = prototype;
  std::array<int, kWorkers> results{};
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[worker] = std::thread([&, worker] {
      TestContext local;
      Sample<T> sample(mode, hermitian);
      for (int repeat = 0; repeat < kRepeats; ++repeat) {
        for (const bool ordinary : {false, true}) {
          const bool singular = !ordinary && worker == 1;
          const bool no_rhs = !ordinary && worker == 2;
          const bool stale = !ordinary && worker == 3;
          const int nonempty_rhs = stale ? 1 : 2;
          const int nrhs = no_rhs ? 0 : nonempty_rhs;
          sample.Reset(singular);
          const auto before = sample;
          const auto& plan = no_rhs ? factor_only : active;
          base::Scratch<T> scratch;
          const auto workspace =
              scratch.Workspace(plan, mode.preferred ? -1 : 1);
          asc::LapackReport report;
          barrier.arrive_and_wait();
          const auto status = asc_rook_driver_test::Driver(
              provider, mode.triangle, hermitian, sample.Matrix(),
              sample.Pivots(), sample.Rhs(nrhs), plan, workspace, report);
          barrier.arrive_and_wait();
          sample.Check(local, before, nrhs, singular, stale, status, report);
          scratch.Guards(local, workspace);
          if (stale) {
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
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(prototype.a.data(), old_prototype.a.data(),
                             sizeof(prototype.a)));
  ASC_DENSE_TEST_EQ(test, prototype.b, old_prototype.b);
  ASC_DENSE_TEST_EQ(test, prototype.p, old_prototype.p);
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
        for (const auto rhs_layout : {base::kColumn, base::kRow}) {
          for (const bool preferred : {false, true}) {
            Group<T>(test, provider, hermitian,
                     {n, triangle, layout, rhs_layout, preferred});
            ++groups;
          }
        }
      }
    }
  }
  std::printf(
      "rook driver concurrent groups=%d workers=%d repeats=%d native_calls=%d "
      "structural_rejections=%d\n",
      groups, kWorkers, kRepeats, groups * kRepeats * 7, groups * kRepeats);
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
