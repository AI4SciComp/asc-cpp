#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <utility>

#include "allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr std::array kLayouts{asc::DenseBlasLayout::kRowMajor,
                              asc::DenseBlasLayout::kColumnMajor};

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::cerr << result.status().ToString() << '\n';
    std::abort();
  }
  return std::move(*result);
}

asc::ExecutionContext Cpu() { return asc::ExecutionContext::Serial(); }

template <typename T>
T Scalar(int real, int imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
Wide Widen(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<long double>(value.real()),
            static_cast<long double>(value.imag())};
  } else {
    return {static_cast<long double>(value), 0};
  }
}

template <typename T>
class Matrix {
 public:
  Matrix(asc::extent_t rows, asc::extent_t columns, asc::DenseBlasLayout layout)
      : rows_(rows),
        columns_(columns),
        layout_(layout),
        lda_((layout == asc::DenseBlasLayout::kRowMajor ? columns : rows) + 2) {
    storage_.fill(Scalar<T>(-313, 97));
    for (asc::index_t r = 0; r < rows_; ++r) {
      for (asc::index_t c = 0; c < columns_; ++c) {
        (*this)(r, c) = T{0};
      }
    }
  }
  T& operator()(asc::index_t row, asc::index_t column) {
    return storage_[Offset(row, column)];
  }
  const T& operator()(asc::index_t row, asc::index_t column) const {
    return storage_[Offset(row, column)];
  }
  asc::DenseBlasMatrixView<T> view(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        storage_.data() + 3, rows_, columns_, layout_, lda_,
        {storage_.data(), sizeof(storage_), space}));
  }
  [[nodiscard]] asc::DenseBlasMatrixView<const T> const_view() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        storage_.data() + 3, rows_, columns_, layout_, lda_,
        {storage_.data(), sizeof(storage_), kHost}));
  }
  [[nodiscard]] asc::extent_t rows() const { return rows_; }
  [[nodiscard]] asc::extent_t columns() const { return columns_; }
  [[nodiscard]] const std::array<T, 128>& storage() const { return storage_; }
  void CheckPadding(TestContext& test) const {
    std::array<bool, 128> logical{};
    for (asc::index_t r = 0; r < rows_; ++r) {
      for (asc::index_t c = 0; c < columns_; ++c) {
        logical[Offset(r, c)] = true;
      }
    }
    for (std::size_t i = 0; i < storage_.size(); ++i) {
      if (!logical[i]) {
        ASC_DENSE_TEST_EQ(test, storage_[i], Scalar<T>(-313, 97));
      }
    }
  }

 private:
  [[nodiscard]] std::size_t Offset(asc::index_t r, asc::index_t c) const {
    return static_cast<std::size_t>(layout_ == asc::DenseBlasLayout::kRowMajor
                                        ? r * lda_ + c
                                        : c * lda_ + r) +
           3;
  }
  asc::extent_t rows_;
  asc::extent_t columns_;
  asc::DenseBlasLayout layout_;
  asc::stride_t lda_;
  std::array<T, 128> storage_{};
};

template <typename T>
class Vector {
 public:
  explicit Vector(asc::extent_t size) : size_(size) {
    storage_.fill(Scalar<T>(-713, 13));
  }
  asc::DenseBlasVectorView<T> view(asc::stride_t increment = 1,
                                   asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasVectorView<T>::Create(
        storage_.data() + 1, size_, increment,
        {storage_.data(), sizeof(storage_), space}));
  }
  [[nodiscard]] asc::DenseBlasVectorView<const T> const_view() const {
    return Take(asc::DenseBlasVectorView<const T>::Create(
        storage_.data() + 1, size_, 1,
        {storage_.data(), sizeof(storage_), kHost}));
  }
  [[nodiscard]] const std::array<T, 32>& storage() const { return storage_; }
  void CheckPadding(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, storage_[0], Scalar<T>(-713, 13));
    for (std::size_t i = static_cast<std::size_t>(size_) + 1;
         i < storage_.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, storage_[i], Scalar<T>(-713, 13));
    }
  }

 private:
  asc::extent_t size_;
  std::array<T, 32> storage_{};
};

template <typename Callable>
asc::Status Observe(TestContext& test, Callable call) {
  asc::Status status;
  std::size_t count = 0;
  {
    const asc_dense_test::AllocationProbe probe;
    status = call();
    count = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test, asc_test::ProcessAllocationCountMatches(count, 0));
  return status;
}

void CheckReport(
    TestContext& test, const asc::LapackReport& report,
    asc::LapackOutcome outcome = asc::LapackOutcome::kSuccess,
    asc::LapackOutputValidity validity = asc::LapackOutputValidity::kComplete) {
  ASC_DENSE_TEST_EQ(test, report.provider, asc::LapackProviderIdentity{});
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  ASC_DENSE_TEST_EQ(test, report.output_validity, validity);
  ASC_DENSE_TEST_CHECK(test, report.routine[0] != '\0');
}

template <typename T>
constexpr long double Tolerance() {
  return 150 * static_cast<long double>(
                   std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon());
}

void Near(TestContext& test, Wide actual, Wide expected, long double tolerance,
          long double scale = 1) {
  ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(actual)));
  ASC_DENSE_TEST_CHECK(test, std::abs(actual - expected) <= tolerance * scale);
}

template <typename T>
void Orthogonality(TestContext& test, const Matrix<T>& q) {
  for (asc::index_t left = 0; left < q.columns(); ++left) {
    for (asc::index_t right = 0; right < q.columns(); ++right) {
      Wide dot{};
      for (asc::index_t row = 0; row < q.rows(); ++row) {
        dot += std::conj(Widen(q(row, left))) * Widen(q(row, right));
      }
      Near(test, dot, Wide{left == right ? 1.0L : 0.0L}, Tolerance<T>());
    }
  }
}

template <typename T>
void Reconstruct(TestContext& test, const Matrix<T>& original,
                 const Matrix<T>& packed, const Matrix<T>& q) {
  long double scale = 0;
  long double maximum_error = 0;
  for (asc::index_t r = 0; r < original.rows(); ++r) {
    for (asc::index_t c = 0; c < original.columns(); ++c) {
      Wide value{};
      for (asc::index_t i = 0;
           i < std::min(original.rows(), original.columns()) && i <= c; ++i) {
        value += Widen(q(r, i)) * Widen(packed(i, c));
      }
      scale = std::max(scale, std::abs(Widen(original(r, c))));
      maximum_error =
          std::max(maximum_error, std::abs(value - Widen(original(r, c))));
    }
  }
  // Explicit absolute allowance is only for unavoidable subnormal rounding;
  // it is never an arbitrary additive one that hides tiny-matrix errors.
  const long double subnormal = static_cast<long double>(
      std::numeric_limits<asc::DenseBlasRealType<T>>::denorm_min());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(maximum_error));
  ASC_DENSE_TEST_CHECK(test,
                       maximum_error <= Tolerance<T>() * scale + 8 * subnormal);
}

template <typename T>
void Fill(Matrix<T>& matrix, int fixture, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  const Real scale = std::ldexp(Real{1}, exponent);
  for (asc::index_t r = 0; r < matrix.rows(); ++r) {
    for (asc::index_t c = 0; c < matrix.columns(); ++c) {
      if (fixture == 1) {
        matrix(r, c) =
            Scalar<T>(static_cast<int>(r + 1), static_cast<int>(2 - r)) * scale;
      } else if (fixture == 2) {
        matrix(r, c) =
            r <= c ? Scalar<T>(static_cast<int>(r + c + 1)) * scale : T{0};
      } else if (fixture == 3) {
        matrix(r, c) = T{0};
      } else {
        matrix(r, c) = Scalar<T>(static_cast<int>((r + 2) * (c + 3) % 7 - 3),
                                 static_cast<int>((r + c) % 4 - 1)) *
                       scale;
      }
    }
  }
}

template <typename T>
void ApplyChecks(TestContext& test,
                 asc::LapackHouseholderQrFactorView<T> factor,
                 const Matrix<T>& q, asc::DenseBlasLayout layout) {
  const auto transpose = asc::DenseBlasComplex<T>
                             ? asc::DenseBlasTranspose::kConjugateTranspose
                             : asc::DenseBlasTranspose::kTranspose;
  for (auto side : {asc::DenseBlasSide::kLeft, asc::DenseBlasSide::kRight}) {
    for (auto op : {asc::DenseBlasTranspose::kNone, transpose}) {
      Matrix<T> matrix(side == asc::DenseBlasSide::kLeft ? q.rows() : 3,
                       side == asc::DenseBlasSide::kLeft ? 3 : q.rows(),
                       layout);
      Fill(matrix, 0, 0);
      const auto before = matrix;
      Vector<T> scratch(matrix.rows() == 0 || matrix.columns() == 0 ||
                                factor.tau().size() == 0
                            ? 0
                            : 3);
      asc::LapackReport report;
      const auto status = Observe(test, [&] {
        return asc::ApplyHouseholderQ(Cpu(), side, op, factor, matrix.view(),
                                      scratch.view(), report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      CheckReport(test, report);
      for (asc::index_t r = 0; r < matrix.rows(); ++r) {
        for (asc::index_t c = 0; c < matrix.columns(); ++c) {
          Wide expected{};
          for (asc::index_t i = 0; i < q.rows(); ++i) {
            if (side == asc::DenseBlasSide::kLeft) {
              const Wide value = op == asc::DenseBlasTranspose::kNone
                                     ? Widen(q(r, i))
                                     : std::conj(Widen(q(i, r)));
              expected += value * Widen(before(i, c));
            } else {
              const Wide value = op == asc::DenseBlasTranspose::kNone
                                     ? Widen(q(i, c))
                                     : std::conj(Widen(q(c, i)));
              expected += Widen(before(r, i)) * value;
            }
          }
          Near(test, Widen(matrix(r, c)), expected, Tolerance<T>(), 20);
        }
      }
      matrix.CheckPadding(test);
      scratch.CheckPadding(test);
    }
  }
}

template <typename T>
void Case(TestContext& test, asc::extent_t rows, asc::extent_t columns,
          asc::DenseBlasLayout layout, int fixture, int exponent) {
  Matrix<T> packed(rows, columns, layout);
  Fill(packed, fixture, exponent);
  const auto original = packed;
  Vector<T> tau(std::min(rows, columns));
  Vector<T> scratch(rows == 0 || columns == 0 ? 0 : columns);
  asc::LapackReport report;
  auto status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), packed.view(), tau.view(), scratch.view(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  if (!status.ok()) {
    return;
  }
  CheckReport(test, report);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kHouseholderQr);
  const auto factor = Take(asc::LapackHouseholderQrFactorView<T>::Create(
      packed.const_view(), tau.const_view(), report));
  const auto factor_bytes = packed.storage();
  const auto tau_bytes = tau.storage();
  for (const auto output_layout : kLayouts) {
    for (const auto q_columns : {rows, std::min(rows, columns)}) {
      Matrix<T> q(rows, q_columns, output_layout);
      Vector<T> work(rows == 0 || q_columns == 0 || tau.const_view().size() == 0
                         ? 0
                         : q_columns);
      status = Observe(test, [&] {
        return asc::FormHouseholderQ(Cpu(), factor, q.view(), work.view(),
                                     report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      CheckReport(test, report);
      Orthogonality(test, q);
      Reconstruct(test, original, packed, q);
      q.CheckPadding(test);
      work.CheckPadding(test);
      if (q_columns == rows) {
        ApplyChecks(test, factor, q, output_layout);
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, packed.storage(), factor_bytes);
  ASC_DENSE_TEST_EQ(test, tau.storage(), tau_bytes);
  packed.CheckPadding(test);
  tau.CheckPadding(test);
  scratch.CheckPadding(test);
  if (test.Finish() == 0) {
    std::cout
        << R"({"route":"native","routine":"geqrf","scalar":)"
        << (asc::DenseBlasComplex<T> ? "\"complex" : "\"real") << sizeof(T)
        << R"(","rows":)" << rows << ",\"columns\":" << columns
        << ",\"layout\":" << static_cast<int>(layout)
        << ",\"fixture\":" << fixture << ",\"exponent\":" << exponent
        << ",\"checks\":\"reconstruction,full-economy-Q,orthogonality,"
           "left-right-N-adjoint-apply,padding,preserved-factor,no-new\"}\n";
  }
}

template <typename T>
void StructuralFailures(TestContext& test) {
  Matrix<T> matrix(3, 2, kLayouts[0]);
  Fill(matrix, 0, 0);
  Vector<T> tau(2);
  Vector<T> short_tau(1);
  Vector<T> scratch(2);
  Vector<T> short_work(1);
  const auto original = matrix.storage();
  const auto tau_before = tau.storage();
  asc::LapackReport report;
  const auto unchanged = [&](const asc::Status& status) {
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    CheckReport(test, report, asc::LapackOutcome::kNotRun,
                asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_EQ(test, matrix.storage(), original);
    ASC_DENSE_TEST_EQ(test, tau.storage(), tau_before);
  };
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), short_tau.view(), scratch.view(),
                      report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(), short_work.view(),
                      report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(2), scratch.view(),
                      report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(), scratch.view(2),
                      report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(), tau.view(), report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(asc::MemorySpace::kDevice), tau.view(),
                      scratch.view(), report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(asc::MemorySpace::kPinnedHost),
                      tau.view(), scratch.view(), report);
  }));
  unchanged(Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(asc::MemorySpace::kManaged),
                      tau.view(), scratch.view(), report);
  }));
}

template <typename T>
void ConsumerFailures(TestContext& test) {
  Matrix<T> matrix(3, 2, kLayouts[0]);
  Fill(matrix, 0, 0);
  Vector<T> tau(2);
  Vector<T> scratch(2);
  Vector<T> short_work(1);
  asc::LapackReport report;
  auto status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(), scratch.view(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  const auto factor = Take(asc::LapackHouseholderQrFactorView<T>::Create(
      matrix.const_view(), tau.const_view(), report));
  Matrix<T> target(3, 3, kLayouts[1]);
  const auto target_before = target.storage();
  Vector<T> work(3);
  // Fixed uint8_t enum underlying types permit every uint8_t value. These are
  // deliberate invalid-option tests, not out-of-range integer conversions.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  for (const auto op : {static_cast<asc::DenseBlasTranspose>(99),
                        asc::DenseBlasComplex<T>
                            ? asc::DenseBlasTranspose::kTranspose
                            : asc::DenseBlasTranspose::kConjugateTranspose}) {
    status = Observe(test, [&] {
      return asc::ApplyHouseholderQ(Cpu(), asc::DenseBlasSide::kLeft, op,
                                    factor, target.view(), work.view(), report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, target.storage(), target_before);
  }
  status = Observe(test, [&] {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return asc::ApplyHouseholderQ(Cpu(), static_cast<asc::DenseBlasSide>(99),
                                  asc::DenseBlasTranspose::kNone, factor,
                                  target.view(), work.view(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  status = Observe(test, [&] {
    return asc::FormHouseholderQ(Cpu(), factor, target.view(),
                                 short_work.view(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, target.storage(), target_before);
  status = Observe(test, [&] {
    return asc::FormHouseholderQ(Cpu(), factor, matrix.view(), work.view(),
                                 report);
  });
  ASC_DENSE_TEST_CHECK(test, !status.ok());
}

template <typename T>
void NumericalFailures(TestContext& test) {
  Matrix<T> matrix(3, 2, kLayouts[0]);
  Fill(matrix, 0, 0);
  Vector<T> tau(2);
  Vector<T> scratch(2);
  asc::LapackReport report;
  auto status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(), scratch.view(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  const auto factor = Take(asc::LapackHouseholderQrFactorView<T>::Create(
      matrix.const_view(), tau.const_view(), report));
  Matrix<T> target(3, 3, kLayouts[1]);
  Vector<T> work(3);
  // Explicit source/destination nonfinite rejection occurs before any writes.
  const auto nan = std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
  target(1, 1) = T{nan};
  const auto nan_bytes = target.storage();
  status = Observe(test, [&] {
    return asc::ApplyHouseholderQ(Cpu(), asc::DenseBlasSide::kLeft,
                                  asc::DenseBlasTranspose::kNone, factor,
                                  target.view(), work.view(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(
      test, std::ranges::equal(std::as_bytes(std::span(target.storage())),
                               std::as_bytes(std::span(nan_bytes))));
  CheckReport(test, report, asc::LapackOutcome::kNotRun,
              asc::LapackOutputValidity::kUnchanged);
  matrix(1, 0) = T{nan};
  const auto bad_factor = matrix.storage();
  status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), matrix.view(), tau.view(), scratch.view(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_CHECK(
      test, std::ranges::equal(std::as_bytes(std::span(matrix.storage())),
                               std::as_bytes(std::span(bad_factor))));
  Matrix<T> overflow(2, 1, kLayouts[0]);
  using Real = asc::DenseBlasRealType<T>;
  overflow(0, 0) = T{std::numeric_limits<Real>::max()};
  overflow(1, 0) = overflow(0, 0);
  Vector<T> one_tau(1);
  Vector<T> one_work(1);
  status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), overflow.view(), one_tau.view(), one_work.view(),
                      report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  CheckReport(test, report, asc::LapackOutcome::kPartialResult,
              asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
  ASC_DENSE_TEST_CHECK(test,
                       !asc::LapackHouseholderQrFactorView<T>::Create(
                            overflow.const_view(), one_tau.const_view(), report)
                            .ok());
}

template <typename T>
void ReflectorEdges(TestContext& test) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout : kLayouts) {
    for (const int mode : {0, 1, 2}) {
      Matrix<T> packed(2, 1, layout);
      Vector<T> tau(1);
      Vector<T> scratch(2);
      if (mode == 0) {
        packed(0, 0) = Scalar<T>(3);
        packed(1, 0) = Scalar<T>(4);
      } else if (mode == 1) {
        packed(0, 0) = Scalar<T>(3, 4);
      } else {
        packed(0, 0) = T{std::numeric_limits<Real>::max() * Real{0.75}};
        packed(1, 0) = T{std::numeric_limits<Real>::max() * Real{0.25}};
      }
      const auto original = packed;
      asc::LapackReport report;
      auto status = Observe(test, [&] {
        return asc::Geqrf(Cpu(), packed.view(), tau.view(), scratch.view(),
                          report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      CheckReport(test, report);
      if (mode == 0) {
        Near(test, Widen(packed(0, 0)), Wide{-5}, Tolerance<T>());
        Near(test, Widen(packed(1, 0)), Wide{0.5L}, Tolerance<T>());
        Near(test, Widen(tau.const_view().data()[0]), Wide{1.6L},
             Tolerance<T>());
      } else if (mode == 1) {
        if constexpr (asc::DenseBlasComplex<T>) {
          Near(test, Widen(packed(0, 0)), Wide{-5}, Tolerance<T>());
          Near(test, Widen(tau.const_view().data()[0]), Wide{1.6L, 0.8L},
               Tolerance<T>());
        } else {
          ASC_DENSE_TEST_EQ(test, packed(0, 0), T{3});
          ASC_DENSE_TEST_EQ(test, tau.const_view().data()[0], T{0});
        }
      }
      const auto factor = Take(asc::LapackHouseholderQrFactorView<T>::Create(
          packed.const_view(), tau.const_view(), report));
      Matrix<T> q(2, 2, layout);
      status = Observe(test, [&] {
        return asc::FormHouseholderQ(Cpu(), factor, q.view(), scratch.view(),
                                     report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      Orthogonality(test, q);
      Reconstruct(test, original, packed, q);
      // The implicit path annihilates the tail without using explicit Q.
      // Its independent expected result is [R(0,0), 0].
      auto transformed = original;
      status = Observe(test, [&] {
        return asc::ApplyHouseholderQ(
            Cpu(), asc::DenseBlasSide::kLeft,
            asc::DenseBlasComplex<T>
                ? asc::DenseBlasTranspose::kConjugateTranspose
                : asc::DenseBlasTranspose::kTranspose,
            factor, transformed.view(), scratch.view(), report);
      });
      ASC_DENSE_TEST_CHECK(test, status.ok());
      const auto scale = std::abs(Widen(packed(0, 0)));
      Near(test, Widen(transformed(0, 0)), Widen(packed(0, 0)), Tolerance<T>(),
           scale);
      Near(test, Widen(transformed(1, 0)), Wide{0}, Tolerance<T>(), scale);
    }
  }
}

template <typename T>
void EmptyAndReportAliases(TestContext& test) {
  Vector<T> empty(0);
  asc::LapackReport report;
  const auto huge = std::numeric_limits<asc::extent_t>::max();
  const auto empty_matrix = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, huge, 0, asc::DenseBlasLayout::kRowMajor, 1,
      {nullptr, 0, kHost}));
  auto status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), empty_matrix, empty.view(), empty.view(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  CheckReport(test, report);
  const auto factor = Take(asc::LapackHouseholderQrFactorView<T>::Create(
      asc::DenseBlasMatrixView<const T>(empty_matrix), empty.const_view(),
      report));
  status = Observe(test, [&] {
    return asc::FormHouseholderQ(Cpu(), factor, empty_matrix, empty.view(),
                                 report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  status = Observe(test, [&] {
    return asc::ApplyHouseholderQ(Cpu(), asc::DenseBlasSide::kLeft,
                                  asc::DenseBlasTranspose::kNone, factor,
                                  empty_matrix, empty.view(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  // Construct a metadata-only descriptor over the report. No T is read or
  // written there; overlap validation must reject before resetting the report.
  Vector<T> tau(1);
  Vector<T> scratch(1);
  const auto aliased = Take(asc::DenseBlasMatrixView<T>::Create(
      reinterpret_cast<T*>(&report), 1, 1, asc::DenseBlasLayout::kColumnMajor,
      1, {&report, sizeof(report), kHost}));
  std::array<std::byte, sizeof(report)> before{};
  const auto report_bytes = std::as_bytes(std::span(&report, 1));
  std::copy(report_bytes.begin(), report_bytes.end(), before.begin());
  status = Observe(test, [&] {
    return asc::Geqrf(Cpu(), aliased, tau.view(), scratch.view(), report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test, std::ranges::equal(report_bytes, before));
}

template <typename T>
void All(TestContext& test) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr std::array kShapes{
      std::array<asc::extent_t, 2>{5, 3}, std::array<asc::extent_t, 2>{3, 5},
      std::array<asc::extent_t, 2>{3, 3}, std::array<asc::extent_t, 2>{1, 1},
      std::array<asc::extent_t, 2>{0, 3}, std::array<asc::extent_t, 2>{3, 0},
      std::array<asc::extent_t, 2>{0, 0}};
  for (const auto layout : kLayouts) {
    for (const auto shape : kShapes) {
      for (int fixture = 0; fixture < 4; ++fixture) {
        Case<T>(test, shape[0], shape[1], layout, fixture, 0);
      }
    }
    for (const int exponent : {std::numeric_limits<Real>::max_exponent - 8,
                               std::numeric_limits<Real>::min_exponent + 4,
                               std::numeric_limits<Real>::min_exponent -
                                   std::numeric_limits<Real>::digits}) {
      Case<T>(test, 3, 2, layout, 0, exponent);
    }
  }
  StructuralFailures<T>(test);
  ConsumerFailures<T>(test);
  NumericalFailures<T>(test);
  ReflectorEdges<T>(test);
  EmptyAndReportAliases<T>(test);
}

}  // namespace

int main() {
  TestContext test;
  All<float>(test);
  All<double>(test);
  All<std::complex<float>>(test);
  All<std::complex<double>>(test);
  return test.Finish();
}
