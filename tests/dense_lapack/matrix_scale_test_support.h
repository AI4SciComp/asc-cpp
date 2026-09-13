#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_SCALE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_SCALE_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_scale.h"
#include "tridiagonal_test_support.h"
namespace asc_scale_test {
namespace support = asc_tridiagonal_test;
using support::Take;
using support::TestContext;
using Part = asc::LapackMatrixScalePart;
using Extent = asc::extent_t;
template <typename T>
bool SameBytes(const T& a, const T& b) {
  const auto first = std::as_bytes(std::span(&a, 1));
  const auto second = std::as_bytes(std::span(&b, 1));
  return std::equal(first.begin(), first.end(), second.begin());
}

inline Part PartFor(char type) {
  switch (type) {
    case 'G':
      return Part::kAll;
    case 'L':
      return Part::kLower;
    case 'U':
      return Part::kUpper;
    default:
      return Part::kUpperHessenberg;
  }
}

template <typename T>
struct Problem {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 512> data{};
  std::array<T, 512> packed{};
  Extent rows;
  Extent columns;
  Extent lower;
  Extent upper;
  Extent leading = 1;
  char type;
  asc::DenseBlasLayout layout;

  Problem(char selected, Extent m, Extent n, Extent kl, Extent ku,
          asc::DenseBlasLayout order)
      : rows(m),
        columns(n),
        lower(kl),
        upper(ku),
        type(selected),
        layout(order) {
    auto contiguous = layout == support::kColumn ? rows : columns;
    if (type == 'Z') {
      contiguous = 2 * lower + upper + 1;
    } else if (type == 'B' || type == 'Q') {
      contiguous = lower + 1;
    }
    leading = contiguous + 3;
    data.fill(support::Value<T>(-37, 19));
    packed.fill(support::Value<T>(-41, 23));
  }

  [[nodiscard]] bool Selected(Extent i, Extent j) const {
    switch (type) {
      case 'G':
        return true;
      case 'L':
        return i >= j;
      case 'U':
        return i <= j;
      case 'H':
        return i <= j + 1;
      case 'B':
        return i >= j && i - j <= lower;
      case 'Q':
        return j >= i && j - i <= upper;
      default:
        return i - j <= lower && j - i <= upper;
    }
  }

  [[nodiscard]] std::size_t Offset(Extent i, Extent j) const {
    Extent offset = 0;
    if (type == 'Z') {
      offset = j * leading + lower + upper + i - j;
    } else if (type == 'B') {
      offset = layout == support::kColumn ? j * leading + i - j
                                          : i * leading + lower + j - i;
    } else if (type == 'Q') {
      offset = layout == support::kColumn ? j * leading + upper + i - j
                                          : i * leading + j - i;
    } else {
      offset = layout == support::kColumn ? j * leading + i : i * leading + j;
    }
    return static_cast<std::size_t>(offset + 1);
  }

  auto Full() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        data.data() + 1, rows, columns, layout, leading,
        {data.data(), sizeof(data), support::kHost}));
  }
  auto PositiveBand() {
    return Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        data.data() + 1, rows, lower,
        type == 'B' ? asc::DenseBlasTriangle::kLower
                    : asc::DenseBlasTriangle::kUpper,
        layout, leading, {data.data(), sizeof(data), support::kHost}));
  }
  auto GeneralBand() {
    return Take(asc::LapackLuBandView<T>::Create(
        data.data() + 1, rows, columns, lower, upper, leading,
        {data.data(), sizeof(data), support::kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if (type == 'Z') {
      return asc::QueryLasclWorkspace(provider, GeneralBand());
    }
    if (type == 'B' || type == 'Q') {
      return asc::QueryLasclWorkspace(provider, PositiveBand());
    }
    return asc::QueryLasclWorkspace(provider, PartFor(type), Full());
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider, Real from,
                  Real to, const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
    if (type == 'Z') {
      return asc::Lascl(provider, from, to, GeneralBand(), plan, workspace,
                        report);
    }
    if (type == 'B' || type == 'Q') {
      return asc::Lascl(provider, from, to, PositiveBand(), plan, workspace,
                        report);
    }
    return asc::Lascl(provider, PartFor(type), from, to, Full(), plan,
                      workspace, report);
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[support::kLayout].minimum_entries;
    if (count > 0) {
      if (static_cast<std::size_t>(count) > packed.size() - 2) {
        std::abort();
      }
      workspace.regions[support::kLayout] = {
          packed.data() + 1, static_cast<std::size_t>(count) * sizeof(T),
          support::kHost};
    }
    return workspace;
  }
};

}  // namespace asc_scale_test
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_SCALE_TEST_SUPPORT_H_
