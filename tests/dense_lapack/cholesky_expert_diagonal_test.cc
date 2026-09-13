#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_condition.h"
#include "asc/dense/providers/lapack_cholesky_equilibration.h"
#include "cholesky_expert_test_support.h"
#include "cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "src/dense/lapack/internal_cholesky_expert_counts.h"

namespace {
using asc_cholesky_expert_test::CheckSuccess;
using asc_cholesky_expert_test::Scratch;
using asc_cholesky_expert_test::Vector;
using asc_cholesky_test::kLayouts;
using asc_cholesky_test::kTriangles;
using asc_cholesky_test::Layout;
using asc_cholesky_test::Matrix;
using asc_cholesky_test::Narrow;
using asc_cholesky_test::NotANumber;
using asc_cholesky_test::Take;
using asc_cholesky_test::TestContext;
using asc_cholesky_test::Triangle;
using asc_cholesky_test::Wide;
using asc_cholesky_test::WithoutAllocation;
std::size_t g_equilibrations = 0;
std::size_t g_conditions = 0;
std::size_t g_failures = 0;

template <typename T>
void Diagonal(Matrix<T>& a, const std::array<long double, 3>& diagonal) {
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      a(i, j) = NotANumber<T>();
    }
    a(i, i) = Narrow<T>({diagonal[i], 0});
    if constexpr (asc::DenseBlasComplex<T>) {
      a(i, i).imag(std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
    }
  }
}

template <typename T>
void Equilibration(TestContext& test,
                   const asc::ReferenceLapackProvider& provider, Layout layout,
                   bool radix, int bad, bool zero = false) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> a(3, 3, layout);
  std::array<long double, 3> diagonal{0.5L, 8, 128};
  if (bad >= 0) {
    diagonal[static_cast<std::size_t>(bad)] = zero ? 0 : -2;
  }
  Diagonal(a, diagonal);
  const auto before = a.bytes();
  std::array<Real, 3> scales{71, 72, 73};
  Real scond = 97;
  Real amax = 98;
  const auto query = [&] {
    return radix ? asc::QueryPoequbWorkspace(provider, a.const_view(),
                                             Vector(scales, 3), scond, amax)
                 : asc::QueryPoequWorkspace(provider, a.const_view(),
                                            Vector(scales, 3), scond, amax);
  };
  const auto plan = Take(WithoutAllocation(test, query));
  Scratch<T> scratch;
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return radix ? asc::Poequb(provider, a.const_view(), Vector(scales, 3),
                               scond, amax, plan, workspace, report)
                 : asc::Poequ(provider, a.const_view(), Vector(scales, 3),
                              scond, amax, plan, workspace, report);
  });
  a.CheckSame(test, before);
  if (bad >= 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-9999), bad + 1);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-9999), bad);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kNotPositiveDefinite);
    ASC_DENSE_TEST_EQ(test, scond, Real{97});
    for (std::size_t i = 0; i < 3; ++i) {
      ASC_DENSE_TEST_EQ(test, scales[i], static_cast<Real>(diagonal[i]));
    }
    ++g_failures;
    return;
  }
  ASC_DENSE_TEST_CHECK(test, status.ok());
  CheckSuccess(test, provider, report, true);
  ASC_DENSE_TEST_NEAR(test, scond, Real{0.0625}, Real{0},
                      Real{8} * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_EQ(test, amax, Real{128});
  constexpr std::array<long double, 3> kRadixScales{1, 0.5L, 0.125L};
  for (std::size_t i = 0; i < 3; ++i) {
    if (radix) {
      ASC_DENSE_TEST_EQ(test, scales[i], static_cast<Real>(kRadixScales[i]));
    } else {
      const auto product =
          static_cast<long double>(scales[i]) * scales[i] * diagonal[i];
      ASC_DENSE_TEST_CHECK(test, std::abs(product - 1) <=
                                     16 * std::numeric_limits<Real>::epsilon());
    }
  }
  ++g_equilibrations;
}

template <typename T>
void IntegerEvidence(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     const Scratch<T>& scratch,
                     const asc::LapackWorkspacePlan& plan) {
  if constexpr (!asc::DenseBlasComplex<T>) {
    const auto width =
        provider.identity().integer_abi == asc::LapackIntegerAbi::kIlp64
            ? sizeof(std::int64_t)
            : sizeof(std::int32_t);
    ASC_DENSE_TEST_EQ(
        test, plan.regions[asc_cholesky_expert_test::kInteger].entry_bytes,
        width);
    for (std::size_t i = 0; i < 3; ++i) {
      std::int64_t value = 0;
      if (width == sizeof(std::int64_t)) {
        std::memcpy(&value, scratch.integer.data() + i * width, width);
      } else {
        std::int32_t narrow = 0;
        std::memcpy(&narrow, scratch.integer.data() + i * width, width);
        value = narrow;
      }
      ASC_DENSE_TEST_CHECK(test, value == 1 || value == -1);
    }
  }
}

template <typename T>
void Condition(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Layout layout, Triangle triangle, long double scale) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T> factor(3, 3, layout);
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      const bool selected = triangle == Triangle::kLower ? i >= j : i <= j;
      factor(i, j) = selected ? T{} : NotANumber<T>();
    }
    factor(i, i) = Narrow<T>({std::ldexp(scale, static_cast<int>(i)), 0});
  }
  const auto before = factor.bytes();
  const Real norm = static_cast<Real>(16 * scale * scale);
  Real rcond = 79;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPoconWorkspace(provider, triangle, factor.const_view(),
                                    norm, rcond);
  }));
  Scratch<T> scratch;
  scratch.integer.fill(std::byte{0x5A});
  const auto workspace = scratch.view(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Pocon(
                                   provider, triangle, factor.const_view(),
                                   norm, rcond, plan, workspace, report);
                             }).ok());
  CheckSuccess(test, provider, report, true);
  IntegerEvidence(test, provider, scratch, plan);
  ASC_DENSE_TEST_NEAR(test, rcond, Real{0.0625}, Real{0},
                      Real{64} * std::numeric_limits<Real>::epsilon());
  factor.CheckSame(test, before);
  ++g_conditions;
}

template <typename T>
void ExtremeDiagonal(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout : kLayouts) {
    for (const auto value : {std::numeric_limits<Real>::denorm_min(),
                             std::numeric_limits<Real>::min(),
                             std::numeric_limits<Real>::max() / 16}) {
      Matrix<T> a(1, 1, layout);
      a(0, 0) = Narrow<T>({value, 0});
      if constexpr (asc::DenseBlasComplex<T>) {
        a(0, 0).imag(std::numeric_limits<Real>::quiet_NaN());
      }
      std::array<Real, 1> scales{71};
      Real scond = 72;
      Real amax = 73;
      Scratch<T> scratch;
      asc::LapackReport report;
      for (const bool radix : {false, true}) {
        const auto plan = Take(
            radix ? asc::QueryPoequbWorkspace(provider, a.const_view(),
                                              Vector(scales, 1), scond, amax)
                  : asc::QueryPoequWorkspace(provider, a.const_view(),
                                             Vector(scales, 1), scond, amax));
        const auto workspace = scratch.view(plan);
        const auto status = WithoutAllocation(test, [&] {
          return radix
                     ? asc::Poequb(provider, a.const_view(), Vector(scales, 1),
                                   scond, amax, plan, workspace, report)
                     : asc::Poequ(provider, a.const_view(), Vector(scales, 1),
                                  scond, amax, plan, workspace, report);
        });
        ASC_DENSE_TEST_CHECK(test, status.ok());
        CheckSuccess(test, provider, report, true);
        ASC_DENSE_TEST_EQ(test, scond, Real{1});
        ASC_DENSE_TEST_EQ(test, amax, value);
        const auto scaled = static_cast<long double>(scales[0]) * scales[0] *
                            static_cast<long double>(value);
        if (radix) {
          ASC_DENSE_TEST_EQ(test, std::scalbn(Real{1}, std::ilogb(scales[0])),
                            scales[0]);
          ASC_DENSE_TEST_CHECK(test, scaled >= 0.125L && scaled <= 8);
        } else {
          ASC_DENSE_TEST_CHECK(test,
                               std::abs(scaled - 1) <=
                                   16 * std::numeric_limits<Real>::epsilon());
        }
        ++g_equilibrations;
      }
    }
  }
}

template <typename T>
void EmptyAndExceptional(TestContext& test,
                         const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout : kLayouts) {
    Matrix<T> a(0, 0, layout);
    std::array<Real, 1> scales{41};
    Real scond = 31;
    Real amax = 32;
    const auto plan = Take(asc::QueryPoequWorkspace(
        provider, a.const_view(), Vector(scales, 0), scond, amax));
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Poequ(provider, a.const_view(),
                                                   Vector(scales, 0), scond,
                                                   amax, plan, {}, report);
                               }).ok());
    CheckSuccess(test, provider, report, false);
    ASC_DENSE_TEST_EQ(test, scond, Real{1});
    ASC_DENSE_TEST_EQ(test, amax, Real{0});
    const auto radix_plan = Take(asc::QueryPoequbWorkspace(
        provider, a.const_view(), Vector(scales, 0), scond, amax));
    ASC_DENSE_TEST_CHECK(
        test, asc::Poequb(provider, a.const_view(), Vector(scales, 0), scond,
                          amax, radix_plan, {}, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
    Real rcond = 42;
    const auto condition_plan = Take(asc::QueryPoconWorkspace(
        provider, Triangle::kUpper, a.const_view(), Real{0}, rcond));
    ASC_DENSE_TEST_CHECK(
        test, asc::Pocon(provider, Triangle::kUpper, a.const_view(), Real{0},
                         rcond, condition_plan, {}, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
    ASC_DENSE_TEST_EQ(test, rcond, Real{1});

    Matrix<T> one(1, 1, layout);
    one(0, 0) = NotANumber<T>();
    const auto zero_plan = Take(asc::QueryPoconWorkspace(
        provider, Triangle::kLower, one.const_view(), Real{0}, rcond));
    ASC_DENSE_TEST_CHECK(
        test, asc::Pocon(provider, Triangle::kLower, one.const_view(), Real{0},
                         rcond, zero_plan, {}, report)
                  .ok());
    CheckSuccess(test, provider, report, false);
    ASC_DENSE_TEST_EQ(test, rcond, Real{0});
    ASC_DENSE_TEST_CHECK(
        test, !asc::QueryPoconWorkspace(provider, Triangle::kLower,
                                        one.const_view(), Real{-1}, rcond)
                   .ok());
    const auto bad_plan = Take(asc::QueryPoequbWorkspace(
        provider, one.const_view(), Vector(scales, 1), scond, amax));
    Scratch<T> scratch;
    const auto bad_workspace = scratch.view(bad_plan);
    const auto status = WithoutAllocation(test, [&] {
      return asc::Poequb(provider, one.const_view(), Vector(scales, 1), scond,
                         amax, bad_plan, bad_workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, scales[0], Real{41});
    for (const auto invalid : {std::numeric_limits<Real>::infinity(),
                               -std::numeric_limits<Real>::infinity()}) {
      one(0, 0) = Narrow<T>({invalid, 0});
      const auto invalid_status = WithoutAllocation(test, [&] {
        return asc::Poequb(provider, one.const_view(), Vector(scales, 1), scond,
                           amax, bad_plan, bad_workspace, report);
      });
      ASC_DENSE_TEST_EQ(test, invalid_status.code(),
                        asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_CHECK(test, !report.called_provider);
      ASC_DENSE_TEST_EQ(test, scales[0], Real{41});
      ASC_DENSE_TEST_CHECK(
          test, !asc::QueryPoconWorkspace(provider, Triangle::kLower,
                                          one.const_view(), invalid, rcond)
                     .ok());
    }
  }
}

void Counts(TestContext& test) {
  namespace counts = asc::internal_cholesky_expert_counts;
  for (const asc::extent_t limit :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<std::int64_t>::max()}) {
    ASC_DENSE_TEST_CHECK(test, counts::Equilibration(limit - 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Equilibration(limit, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Estimator(limit / 3, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Estimator(limit / 3 + 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::System(0, limit, limit, true).ok());
    ASC_DENSE_TEST_CHECK(test, counts::System(limit, 0, limit, false).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::System(limit, 0, limit, true).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::System(1, limit, limit, true).ok());
    ASC_DENSE_TEST_CHECK(test, counts::System(1, limit - 1, limit, true).ok());
  }
  ASC_DENSE_TEST_CHECK(test, !counts::Estimator(-1, 2147483647).ok());
  ASC_DENSE_TEST_CHECK(test, !counts::Estimator(1, 127).ok());
}

template <typename T>
void All(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kExponent = std::numeric_limits<Real>::max_exponent / 4;
  for (const auto layout : kLayouts) {
    for (const bool radix : {false, true}) {
      for (const int bad : {-1, 0, 2}) {
        Equilibration<T>(test, provider, layout, radix, bad);
        if (bad >= 0) {
          Equilibration<T>(test, provider, layout, radix, bad, true);
        }
      }
    }
    for (const auto triangle : kTriangles) {
      for (const auto scale :
           {1.L, std::ldexp(1.L, -kExponent), std::ldexp(1.L, kExponent)}) {
        Condition<T>(test, provider, layout, triangle, scale);
      }
    }
  }
  EmptyAndExceptional<T>(test, provider);
  ExtremeDiagonal<T>(test, provider);
}

}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  All<float>(test, provider);
  All<double>(test, provider);
  All<std::complex<float>>(test, provider);
  All<std::complex<double>>(test, provider);
  Counts(test);
  std::cout << "Cholesky expert diagonal/condition: equilibrations="
            << g_equilibrations << " conditions=" << g_conditions
            << " actual_nonpositive_failures=" << g_failures << '\n';
  return test.Finish();
}
