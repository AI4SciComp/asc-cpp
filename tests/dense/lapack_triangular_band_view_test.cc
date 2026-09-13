
#include <array>
#include <complex>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Layout = asc::DenseBlasLayout;
using Triangle = asc::DenseBlasTriangle;

template <typename T>
asc::ConstMemoryView Backing(std::span<T> values,
                             asc::MemorySpace space = asc::MemorySpace::kHost) {
  return {values.data(), values.size_bytes(), space};
}

template <typename T>
void TestTables(TestContext& test) {
  static_assert(std::is_same_v<
                decltype(std::declval<asc::LapackTriangularBandView<const T>>()
                             .storage()
                             .data()),
                const T*>);
  // Literal physical tables for n=3, kd=4, ld=6. Each coordinate appears
  // once, with no Hermitian completion. Positions are independent fixtures.
  const std::array<std::array<std::size_t, 6>, 4> positions{{
      {{4, 9, 14, 10, 15, 16}},  // Column upper: (00,01,02,11,12,22).
      {{0, 1, 2, 6, 7, 12}},     // Column lower: (00,10,20,11,21,22).
      {{0, 1, 2, 6, 7, 12}},     // Row upper:    (00,01,02,11,12,22).
      {{4, 9, 14, 10, 15, 16}},  // Row lower:    (00,10,20,11,21,22).
  }};
  std::size_t fixture = 0;
  for (auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    for (auto triangle : {Triangle::kUpper, Triangle::kLower}) {
      std::array<T, 18> values;
      values.fill(T{-71});
      for (std::size_t i = 0; i < 6; ++i) {
        values[positions[fixture][i]] =
            T{static_cast<asc::DenseBlasRealType<T>>(i + 1)};
      }
      const auto before = values;
      const auto view = asc::LapackTriangularBandView<T>::Create(
          values.data(), 3, 4, triangle, layout, 6, Backing<T>(values));
      ASC_DENSE_TEST_CHECK(test, view.ok());
      if (!view.ok()) {
        return;
      }
      ASC_DENSE_TEST_EQ(test, view->order(), 3);
      ASC_DENSE_TEST_EQ(test, view->bandwidth(), 4);
      ASC_DENSE_TEST_EQ(test, view->layout(), layout);
      ASC_DENSE_TEST_EQ(test, view->triangle(), triangle);
      const auto storage = view->storage();
      ASC_DENSE_TEST_EQ(test, storage.data(), values.data());
      ASC_DENSE_TEST_EQ(test, storage.leading_dimension(), 6);
      ASC_DENSE_TEST_EQ(test, storage.rows(),
                        layout == Layout::kColumnMajor ? 6 : 3);
      ASC_DENSE_TEST_EQ(test, storage.columns(),
                        layout == Layout::kColumnMajor ? 3 : 6);
      ASC_DENSE_TEST_EQ(test, storage.reachable_storage().size(),
                        sizeof(values));
      ASC_DENSE_TEST_EQ(test, view->diagonal_row(),
                        fixture == 0 || fixture == 3 ? 4 : 0);
      for (std::size_t i = 0; i < 6; ++i) {
        ASC_DENSE_TEST_EQ(test, storage.data()[positions[fixture][i]],
                          T{static_cast<asc::DenseBlasRealType<T>>(i + 1)});
      }
      ASC_DENSE_TEST_CHECK(test, values == before);
      // The older BLAS bandwidth restriction remains independently checked.
      ASC_DENSE_TEST_CHECK(
          test, !asc::DenseBlasTriangularBandView<T>::Create(
                     values.data(), 3, 4, layout, 6, Backing<T>(values))
                     .ok());
      ++fixture;
    }
  }
}

template <typename T>
void TestDomain(TestContext& test) {
  std::array<T, 32> values{};
  const auto before = values;
  constexpr auto kMax = std::numeric_limits<asc::extent_t>::max();
  for (auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    for (auto triangle : {Triangle::kUpper, Triangle::kLower}) {
      for (asc::extent_t n : {0, 1, 2, 3}) {
        for (asc::extent_t kd : {0, 1, 2, 3, 4}) {
          const auto view = asc::LapackTriangularBandView<T>::Create(
              values.data(), n, kd, triangle, layout, kd + 2,
              Backing<T>(values));
          ASC_DENSE_TEST_CHECK(test, view.ok());
          ASC_DENSE_TEST_EQ(test, view->storage().reachable_storage().size(),
                            static_cast<std::size_t>(n * (kd + 2)) * sizeof(T));
          const auto immutable = asc::LapackTriangularBandView<const T>::Create(
              values.data(), n, kd, triangle, layout, kd + 2,
              Backing<T>(values));
          ASC_DENSE_TEST_CHECK(test, immutable.ok());
          const auto pinned = asc::LapackTriangularBandView<T>::Create(
              values.data(), n, kd, triangle, layout, kd + 2,
              Backing<T>(values, asc::MemorySpace::kPinnedHost));
          ASC_DENSE_TEST_CHECK(test, pinned.ok());
        }
      }
      const auto empty = asc::LapackTriangularBandView<T>::Create(
          nullptr, 0, kMax - 1, triangle, layout, kMax,
          {nullptr, 0, asc::MemorySpace::kHost});
      ASC_DENSE_TEST_CHECK(test, empty.ok());
    }
  }
  ASC_DENSE_TEST_CHECK(test, values == before);
}

template <typename T>
void TestStorageFailures(TestContext& test) {
  std::array<T, 32> values{};
  const auto before = values;
  constexpr auto kMax = std::numeric_limits<asc::extent_t>::max();
  for (auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
    for (auto triangle : {Triangle::kUpper, Triangle::kLower}) {
      const auto overflow = asc::LapackTriangularBandView<T>::Create(
          values.data(), 2, 0, triangle, layout, kMax, Backing<T>(values));
      ASC_DENSE_TEST_EQ(test, overflow.status().code(),
                        asc::ErrorCode::kOverflow);
      const auto kd_overflow = asc::LapackTriangularBandView<T>::Create(
          nullptr, 0, kMax, triangle, layout, kMax,
          {nullptr, 0, asc::MemorySpace::kHost});
      ASC_DENSE_TEST_EQ(test, kd_overflow.status().code(),
                        asc::ErrorCode::kOverflow);
      const auto short_stride = asc::LapackTriangularBandView<T>::Create(
          values.data(), 3, 2, triangle, layout, 2, Backing<T>(values));
      ASC_DENSE_TEST_EQ(test, short_stride.status().code(),
                        asc::ErrorCode::kShape);
      ASC_DENSE_TEST_CHECK(test, !asc::LapackTriangularBandView<T>::Create(
                                      values.data(), 3, 2, triangle, layout, 4,
                                      {values.data(), 12 * sizeof(T) - 1,
                                       asc::MemorySpace::kHost})
                                      .ok());
      // Full last-row/column padding belongs to the reachable span.
      ASC_DENSE_TEST_CHECK(test,
                           !asc::LapackTriangularBandView<T>::Create(
                                values.data(), 3, 2, triangle, layout, 4,
                                Backing<T>(std::span<T>(values).first(11)))
                                .ok());
      ASC_DENSE_TEST_CHECK(test,
                           !asc::LapackTriangularBandView<T>::Create(
                                values.data() + 1, 3, 2, triangle, layout, 4,
                                Backing<T>(std::span<T>(values).first(12)))
                                .ok());
      auto* misaligned =
          reinterpret_cast<T*>(reinterpret_cast<std::byte*>(values.data()) + 1);
      ASC_DENSE_TEST_CHECK(
          test, !asc::LapackTriangularBandView<T>::Create(
                     misaligned, 3, 2, triangle, layout, 4, Backing<T>(values))
                     .ok());
      ASC_DENSE_TEST_CHECK(test, !asc::LapackTriangularBandView<T>::Create(
                                      nullptr, 1, 0, triangle, layout, 1,
                                      {nullptr, 0, asc::MemorySpace::kHost})
                                      .ok());
      for (auto space :
           {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
        const auto result = asc::LapackTriangularBandView<T>::Create(
            values.data(), 3, 2, triangle, layout, 4,
            Backing<T>(values, space));
        ASC_DENSE_TEST_EQ(test, result.status().code(),
                          asc::ErrorCode::kMemoryAccess);
      }
    }
  }
  ASC_DENSE_TEST_CHECK(test, values == before);
}

template <typename T>
void TestInvalidTags(TestContext& test) {
  std::array<T, 32> values{};
  const auto before = values;
  const auto negative_order = asc::LapackTriangularBandView<T>::Create(
      values.data(), -1, 0, Triangle::kUpper, 1, Backing<T>(values));
  ASC_DENSE_TEST_EQ(test, negative_order.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  const auto negative_kd = asc::LapackTriangularBandView<T>::Create(
      values.data(), 1, -1, Triangle::kUpper, 1, Backing<T>(values));
  ASC_DENSE_TEST_EQ(test, negative_kd.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_triangle = static_cast<Triangle>(9);
  const auto triangle_result = asc::LapackTriangularBandView<T>::Create(
      values.data(), 1, 0, invalid_triangle, 1, Backing<T>(values));
  ASC_DENSE_TEST_EQ(test, triangle_result.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid_layout = static_cast<Layout>(9);
  const auto layout_result = asc::LapackTriangularBandView<T>::Create(
      values.data(), 1, 0, Triangle::kUpper, invalid_layout, 1,
      Backing<T>(values));
  ASC_DENSE_TEST_EQ(test, layout_result.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test, values == before);
}

template <typename T>
void TestScalar(TestContext& test) {
  TestTables<T>(test);
  TestDomain<T>(test);
  TestStorageFailures<T>(test);
  TestInvalidTags<T>(test);
}

}  // namespace

int main() {
  TestContext test;
  TestScalar<float>(test);
  TestScalar<double>(test);
  TestScalar<std::complex<float>>(test);
  TestScalar<std::complex<double>>(test);
  return test.Finish();
}
