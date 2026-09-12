#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_inverse_test_support.h"
#include "indefinite_rk_test_support.h"
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
  bool blocked;
};
template <typename T, std::size_t Size>
class ReadOnlyArray {
 public:
  ReadOnlyArray(TestContext& test, const std::array<T, Size>& values)
      : data_(values.data()) {
#if defined(__linux__)
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test, page > 0);
    if (page <= 0) {
      valid_ = false;
      return;
    }
    bytes_ = ((sizeof(values) + static_cast<std::size_t>(page) - 1) /
              static_cast<std::size_t>(page)) *
             static_cast<std::size_t>(page);
    memory_ = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASC_DENSE_TEST_CHECK(test, memory_ != MAP_FAILED);
    if (memory_ == MAP_FAILED) {
      memory_ = nullptr;
      valid_ = false;
      return;
    }
    std::memcpy(memory_, values.data(), sizeof(values));
    const auto result = mprotect(memory_, bytes_, PROT_READ);
    ASC_DENSE_TEST_EQ(test, result, 0);
    valid_ = result == 0;
    data_ = static_cast<const T*>(memory_);
#else
    (void)test;
#endif
  }
  ReadOnlyArray(const ReadOnlyArray&) = delete;
  ReadOnlyArray& operator=(const ReadOnlyArray&) = delete;
  ReadOnlyArray(ReadOnlyArray&&) = delete;
  ReadOnlyArray& operator=(ReadOnlyArray&&) = delete;
  ~ReadOnlyArray() {
#if defined(__linux__)
    if (memory_ != nullptr) {
      munmap(memory_, bytes_);
    }
#endif
  }
  [[nodiscard]] bool valid() const { return valid_; }
  [[nodiscard]] const T* data() const { return data_; }
  void Check(TestContext& test, const std::array<T, Size>& original) const {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(data_, original.data(), sizeof(original)));
  }

 private:
  const T* data_;
  bool valid_ = true;
#if defined(__linux__)
  void* memory_ = nullptr;
  std::size_t bytes_ = 0;
#endif
};
template <typename T>
auto Extra(const ReadOnlyArray<T, 72>& e, asc::extent_t n) {
  return base::Take(asc::DenseBlasVectorView<const T>::Create(
      e.data() + 1, n, 1, {e.data(), 72 * sizeof(T), base::kHost}));
}
auto Pivots(const ReadOnlyArray<asc::index_t, 72>& p, asc::extent_t n) {
  return base::Take(asc::RawLapackPivotView::Create(
      p.data() + 1, n, asc::LapackFactorFamily::kRook,
      {p.data(), 72 * sizeof(asc::index_t), base::kHost}));
}
template <typename T>
struct Sample {
  Mode mode;
  bool hermitian;
  std::array<T, 5000> a{};
  std::array<asc::index_t, 72> p{};
  std::array<T, 72> e{};
  Sample(Mode selected, bool he, bool singular)
      : mode(selected), hermitian(he) {
    a.fill(base::Value<T>(-601, 17));
    p.fill(-607);
    e.fill(base::Value<T>(-609, 19));
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
    auto extra = asc_rk_test::OffDiagonal(e, mode.n);
    const auto plan = base::Take(
        asc_rk_test::QueryFactor(provider, mode.triangle, hermitian,
                                 mode.blocked, matrix, extra, pivots));
    asc_rk_inverse_test::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status =
        asc_rk_test::Factor(provider, mode.triangle, hermitian, mode.blocked,
                            matrix, extra, pivots, plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
    for (int i = 0; i < mode.n;) {
      const bool pair = p[static_cast<std::size_t>(i) + 1] < 0;
      const int ignored = pair && mode.triangle == base::kLower ? i + 1 : i;
      e[static_cast<std::size_t>(ignored) + 1] = base::Value<T>(
          std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN(), 19);
      i += pair ? 2 : 1;
    }
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
    ASC_DENSE_TEST_EQ(test, report.factor_family,
                      asc::LapackFactorFamily::kRook);
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
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(e.data(), before.e.data(), sizeof(e)));
  }
};

template <typename T>
Sample<T> SerialSingular(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         const Sample<T>& singular,
                         const ReadOnlyArray<T, 72>& bad_e,
                         const ReadOnlyArray<asc::index_t, 72>& bad_p,
                         const asc::LapackWorkspacePlan& active) {
  const auto mode = singular.mode;
  const bool hermitian = singular.hermitian;
  Sample<T> serial_partial = singular;
  asc_rk_inverse_test::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(active);
  asc::LapackReport report;
  const auto status = asc_rk_inverse_test::Inverse(
      provider, mode.triangle, hermitian, serial_partial.Matrix(mode.n),
      Extra(bad_e, mode.n), Pivots(bad_p, mode.n), active, workspace, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), mode.n);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  scratch.Guards(test, workspace);
  return serial_partial;
}

template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, Mode mode) {
  Sample<T> ordinary(mode, hermitian, false);
  Sample<T> singular(mode, hermitian, true);
  ordinary.Factor(test, provider, false);
  singular.Factor(test, provider, true);
  const ReadOnlyArray<T, 72> good_e(test, ordinary.e);
  const ReadOnlyArray<T, 72> bad_e(test, singular.e);
  const ReadOnlyArray<asc::index_t, 72> good_p(test, ordinary.p);
  const ReadOnlyArray<asc::index_t, 72> bad_p(test, singular.p);
  if (!good_e.valid() || !bad_e.valid() || !good_p.valid() || !bad_p.valid()) {
    return;
  }
  const auto original_good = ordinary;
  const auto original_bad = singular;
  const auto active = base::Take(asc_rk_inverse_test::Query(
      provider, mode.triangle, hermitian, ordinary.Matrix(mode.n),
      Extra(good_e, mode.n), Pivots(good_p, mode.n)));
  const auto empty_plan = base::Take(asc_rk_inverse_test::Query(
      provider, mode.triangle, hermitian, ordinary.Matrix(0), Extra(good_e, 0),
      Pivots(good_p, 0)));
  const auto serial_partial =
      SerialSingular(test, provider, singular, bad_e, bad_p, active);
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
          const auto raw = Pivots(bad ? bad_p : good_p, empty ? 0 : mode.n);
          const auto extra = Extra(bad ? bad_e : good_e, empty ? 0 : mode.n);
          auto selected = mode.triangle;
          if (stale) {
            selected =
                mode.triangle == base::kUpper ? base::kLower : base::kUpper;
          }
          asc_rk_inverse_test::Scratch<T> scratch;
          const auto workspace = scratch.Workspace(plan);
          asc::LapackReport report;
          barrier.arrive_and_wait();
          const auto status = asc_rk_inverse_test::Inverse(
              provider, selected, hermitian, sample.Matrix(empty ? 0 : mode.n),
              extra, raw, plan, workspace, report);
          barrier.arrive_and_wait();
          sample.Check(local, bad ? serial_partial : before, bad, empty, stale,
                       status, report);
          scratch.Guards(local, workspace);
          if (empty || stale) {
            const asc_rk_inverse_test::Scratch<T> untouched;
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
  good_e.Check(test, ordinary.e);
  bad_e.Check(test, singular.e);
  good_p.Check(test, ordinary.p);
  bad_p.Check(test, singular.p);
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
        for (const bool blocked : {false, true}) {
          Group<T>(test, provider, hermitian, {n, triangle, layout, blocked});
          ++groups;
        }
      }
    }
  }
  std::printf(
      "RK inverse concurrent groups=%d workers=%d repeats=%d "
      "worker_native_calls=%d "
      "noncalls=%d structural_rejections=%d\n",
      groups, kWorkers, kRepeats, groups * kRepeats * 6, groups * kRepeats,
      groups * kRepeats);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3 || !asc_rk_inverse_test::Select(argv[2])) {
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
