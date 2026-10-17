#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <limits>
#include <thread>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/lu.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
using Layout = asc::DenseBlasLayout;
using Op = asc::DenseBlasTranspose;

template <typename T>
T Scalar(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

bool Complete(const asc::LapackReport& report) {
  return !report.called_provider && !report.native_info.has_value() &&
         !report.native_argument.has_value() &&
         report.provider == asc::LapackProviderIdentity{} &&
         report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete;
}

template <typename T>
bool OneSolve(bool cholesky, int iteration) {
  const auto layout = iteration & 1 ? Layout::kRowMajor : Layout::kColumnMajor;
  const auto rhs_layout =
      iteration & 2 ? Layout::kRowMajor : Layout::kColumnMajor;
  constexpr std::array kOperations{Op::kNone, Op::kTranspose,
                                   Op::kConjugateTranspose};
  const auto operation = kOperations[static_cast<std::size_t>(iteration % 3)];
  const auto triangle = iteration & 4 ? asc::DenseBlasTriangle::kUpper
                                      : asc::DenseBlasTriangle::kLower;
  const auto expected = Scalar<T>(1, -1);
  T a = cholesky ? Scalar<T>(4, 53) : Scalar<T>(2, 1);
  const T coefficient =
      cholesky ? T{4}
               : Scalar<T>(2, operation == Op::kConjugateTranspose ? -1 : 1);
  T b = coefficient * expected;
  auto matrix = asc::DenseBlasMatrixView<T>::Create(&a, 1, 1, layout, 1,
                                                    {&a, sizeof(a), kHost});
  auto rhs = asc::DenseBlasMatrixView<T>::Create(&b, 1, 1, rhs_layout, 1,
                                                 {&b, sizeof(b), kHost});
  if (!matrix.ok() || !rhs.ok()) {
    return false;
  }
  const auto cpu = asc::ExecutionContext::Serial();
  asc::LapackReport report;
  if (cholesky) {
    if (!asc::Potrf(cpu, triangle, *matrix, report).ok() || !Complete(report)) {
      return false;
    }
    auto factor =
        asc::LapackCholeskyFactorView<T>::Create(*matrix, triangle, report);
    if (!factor.ok() || !asc::Potrs(cpu, *factor, *rhs, report).ok()) {
      return false;
    }
  } else {
    asc::index_t pivot = 0;
    auto pivots = asc::DenseBlasVectorView<asc::index_t>::Create(
        &pivot, 1, 1, {&pivot, sizeof(pivot), kHost});
    if (!pivots.ok() || !asc::Getrf(cpu, *matrix, *pivots, report).ok() ||
        !Complete(report)) {
      return false;
    }
    auto raw = asc::RawLapackPivotView::Create(
        &pivot, 1, asc::LapackFactorFamily::kLuPartialPivot,
        {&pivot, sizeof(pivot), kHost});
    if (!raw.ok()) {
      return false;
    }
    auto factor = asc::LapackLuFactorView<T>::Create(*matrix, *raw, report);
    if (!factor.ok() ||
        !asc::Getrs(cpu, operation, *factor, *rhs, report).ok()) {
      return false;
    }
  }
  const auto error = std::abs(b - expected);
  return Complete(report) && std::isfinite(error) &&
         error <=
             64 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                 std::abs(expected);
}

template <typename T>
bool OneQr(int iteration) {
  std::array<T, 2> values{T{3}, T{4}};
  T tau{};
  T scratch{};
  const auto layout = iteration & 1 ? Layout::kRowMajor : Layout::kColumnMajor;
  auto matrix = asc::DenseBlasMatrixView<T>::Create(
      values.data(), 2, 1, layout, layout == Layout::kRowMajor ? 1 : 2,
      {values.data(), sizeof(values), kHost});
  auto coefficients = asc::DenseBlasVectorView<T>::Create(
      &tau, 1, 1, {&tau, sizeof(tau), kHost});
  auto workspace = asc::DenseBlasVectorView<T>::Create(
      &scratch, 1, 1, {&scratch, sizeof(scratch), kHost});
  if (!matrix.ok() || !coefficients.ok() || !workspace.ok()) {
    return false;
  }
  asc::LapackReport report;
  if (!asc::Geqrf(asc::ExecutionContext::Serial(), *matrix, *coefficients,
                  *workspace, report)
           .ok()) {
    return false;
  }
  using Real = asc::DenseBlasRealType<T>;
  return Complete(report) && values[0] == T{-5} && values[1] == T{0.5} &&
         std::abs(tau - T{Real{1.6}}) <=
             8 * std::numeric_limits<Real>::epsilon();
}

template <typename T>
void Worker(std::barrier<>& start, bool& passed) {
  start.arrive_and_wait();
  bool result = true;
  for (int iteration = 0; iteration < 1000; ++iteration) {
    result = OneSolve<T>(false, iteration) && result;
    result = OneSolve<T>(true, iteration) && result;
    result = OneQr<T>(iteration) && result;
  }
  passed = result;
}
}  // namespace

int main() {
  std::array<bool, 4> passed{};
  std::barrier start(5);
  constexpr std::array kWorkers{Worker<float>, Worker<double>,
                                Worker<std::complex<float>>,
                                Worker<std::complex<double>>};
  std::array<std::thread, 4> workers;
  std::size_t launched = 0;
  try {
    for (std::size_t i = 0; i < workers.size(); ++i) {
      workers[i] = std::thread([&, i] { kWorkers[i](start, passed[i]); });
      ++launched;
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Native worker launch failed: %s\n", error.what());
  }
  // Release and join every live worker even if a later launch failed.
  for (std::size_t i = launched; i < workers.size(); ++i) {
    start.arrive_and_drop();
  }
  start.arrive_and_wait();
  for (std::size_t i = 0; i < launched; ++i) {
    workers[i].join();
  }
  if (launched != workers.size()) {
    return 1;
  }
  for (bool result : passed) {
    if (!result) {
      return 1;
    }
  }
  std::puts(
      "Four concurrent scalar workers: 4000 fixture groups, 20000 native calls "
      "passed.");
  return 0;
}
