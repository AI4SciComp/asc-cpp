#ifndef ASC_TESTS_DENSE_LAPACK_PROVIDER_PLACEMENT_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_PROVIDER_PLACEMENT_SUPPORT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "lu_driver_test_support.h"

namespace asc_placement_test {
using asc_driver_test::kHost;
using asc_driver_test::kNone;
using asc_driver_test::Take;
using asc_driver_test::TestContext;
using asc_driver_test::Vector;
using asc_driver_test::WithoutAllocation;

enum class Route : std::uint8_t {
  kGetrf,
  kGetrs,
  kGetrf2,
  kGetf2,
  kGetriQuery,
  kGetri,
  kGesv,
  kGeequ,
  kGeequb,
  kGecon,
  kGerfs,
  kGesvx,
  kGesvxEquilibrated,
  kGesvxFactored,
  kPotrf,
  kPotrf2,
  kPotf2,
  kPotrs,
  kPotri,
  kPosv,
  kCount
};
inline constexpr std::array<std::string_view, 20> kNames{
    "getrf", "getrs",  "getrf2", "getf2", "getri.query", "getri", "gesv",
    "geequ", "geequb", "gecon",  "gerfs", "gesvx",       "gesvx", "gesvx",
    "potrf", "potrf2", "potf2",  "potrs", "potri",       "posv"};
inline constexpr auto kLower = asc::DenseBlasTriangle::kLower;
static_assert(kNames.size() == static_cast<std::size_t>(Route::kCount));

template <typename T>
struct Storage {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 256> scalar{};
  std::array<Real, 256> real{};
  alignas(16) std::array<std::byte, 2048> integer{};
  alignas(16) std::array<std::byte, 2048> logical{};
  alignas(16) std::array<std::byte, 2048> pivots{};
  std::array<T, 256> layout{};
  std::array<T, 256> complex{};
  alignas(16) std::array<std::byte, 2048> scratch{};

  asc::LapackWorkspace Workspace() {
    asc::LapackWorkspace result;
    result.regions = {{{scalar.data(), sizeof(scalar), kHost},
                       {real.data(), sizeof(real), kHost},
                       {integer.data(), sizeof(integer), kHost},
                       {logical.data(), sizeof(logical), kHost},
                       {pivots.data(), sizeof(pivots), kHost},
                       {layout.data(), sizeof(layout), kHost},
                       {complex.data(), sizeof(complex), kHost},
                       {scratch.data(), sizeof(scratch), kHost}}};
    return result;
  }
};

template <typename T>
struct Data {
  using Real = asc::DenseBlasRealType<T>;
  asc_driver_test::Sample<T> values;
  Storage<T> storage;
  asc::LapackEquilibrationStatistics<Real> equilibration{-17, -19, -23};
  Real condition = -29;

  explicit Data(bool row_major) : values(2, 2, row_major ? 15U : 0U) {
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        values.a[values.Offset(0, i, j)] = i == j ? T{4} : T{0};
        values.af[values.Offset(1, i, j)] = i == j ? T{4} : T{0};
        values.b[values.Offset(2, i, j)] = T{4} * static_cast<T>(i + j + 1);
        values.x[values.Offset(3, i, j)] = static_cast<T>(i + j + 1);
      }
      values.rows[i + 1] = 1;
      values.columns[i + 1] = 1;
    }
  }

  [[nodiscard]] auto Snapshot() const {
    std::array<std::byte, sizeof(Data)> bytes;
    std::memcpy(bytes.data(), this, bytes.size());
    return bytes;
  }
};

template <typename T>
auto LuFactor(const Data<T>& data, const asc::LapackReport& factor_report) {
  return Take(
      asc::LapackLuFactorView<T>::Create(data.values.Matrix(data.values.af, 1),
                                         data.values.Pivots(), factor_report));
}

template <typename T>
auto CholeskyFactor(const Data<T>& data,
                    const asc::LapackReport& factor_report) {
  return Take(asc::LapackCholeskyFactorView<T>::Create(
      data.values.Matrix(data.values.af, 1), kLower, factor_report));
}

template <typename T>
asc::LapackReport Prepare(TestContext& test,
                          const asc::ReferenceLapackProvider& provider,
                          Route route, Data<T>& data) {
  auto& values = data.values;
  asc::LapackReport report;
  const auto workspace = data.storage.Workspace();
  if (route == Route::kPotrs || route == Route::kPotri) {
    const auto plan = Take(asc::QueryPotrfWorkspace(
        provider, kLower, values.Matrix(values.af, 1)));
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Potrf(provider, kLower,
                                                   values.Matrix(values.af, 1),
                                                   plan, workspace, report);
                               }).ok());
  } else if (route == Route::kGetrs || route == Route::kGetriQuery ||
             route == Route::kGetri || route == Route::kGecon ||
             route == Route::kGerfs || route == Route::kGesvxFactored) {
    const auto plan = Take(asc::QueryGetrfWorkspace(
        provider, values.Matrix(values.af, 1), Vector(values.pivots, 2)));
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Getrf(provider,
                                                   values.Matrix(values.af, 1),
                                                   Vector(values.pivots, 2),
                                                   plan, workspace, report);
                               }).ok());
  }
  return report;
}
}  // namespace asc_placement_test

#endif  // ASC_TESTS_DENSE_LAPACK_PROVIDER_PLACEMENT_SUPPORT_H_
