#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_EXPERT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_EXPERT_TEST_SUPPORT_H_

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "lu_band_test_support.h"

namespace asc_lu_band_expert_test {
inline constexpr auto kOperations = asc_lu_band_test::kOperations;
using asc_lu_band_test::SameBytes;
using asc_lu_band_test::Take;
using asc_lu_band_test::TestContext;
using asc_lu_band_test::ToWide;
using asc_lu_band_test::Value;
using asc_lu_band_test::Wide;
using asc_lu_band_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
struct Vector {
  std::vector<T> values;
  explicit Vector(asc::extent_t count)
      : values(static_cast<std::size_t>(count) + 2, Value<T>(-211, 17)) {}
  auto View() {
    return Take(asc::DenseBlasVectorView<T>::Create(
        values.data() + 1, static_cast<asc::extent_t>(values.size() - 2), 1,
        {values.data(), values.size() * sizeof(T), kHost}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasVectorView<const T>::Create(
        values.data() + 1, static_cast<asc::extent_t>(values.size() - 2), 1,
        {values.data(), values.size() * sizeof(T), kHost}));
  }
  void Check(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, values.front(), Value<T>(-211, 17));
    ASC_DENSE_TEST_EQ(test, values.back(), Value<T>(-211, 17));
  }
};

template <typename T>
struct Compact {
  asc::extent_t m;
  asc::extent_t n;
  asc::extent_t kl;
  asc::extent_t ku;
  asc::extent_t ld;
  std::vector<T> values;
  explicit Compact(const asc_lu_band_test::Band<T>& source)
      : m(source.m),
        n(source.n),
        kl(source.kl),
        ku(source.ku),
        ld(kl + ku + 4),
        values(static_cast<std::size_t>(ld * n) + 2, Value<T>(-223, 19)) {
    const auto nan =
        std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t row = 0; row < kl + ku + 1; ++row) {
        values[static_cast<std::size_t>(1 + j * ld + row)] = Value<T>(nan, nan);
      }
      for (asc::extent_t i = std::max<asc::extent_t>(0, j - ku);
           i < std::min(m, j + kl + 1); ++i) {
        At(i, j) = source.values[source.Index(i, j)];
      }
    }
  }
  T& At(asc::extent_t i, asc::extent_t j) {
    return values[static_cast<std::size_t>(1 + j * ld + ku + i - j)];
  }
  [[nodiscard]] T At(asc::extent_t i, asc::extent_t j) const {
    if (i < j - ku || i > j + kl) {
      return T{};
    }
    return values[static_cast<std::size_t>(1 + j * ld + ku + i - j)];
  }
  auto View() {
    return Take(asc::ReferenceGeneralBandView<T>::Create(
        values.data() + 1, m, n, kl, ku, ld,
        {values.data(), values.size() * sizeof(T), kHost}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::ReferenceGeneralBandView<const T>::Create(
        values.data() + 1, m, n, kl, ku, ld,
        {values.data(), values.size() * sizeof(T), kHost}));
  }
};

template <typename T>
struct Scratch {
  using Real = asc::DenseBlasRealType<T>;
  std::vector<T> scalar;
  std::vector<Real> real;
  std::vector<T> layout;
  std::vector<std::max_align_t> integer;
  std::size_t integer_bytes;
  explicit Scratch(const asc::LapackWorkspacePlan& plan)
      : scalar(
            static_cast<std::size_t>(plan.regions[kScalar].preferred_entries) +
                2,
            Value<T>(-227, 23)),
        real(
            static_cast<std::size_t>(plan.regions[kReal].preferred_entries) + 2,
            Real{-229}),
        layout(
            static_cast<std::size_t>(plan.regions[kLayout].preferred_entries) +
                2,
            Value<T>(-233, 29)),
        integer((static_cast<std::size_t>(
                     plan.regions[kInteger].preferred_entries) *
                     plan.regions[kInteger].entry_bytes +
                 sizeof(std::max_align_t) - 1) /
                    sizeof(std::max_align_t) +
                2),
        integer_bytes(
            static_cast<std::size_t>(plan.regions[kInteger].preferred_entries) *
            plan.regions[kInteger].entry_bytes) {
    std::fill_n(reinterpret_cast<std::byte*>(integer.data()),
                integer.size() * sizeof(std::max_align_t), std::byte{0x7d});
  }
  asc::LapackWorkspace View() {
    asc::LapackWorkspace result;
    result.regions[kScalar] = {scalar.data() + 1,
                               (scalar.size() - 2) * sizeof(T), kHost};
    result.regions[kReal] = {real.data() + 1, (real.size() - 2) * sizeof(Real),
                             kHost};
    result.regions[kLayout] = {layout.data() + 1,
                               (layout.size() - 2) * sizeof(T), kHost};
    result.regions[kInteger] = {integer.data() + 1, integer_bytes, kHost};
    return result;
  }
  void Check(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, scalar.front(), Value<T>(-227, 23));
    ASC_DENSE_TEST_EQ(test, scalar.back(), Value<T>(-227, 23));
    ASC_DENSE_TEST_EQ(test, real.front(), Real{-229});
    ASC_DENSE_TEST_EQ(test, real.back(), Real{-229});
    ASC_DENSE_TEST_EQ(test, layout.front(), Value<T>(-233, 29));
    ASC_DENSE_TEST_EQ(test, layout.back(), Value<T>(-233, 29));
    const auto* bytes = reinterpret_cast<const std::byte*>(integer.data());
    for (std::size_t i = 0; i < integer.size() * sizeof(std::max_align_t);
         ++i) {
      if (i < sizeof(std::max_align_t) ||
          i >= sizeof(std::max_align_t) + integer_bytes) {
        ASC_DENSE_TEST_EQ(test, bytes[i], std::byte{0x7d});
      }
    }
  }
};

}  // namespace asc_lu_band_expert_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_EXPERT_TEST_SUPPORT_H_
