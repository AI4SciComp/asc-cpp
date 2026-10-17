#ifndef ASC_TESTS_DENSE_LAPACK_DMD_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_DMD_TEST_SUPPORT_H_

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export

namespace asc_dmd_test {
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kInteger;
using asc_tridiagonal_test::kLayout;
using asc_tridiagonal_test::kReal;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::kScalar;
using asc_tridiagonal_test::kScratch;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::WithoutAllocation;
template <typename T>
using Real = asc::DenseBlasRealType<T>;
template <typename T>
struct Problem {
  Rhs<T> x, y, z, b, w, s;
  std::array<std::complex<Real<T>>, 66> eigen{};
  std::array<Real<T>, 66> singular{}, residual{};
  asc::index_t rank = -73;
  Problem(asc::extent_t m, asc::extent_t n,
          const std::array<asc::DenseBlasLayout, 6>& layouts)
      : x(m, n, layouts[0]),
        y(m, n, layouts[1]),
        z(m, n, layouts[2]),
        b(m, n, layouts[3]),
        w(n, n, layouts[4]),
        s(n, n, layouts[5]) {
    eigen.fill({-41, 3});
    singular.fill(-43);
    residual.fill(-47);
  }
  asc::LapackDmdBuffers<T> Buffers() {
    return {x.View(),
            y.View(),
            z.View(),
            b.View(),
            w.View(),
            s.View(),
            Vector(eigen, x.columns),
            Vector(singular, x.columns),
            Vector(residual, x.columns)};
  }
};
template <typename T>
struct Scratch {
  std::vector<T> scalar, layout;
  std::vector<Real<T>> real, scratch;
  std::vector<std::byte> integer;
  asc::LapackWorkspace workspace;
  Scratch(const asc::LapackWorkspacePlan& plan, bool preferred) {
    const auto set = [&](std::size_t role, auto& data, std::size_t offset,
                         auto value) {
      const auto entries = preferred ? plan.regions[role].preferred_entries
                                     : plan.regions[role].minimum_entries;
      const auto bytes =
          static_cast<std::size_t>(entries) * plan.regions[role].entry_bytes;
      const auto size = bytes / sizeof(value);
      data.resize(size + 2 * offset, value);
      if (bytes != 0) {
        workspace.regions[role] = {data.data() + offset, bytes, kHost};
      }
    };
    set(kScalar, scalar, 1, Value<T>(-11, 3));
    set(kLayout, layout, 1, Value<T>(-13, 5));
    set(kReal, real, 1, Real<T>{-17});
    set(kScratch, scratch, 1, Real<T>{-19});
    set(kInteger, integer, 16, std::byte{0x5a});
  }
  void Guards(TestContext& test) const {
    const auto check = [&](std::size_t role, const auto& data,
                           std::size_t offset, auto value) {
      const auto end = offset + workspace.regions[role].size() / sizeof(value);
      for (std::size_t i = 0; i < data.size(); ++i) {
        if (i < offset || i >= end) {
          ASC_DENSE_TEST_EQ(test, data[i], value);
        }
      }
    };
    check(kScalar, scalar, 1, Value<T>(-11, 3));
    check(kLayout, layout, 1, Value<T>(-13, 5));
    check(kReal, real, 1, Real<T>{-17});
    check(kScratch, scratch, 1, Real<T>{-19});
    check(kInteger, integer, 16, std::byte{0x5a});
  }
};
}  // namespace asc_dmd_test
#endif  // ASC_TESTS_DENSE_LAPACK_DMD_TEST_SUPPORT_H_
