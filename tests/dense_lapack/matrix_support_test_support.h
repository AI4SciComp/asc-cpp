#ifndef ASC_TESTS_DENSE_LAPACK_MATRIX_SUPPORT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_MATRIX_SUPPORT_TEST_SUPPORT_H_
#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_matrix_support_prototypes.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_support.h"
#include "installed_lu/matrix_support_numerical.h"  // IWYU pragma: export
#include "tridiagonal_test_support.h"               // IWYU pragma: export
namespace asc_matrix_support_test {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kLayout;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::WithoutAllocation;
template <typename T>
auto Bytes(const T& object) {
  std::array<std::byte, sizeof(T)> bytes{};
  const auto view = std::as_bytes(std::span(&object, 1));
  std::copy(view.begin(), view.end(), bytes.begin());
  return bytes;
}
template <typename T>
struct Matrix {
  std::array<T, 96> data{};
  int rows;
  int columns;
  int leading;
  bool row_major;
  Matrix(int m, int n, bool row, int padding)
      : rows(m),
        columns(n),
        leading(std::max(1, row ? n : m) + padding),
        row_major(row) {
    data.fill(T{71});
  }
  void Fill(int pattern, T marker) {
    asc_matrix_support_numerical::Fill(data.data() + 2, rows, columns, leading,
                                       row_major, pattern, marker);
  }
  [[nodiscard]] auto View() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        data.data() + 2, rows, columns, row_major ? kRow : kColumn, leading,
        {data.data(), sizeof(data), kHost}));
  }
};
template <typename T>
struct Scratch {
  std::array<T, 128> data{};
  Scratch() { data.fill(T{-17}); }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    const auto count = plan.regions[kLayout].minimum_entries;
    if (count < 0 || static_cast<std::size_t>(count) > data.size() - 2) {
      std::abort();
    }
    if (count > 0) {
      result.regions[kLayout] = {
          data.data() + 1, static_cast<std::size_t>(count) * sizeof(T), kHost};
    }
    return result;
  }
  [[nodiscard]] bool Guards(const asc::LapackWorkspace& workspace) const {
    const auto end = 1 + workspace.regions[kLayout].size() / sizeof(T);
    for (std::size_t i = 0; i < data.size(); ++i) {
      if ((i == 0 || i >= end) && data[i] != T{-17}) {
        return false;
      }
    }
    return true;
  }
};
template <typename T, bool Rows>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasMatrixView<const T> matrix) {
  if constexpr (Rows) {
    return asc::QueryIlalrWorkspace(provider, matrix);
  } else {
    return asc::QueryIlalcWorkspace(provider, matrix);
  }
}
template <typename T, bool Rows>
auto Execute(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasMatrixView<const T> matrix, asc::index_t& extent,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (Rows) {
    return asc::Ilalr(provider, matrix, extent, plan, workspace, report);
  } else {
    return asc::Ilalc(provider, matrix, extent, plan, workspace, report);
  }
}
template <typename T, bool Rows>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return Rows ? "ilaslr" : "ilaslc";
  } else if constexpr (std::is_same_v<T, double>) {
    return Rows ? "iladlr" : "iladlc";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Rows ? "ilaclr" : "ilaclc";
  } else {
    return Rows ? "ilazlr" : "ilazlc";
  }
}
template <typename T, bool Rows>
lapack_int Native(lapack_int& m, lapack_int& n, T* a, lapack_int& leading) {
  if constexpr (std::is_same_v<T, float>) {
    return Rows ? ASC_LAPACK_ilaslr(&m, &n, a, &leading)
                : ASC_LAPACK_ilaslc(&m, &n, a, &leading);
  } else if constexpr (std::is_same_v<T, double>) {
    return Rows ? ASC_LAPACK_iladlr(&m, &n, a, &leading)
                : ASC_LAPACK_iladlc(&m, &n, a, &leading);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Rows ? ASC_LAPACK_ilaclr(&m, &n, a, &leading)
                : ASC_LAPACK_ilaclc(&m, &n, a, &leading);
  } else {
    return Rows ? ASC_LAPACK_ilazlr(&m, &n, a, &leading)
                : ASC_LAPACK_ilazlc(&m, &n, a, &leading);
  }
}

}  // namespace asc_matrix_support_test
#endif  // ASC_TESTS_DENSE_LAPACK_MATRIX_SUPPORT_TEST_SUPPORT_H_
