#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_rank_revealing.h"
#include "factorization_support.h"

namespace {
using installed_internal::kColumn;
using installed_internal::kHost;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Vector;
using installed_internal::Wide;
using installed_internal::Widen;

template <typename T>
struct Scratch {
  std::array<T, 5000> scalar{};
  std::array<T, 64> packing{};
  std::array<asc::DenseBlasRealType<T>, 16> real{};
  std::array<std::int32_t, 8> integer32{};
  std::array<std::int64_t, 8> integer64{};
  std::array<asc::index_t, 8> conversion{};

  asc::LapackWorkspace View(const asc::LapackWorkspacePlan& plan,
                            bool preferred) {
    using Kind = asc::LapackWorkspaceKind;
    asc::LapackWorkspace work;
    const auto put = [&](Kind kind, void* data, std::size_t bytes) {
      work.regions[static_cast<std::size_t>(kind)] = {data, bytes, kHost};
    };
    const auto& scalar_plan =
        plan.regions[static_cast<std::size_t>(Kind::kScalar)];
    const auto count =
        preferred ? scalar_plan.preferred_entries : scalar_plan.minimum_entries;
    if (count < 0 || static_cast<std::size_t>(count) > scalar.size()) {
      std::abort();
    }
    put(Kind::kScalar, scalar.data(),
        static_cast<std::size_t>(count) * sizeof(T));
    put(Kind::kReal, real.data(), sizeof(real));
    put(Kind::kLayoutConversion, packing.data(), sizeof(packing));
    put(Kind::kPivotConversion, conversion.data(), sizeof(conversion));
    if (plan.regions[static_cast<std::size_t>(Kind::kInteger)].entry_bytes ==
        4) {
      put(Kind::kInteger, integer32.data(), sizeof(integer32));
    } else {
      put(Kind::kInteger, integer64.data(), sizeof(integer64));
    }
    return work;
  }
};

template <typename T>
bool Reconstruct(const Matrix<T, 4, 3>& factor, const Matrix<T, 4, 3>& original,
                 const std::array<T, 3>& tau,
                 const std::array<asc::index_t, 3>& permutation) {
  // Apply reflectors to [R;0] from the left, in reverse order. This does not
  // call another ASC/provider Q routine or use a provider factor certificate.
  std::array<std::array<Wide, 3>, 4> recovered{};
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = i; j < 3; ++j) {
      recovered[i][j] = Widen(factor.At(i, j));
    }
  }
  for (std::size_t end = 3; end != 0; --end) {
    const auto h = end - 1;
    for (std::size_t j = 0; j < 3; ++j) {
      Wide product = recovered[h][j];
      for (std::size_t i = h + 1; i < 4; ++i) {
        product += std::conj(Widen(factor.At(i, h))) * recovered[i][j];
      }
      product *= Widen(tau[h]);
      recovered[h][j] -= product;
      for (std::size_t i = h + 1; i < 4; ++i) {
        recovered[i][j] -= Widen(factor.At(i, h)) * product;
      }
    }
  }
  for (std::size_t i = 0; i < 4; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      if (!Near<T>(
              recovered[i][j],
              Widen(original.At(i, static_cast<std::size_t>(permutation[j]))),
              32)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Qr(const asc::ReferenceLapackProvider& provider,
        asc::DenseBlasLayout layout, bool preferred) {
  Matrix<T, 4, 3> matrix{{}, layout};
  matrix.data.fill(T{-73});
  for (std::size_t i = 0; i < 4; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      matrix.At(i, j) = Value<T>(i == j ? 4 : 1, i < j ? 0.5 : -0.25);
    }
  }
  const auto original = matrix;
  std::array<asc::index_t, 3> pivots{0, 0,
                                     std::numeric_limits<asc::index_t>::min()};
  const auto flags = pivots;
  std::array<T, 3> tau{T{-7}, T{-7}, T{-7}};
  const auto before_tau = tau;
  asc::LapackReport report;
  const auto plan = Take(asc::QueryGeqp3Workspace(
      provider, matrix.View(), Vector(pivots), Vector(tau), report));
  if (matrix.data != original.data || pivots != flags || tau != before_tau ||
      !report.called_provider || report.native_info != 0) {
    return false;
  }
  Scratch<T> scratch;
  const auto work = scratch.View(plan, preferred);
  if (!installed_internal::Succeeded(
          asc::Geqp3(provider, matrix.View(), Vector(pivots), Vector(tau), plan,
                     work, report),
          report) ||
      report.factor_family != asc::LapackFactorFamily::kColumnPivotedQr ||
      pivots[0] != 3 || !matrix.PaddingEquals(original.data)) {
    return false;
  }
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data(), pivots.size(), asc::LapackFactorFamily::kColumnPivotedQr,
      {pivots.data(), sizeof(pivots), kHost}));
  std::array<asc::index_t, 3> permutation{};
  std::array<std::byte, 3> validation{};
  if (!asc::ValidateColumnPermutation(raw, validation).ok() ||
      !asc::ConvertColumnPermutationToZeroBased(raw, permutation, validation)
           .ok() ||
      asc::ValidateLuPivots(raw, 4).ok()) {
    return false;
  }
  return Reconstruct(matrix, original, tau, permutation);
}

template <typename T>
bool LeastSquaresMath(const Matrix<T, 3, 4>& a,
                      const Matrix<T, 4, 2>& original_rhs,
                      const Matrix<T, 4, 2>& result) {
  for (std::size_t h = 0; h < 2; ++h) {
    std::array<Wide, 3> residual{};
    for (std::size_t i = 0; i < 3; ++i) {
      residual[i] = -Widen(original_rhs.At(i, h));
      for (std::size_t j = 0; j < 4; ++j) {
        residual[i] += Widen(a.At(i, j)) * Widen(result.At(j, h));
      }
    }
    for (std::size_t j = 0; j < 4; ++j) {
      Wide normal{};
      for (std::size_t i = 0; i < 3; ++i) {
        normal += std::conj(Widen(a.At(i, j))) * residual[i];
      }
      const auto expected = Value<T>(1 + j % 2 + h, j % 2 == 0 ? 0.5 : -0.25);
      if (!Near<T>(normal, {}, 32) ||
          !Near<T>(Widen(result.At(j, h)), Widen(expected), 32) ||
          !Near<T>(Widen(result.At(j, h)), Widen(result.At(j % 2, h)), 32)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool LeastSquares(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasLayout al, asc::DenseBlasLayout bl,
                  bool preferred) {
  Matrix<T, 3, 4> a{{}, al};
  Matrix<T, 4, 2> b{{}, bl};
  a.data.fill(T{-73});
  b.data.fill(T{-79});
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 4; ++j) {
      a.At(i, j) = (i % 2 == j % 2) ? Value<T>(i == 1 ? 2 : 1, 0.5) : T{};
    }
    for (std::size_t h = 0; h < 2; ++h) {
      constexpr std::array<T, 3> kResidual{T{1}, T{}, T{-1}};
      b.At(i, h) = kResidual[i];
      for (std::size_t j = 0; j < 4; ++j) {
        b.At(i, h) +=
            a.At(i, j) * Value<T>(1 + j % 2 + h, j % 2 == 0 ? 0.5 : -0.25);
      }
    }
  }
  const auto original = a;
  const auto before_b = b;
  std::array<asc::index_t, 4> pivots{-1, 0, 0, 0};
  const auto flags = pivots;
  using Real = asc::DenseBlasRealType<T>;
  constexpr Real kCutoff = Real{0.001};
  asc::LapackReport report;
  const auto plan = Take(asc::QueryGelsyWorkspace(
      provider, a.View(), b.View(), Vector(pivots), kCutoff, report));
  if (a.data != original.data || b.data != before_b.data || pivots != flags) {
    return false;
  }
  Scratch<T> scratch;
  const auto work = scratch.View(plan, preferred);
  auto short_work = work;
  short_work
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)] = {
      scratch.scalar.data(), 0, kHost};
  asc::index_t rank = -91;
  if (asc::Gelsy(provider, a.View(), b.View(), Vector(pivots), kCutoff, rank,
                 plan, short_work, report)
          .ok() ||
      report.called_provider || rank != -91 || a.data != original.data ||
      b.data != before_b.data || pivots != flags) {
    return false;
  }
  if (!asc::Gelsy(provider, a.View(), b.View(), Vector(pivots), kCutoff, rank,
                  plan, work, report)
           .ok() ||
      rank != 2 || !report.called_provider || report.native_info != 0 ||
      report.outcome != asc::LapackOutcome::kRankDecision ||
      report.output_validity != asc::LapackOutputValidity::kComplete ||
      !a.PaddingEquals(original.data) || !b.PaddingEquals(before_b.data)) {
    return false;
  }
  return LeastSquaresMath(original, before_b, b);
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  bool passed = true;
  for (const auto al : {kColumn, kRow}) {
    for (const bool preferred : {false, true}) {
      passed = Qr<T>(provider, al, preferred) && passed;
      for (const auto bl : {kColumn, kRow}) {
        passed = LeastSquares<T>(provider, al, bl, preferred) && passed;
      }
    }
  }
  return passed;
}
}  // namespace

int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool s = Run<float>(provider);
  const bool d = Run<double>(provider);
  const bool c = Run<std::complex<float>>(provider);
  const bool z = Run<std::complex<double>>(provider);
  return s && d && c && z ? 0 : 1;
}
