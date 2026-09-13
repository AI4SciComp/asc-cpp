#ifndef ASC_TESTS_DENSE_LAPACK_MIXED_GENERAL_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_MIXED_GENERAL_TEST_SUPPORT_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_mixed_general.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export

namespace asc_mixed_test {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kInteger;
using asc_tridiagonal_test::kLayout;
using asc_tridiagonal_test::kReal;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::kScalar;
using asc_tridiagonal_test::kScratch;
using asc_tridiagonal_test::Narrow;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;

template <typename T>
using Low =
    std::conditional_t<asc::DenseBlasComplex<T>, std::complex<float>, float>;

template <typename T>
struct Problem {
  Rhs<T> a;
  Rhs<T> b;
  Rhs<T> x;
  std::array<asc::index_t, 66> pivots{};
  Problem(asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout al,
          asc::DenseBlasLayout bl, asc::DenseBlasLayout xl)
      : a(n, n, al), b(n, nrhs, bl), x(n, nrhs, xl) {
    pivots.fill(-73);
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::QueryZcgesvWorkspace(provider, a.View(),
                                       Vector(pivots, a.rows),
                                       std::as_const(b).View(), x.View());
    } else {
      return asc::QueryDsgesvWorkspace(provider, a.View(),
                                       Vector(pivots, a.rows),
                                       std::as_const(b).View(), x.View());
    }
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackMixedSolveStatistics& statistics,
                  asc::LapackReport& report) {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::Zcgesv(provider, a.View(), Vector(pivots, a.rows),
                         std::as_const(b).View(), x.View(), plan, workspace,
                         statistics, report);
    } else {
      return asc::Dsgesv(provider, a.View(), Vector(pivots, a.rows),
                         std::as_const(b).View(), x.View(), plan, workspace,
                         statistics, report);
    }
  }
};

template <typename T>
struct Scratch {
  std::array<T, 512> scalar{};
  std::array<T, 2048> layout{};
  std::array<Low<T>, 1024> lower{};
  std::array<double, 66> real{};
  alignas(16) std::array<std::byte, 544> integer{};
  Scratch() {
    scalar.fill(Value<T>(-11, 3));
    layout.fill(Value<T>(-13, 5));
    lower.fill(Value<Low<T>>(-17, 7));
    real.fill(-19);
    integer.fill(std::byte{0x5a});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto set = [&](std::size_t role, auto& data, std::size_t offset) {
      const auto bytes =
          static_cast<std::size_t>(plan.regions[role].minimum_entries) *
          plan.regions[role].entry_bytes;
      if (bytes > (data.size() - 2 * offset) * sizeof(data[0])) {
        std::abort();
      }
      if (bytes != 0) {
        workspace.regions[role] = {data.data() + offset, bytes, kHost};
      }
    };
    set(kScalar, scalar, 1);
    set(kLayout, layout, 1);
    set(kScratch, lower, 1);
    set(kReal, real, 1);
    set(kInteger, integer, 16);
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    const auto check = [&](std::size_t role, const auto& data,
                           std::size_t offset, auto sentinel) {
      const auto end =
          offset + workspace.regions[role].size() / sizeof(data[0]);
      for (std::size_t i = 0; i < data.size(); ++i) {
        if (i < offset || i >= end) {
          ASC_DENSE_TEST_EQ(test, data[i], sentinel);
        }
      }
    };
    check(kScalar, scalar, 1, Value<T>(-11, 3));
    check(kLayout, layout, 1, Value<T>(-13, 5));
    check(kScratch, lower, 1, Value<Low<T>>(-17, 7));
    check(kReal, real, 1, -19.0);
    check(kInteger, integer, 16, std::byte{0x5a});
  }
};
}  // namespace asc_mixed_test
#endif  // ASC_TESTS_DENSE_LAPACK_MIXED_GENERAL_TEST_SUPPORT_H_
