#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>

#include "asc/core/array_io.h"
#include "asc/core/extents.h"
#include "asc/core/matrix_market.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/matrix_market.h"
#include "asc/dense/view.h"
#include "test_support.h"

namespace {

using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using Symmetry = asc::MatrixMarketSymmetry;

template <typename T, std::size_t Count>
void Case(asc_dense_test::TestContext& test, bool write,
          const std::filesystem::path& directory, std::string_view name,
          const std::array<asc::extent_t, 2>& shape,
          const std::array<T, Count>& columns, Symmetry symmetry) {
  const auto path = directory / name;
  std::array<std::byte, 1024> scratch{};
  asc::ArrayIoReport report;
  if (write) {
    auto layout = asc::DenseLayout<2>::Create(shape, asc::LayoutLeft{});
    auto view = asc::DenseView<const T, 2>::Create(columns.data(), *layout,
                                                   asc::MemorySpace::kHost);
    const auto status = asc::SaveDenseMatrixMarket(
        path, *view, symmetry, asc::ArrayFileOverwrite::kTruncate, {}, scratch,
        report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
  } else {
    asc::HostMemoryResource resource;
    auto owner = asc::LoadDenseMatrixMarket<T, Shape>(
        path, resource, asc::LayoutRight{}, scratch, {}, report);
    ASC_DENSE_TEST_CHECK(test, owner.ok() && report.committed);
    if (owner.ok()) {
      auto view = owner->view();
      ASC_DENSE_TEST_EQ(test, view->extents()[0], shape[0]);
      ASC_DENSE_TEST_EQ(test, view->extents()[1], shape[1]);
      const auto rows = static_cast<std::size_t>(shape[0]);
      const auto cols = static_cast<std::size_t>(shape[1]);
      for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < cols; ++column) {
          ASC_DENSE_TEST_EQ(test, view->data()[row * cols + column],
                            columns[row + rows * column]);
        }
      }
    }
  }
  std::cout << (write ? "wrote " : "read ") << name << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 || (std::string_view(argv[1]) != "write" &&
                    std::string_view(argv[1]) != "read")) {
    std::cerr
        << "Usage: matrix_market_interop_test write|read EXISTING_DIRECTORY\n";
    return 2;
  }
  const bool write = std::string_view(argv[1]) == "write";
  const std::filesystem::path directory(argv[2]);
  asc_dense_test::TestContext test;
  Case<std::int64_t>(test, write, directory, "integer-general.mtx", {2, 3},
                     std::array<std::int64_t, 6>{1, 4, 2, 5, 3, 6},
                     Symmetry::kGeneral);
  Case<std::int64_t>(test, write, directory, "integer-symmetric.mtx", {3, 3},
                     std::array<std::int64_t, 9>{1, 2, 3, 2, 4, 5, 3, 5, 6},
                     Symmetry::kSymmetric);
  Case<std::int64_t>(test, write, directory, "integer-skew.mtx", {3, 3},
                     std::array<std::int64_t, 9>{0, 2, 3, -2, 0, 5, -3, -5, 0},
                     Symmetry::kSkewSymmetric);
  Case<double>(test, write, directory, "real-general.mtx", {2, 3},
               std::array<double, 6>{1, 4, 2, 5, 3, 6}, Symmetry::kGeneral);
  Case<double>(test, write, directory, "real-symmetric.mtx", {3, 3},
               std::array<double, 9>{1, 2, 3, 2, 4, 5, 3, 5, 6},
               Symmetry::kSymmetric);
  Case<double>(test, write, directory, "real-skew.mtx", {3, 3},
               std::array<double, 9>{0, 2, 3, -2, 0, 5, -3, -5, 0},
               Symmetry::kSkewSymmetric);
  using C = std::complex<double>;
  Case<C>(
      test, write, directory, "complex-general.mtx", {2, 3},
      std::array<C, 6>{C(1, 0), C(4, 0), C(2, 0), C(5, 0), C(3, 0), C(6, 0)},
      Symmetry::kGeneral);
  Case<C>(test, write, directory, "complex-symmetric.mtx", {2, 2},
          std::array<C, 4>{C(1, 1), C(2, 3), C(2, 3), C(4, 0)},
          Symmetry::kSymmetric);
  Case<C>(test, write, directory, "complex-skew.mtx", {2, 2},
          std::array<C, 4>{C(0, 0), C(2, 3), C(-2, -3), C(0, 0)},
          Symmetry::kSkewSymmetric);
  Case<C>(test, write, directory, "complex-hermitian.mtx", {3, 3},
          std::array<C, 9>{C(4, 0), C(2, 2), C(3, -1), C(2, -2), C(11, 0),
                           C(5, 3), C(3, 1), C(5, -3), C(17, 0)},
          Symmetry::kHermitian);
  return test.Finish();
}
