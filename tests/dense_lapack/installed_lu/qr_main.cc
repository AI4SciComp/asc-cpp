#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_qr.h"
#include "factorization_support.h"

namespace {
using installed_internal::ConstVector;
using installed_internal::kColumn;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Scratch;
using installed_internal::Succeeded;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Vector;
using installed_internal::Wide;
using installed_internal::Widen;

template <typename T>
bool Generate(const asc::ReferenceLapackProvider& provider, Matrix<T, 3, 3>& q,
              std::array<T, 2>& tau, Scratch<T>& scratch) {
  asc::LapackReport report;
  const auto before = q.data;
  const auto tau_before = tau;
  if constexpr (asc::DenseBlasComplex<T>) {
    const auto plan =
        asc::QueryUngqrWorkspace(provider, q.View(), ConstVector(tau), report);
    return plan.ok() && report.called_provider && report.native_info == 0 &&
           q.data == before && tau == tau_before &&
           Succeeded(asc::Ungqr(provider, q.View(), ConstVector(tau), *plan,
                                scratch.workspace, report),
                     report) &&
           q.PaddingEquals(before) && tau == tau_before;
  } else {
    const auto plan =
        asc::QueryOrgqrWorkspace(provider, q.View(), ConstVector(tau), report);
    return plan.ok() && report.called_provider && report.native_info == 0 &&
           q.data == before && tau == tau_before &&
           Succeeded(asc::Orgqr(provider, q.View(), ConstVector(tau), *plan,
                                scratch.workspace, report),
                     report) &&
           q.PaddingEquals(before) && tau == tau_before;
  }
}

template <typename T>
bool CheckQr(const Matrix<T, 3, 2>& original, const Matrix<T, 3, 2>& packed,
             const Matrix<T, 3, 3>& q) {
  for (std::size_t row = 0; row < 3; ++row) {
    const auto first_column = row;
    for (std::size_t column = 0; column < 3; ++column) {
      Wide dot{};
      for (std::size_t inner = 0; inner < 3; ++inner) {
        dot += std::conj(Widen(q.At(inner, first_column))) *
               Widen(q.At(inner, column));
      }
      if (!Near<T>(dot, row == column ? 1 : 0, 1)) {
        return false;
      }
    }
    for (std::size_t column = 0; column < 2; ++column) {
      Wide product{};
      for (std::size_t inner = 0; inner <= column; ++inner) {
        product += Widen(q.At(row, inner)) * Widen(packed.At(inner, column));
      }
      if (!Near<T>(product, Widen(original.At(row, column)), 2)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Apply(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasSide side, asc::DenseBlasTranspose transpose,
           Matrix<T, 3, 2>& packed, std::array<T, 2>& tau, Matrix<T, 3, 3>& c,
           Scratch<T>& scratch) {
  asc::LapackReport report;
  const auto before = c.data;
  if constexpr (asc::DenseBlasComplex<T>) {
    const auto plan =
        asc::QueryUnmqrWorkspace(provider, side, transpose, packed.ConstView(),
                                 ConstVector(tau), c.View(), report);
    return plan.ok() && report.called_provider && report.native_info == 0 &&
           c.data == before &&
           Succeeded(asc::Unmqr(provider, side, transpose, packed.ConstView(),
                                ConstVector(tau), c.View(), *plan,
                                scratch.workspace, report),
                     report);
  } else {
    const auto plan =
        asc::QueryOrmqrWorkspace(provider, side, transpose, packed.ConstView(),
                                 ConstVector(tau), c.View(), report);
    return plan.ok() && report.called_provider && report.native_info == 0 &&
           c.data == before &&
           Succeeded(asc::Ormqr(provider, side, transpose, packed.ConstView(),
                                ConstVector(tau), c.View(), *plan,
                                scratch.workspace, report),
                     report);
  }
}

template <typename T>
Wide QEntry(const Matrix<T, 3, 3>& q, asc::DenseBlasTranspose transpose,
            std::size_t row, std::size_t column) {
  const auto i = column;
  const auto j = row;
  return transpose == asc::DenseBlasTranspose::kNone
             ? Widen(q.At(row, column))
             : std::conj(Widen(q.At(i, j)));
}

template <typename T>
bool CheckApplication(const Matrix<T, 3, 3>& q, const Matrix<T, 3, 3>& original,
                      const Matrix<T, 3, 3>& actual, asc::DenseBlasSide side,
                      asc::DenseBlasTranspose transpose) {
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      Wide expected{};
      long double scale = 0;
      for (std::size_t inner = 0; inner < 3; ++inner) {
        const auto product = side == asc::DenseBlasSide::kLeft
                                 ? QEntry(q, transpose, row, inner) *
                                       Widen(original.At(inner, column))
                                 : Widen(original.At(row, inner)) *
                                       QEntry(q, transpose, inner, column);
        expected += product;
        scale += std::abs(product);
      }
      if (!Near<T>(Widen(actual.At(row, column)), expected, scale + 1)) {
        return false;
      }
    }
  }
  return actual.PaddingEquals(original.data);
}

template <typename T>
bool Applications(const asc::ReferenceLapackProvider& provider,
                  Matrix<T, 3, 2>& packed, std::array<T, 2>& tau,
                  const Matrix<T, 3, 3>& q, Scratch<T>& scratch) {
  const auto factor_bytes = packed.data;
  const auto tau_bytes = tau;
  const auto adjoint = asc::DenseBlasComplex<T>
                           ? asc::DenseBlasTranspose::kConjugateTranspose
                           : asc::DenseBlasTranspose::kTranspose;
  for (const auto layout : {kRow, kColumn}) {
    Matrix<T, 3, 3> original{{}, layout};
    original.data.fill(Value<T>(-63));
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t column = 0; column < 3; ++column) {
        original.At(row, column) = Value<T>(
            1 + 2 * row + column, 0.25 * static_cast<double>(row + column));
      }
    }
    for (const auto side :
         {asc::DenseBlasSide::kLeft, asc::DenseBlasSide::kRight}) {
      for (const auto transpose : {asc::DenseBlasTranspose::kNone, adjoint}) {
        auto c = original;
        if (!Apply(provider, side, transpose, packed, tau, c, scratch) ||
            !CheckApplication(q, original, c, side, transpose) ||
            packed.data != factor_bytes || tau != tau_bytes) {
          return false;
        }
      }
    }
  }
  return true;
}

template <typename T>
bool Qr(const asc::ReferenceLapackProvider& provider, int algorithm,
        asc::DenseBlasLayout layout, asc::DenseBlasLayout output_layout) {
  Matrix<T, 3, 2> a{{}, layout};
  a.data.fill(Value<T>(-91));
  a.At(0, 0) = Value<T>(1, 0.25);
  a.At(0, 1) = a.At(1, 0) = T{};
  a.At(1, 1) = Value<T>(1, -0.125);
  a.At(2, 0) = a.At(2, 1) = Value<T>(1);
  const auto original = a;
  std::array<T, 2> tau{};
  Scratch<T> scratch;
  asc::LapackReport report;
  const auto plan =
      algorithm == 0
          ? asc::QueryGeqrfWorkspace(provider, a.View(), Vector(tau), report)
          : asc::QueryGeqr2Workspace(provider, a.View(), Vector(tau), report);
  if (!plan.ok() || a.data != original.data || tau != std::array<T, 2>{} ||
      report.called_provider != (algorithm == 0) ||
      report.native_info.has_value() != (algorithm == 0)) {
    return false;
  }
  const auto status = algorithm == 0
                          ? asc::Geqrf(provider, a.View(), Vector(tau), *plan,
                                       scratch.workspace, report)
                          : asc::Geqr2(provider, a.View(), Vector(tau), *plan,
                                       scratch.workspace, report);
  if (!Succeeded(status, report) || !a.PaddingEquals(original.data)) {
    return false;
  }
  Matrix<T, 3, 3> q{{}, output_layout};
  q.data.fill(Value<T>(-81));
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      q.At(row, column) = a.At(row, column);
    }
  }
  return Generate(provider, q, tau, scratch) && CheckQr(original, a, q) &&
         Applications(provider, a, tau, q, scratch);
}

template <typename T>
bool AllModes(const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kRow, kColumn}) {
    for (const auto output_layout : {kRow, kColumn}) {
      for (int algorithm = 0; algorithm < 2; ++algorithm) {
        if (!Qr<T>(provider, algorithm, layout, output_layout)) {
          return false;
        }
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool passed = AllModes<float>(provider) && AllModes<double>(provider) &&
                      AllModes<std::complex<float>>(provider) &&
                      AllModes<std::complex<double>>(provider);
  std::puts(passed ? "Installed QR: all 16 scalar routes passed."
                   : "Installed QR failed.");
  return passed ? 0 : 1;
}
