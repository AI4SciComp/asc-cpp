#ifndef ASC_TESTS_DENSE_LAPACK_SVD_LEAST_SQUARES_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_SVD_LEAST_SQUARES_TEST_SUPPORT_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_svd_least_squares.h"
#include "lapack_build_config.h"
#include "least_squares_test_support.h"
#include "svd_least_squares_faults.h"

namespace asc_svd_least_squares_test {
using asc_least_squares_test::Layout;
using asc_least_squares_test::Matrix;
using asc_least_squares_test::Narrow;
using asc_least_squares_test::NotANumber;
using asc_least_squares_test::SameBits;
using asc_least_squares_test::Take;
using asc_least_squares_test::TestContext;
using asc_least_squares_test::Vector;
using asc_least_squares_test::Wide;
using asc_least_squares_test::Widen;
using asc_least_squares_test::WithoutAllocation;
using Integer = std::conditional_t<ASC_LAPACK_INTEGER_BITS == 64, std::int64_t,
                                   std::int32_t>;

constexpr std::size_t Index(asc::LapackWorkspaceKind kind) {
  return static_cast<std::size_t>(kind);
}

template <typename T>
class Guarded {
 public:
  explicit Guarded(std::size_t size)
      : size_(size), values_(size + 6, Sentinel()) {}
  [[nodiscard]] asc::MutableMemoryView view() {
    return {values_.data() + 3, size_ * sizeof(T), asc::MemorySpace::kHost};
  }
  T* data() { return values_.data() + 3; }
  [[nodiscard]] auto vector() {
    return Take(asc::DenseBlasVectorView<T>::Create(
        data(), static_cast<asc::extent_t>(size_), 1, view()));
  }
  void CheckGuards(TestContext& test) const {
    for (std::size_t i = 0; i < 3; ++i) {
      ASC_DENSE_TEST_CHECK(test, SameBits(values_[i], Sentinel()));
      ASC_DENSE_TEST_CHECK(test, SameBits(values_[size_ + 3 + i], Sentinel()));
    }
  }
  [[nodiscard]] const std::vector<T>& bytes() const { return values_; }
  void CheckSame(TestContext& test, const std::vector<T>& before) const {
    for (std::size_t i = 0; i < values_.size(); ++i) {
      ASC_DENSE_TEST_CHECK(test, SameBits(values_[i], before[i]));
    }
  }

 private:
  static T Sentinel() {
    if constexpr (std::is_integral_v<T>) {
      return T{-171};
    } else {
      return Narrow<T>({-171, 13});
    }
  }
  std::size_t size_;
  std::vector<T> values_;
};

template <typename T>
class Scratch {
 public:
  Scratch(const asc::LapackWorkspacePlan& plan, bool preferred)
      : scalar_(Count(plan, asc::LapackWorkspaceKind::kScalar, preferred)),
        real_(Count(plan, asc::LapackWorkspaceKind::kReal, preferred)),
        integer_(Count(plan, asc::LapackWorkspaceKind::kInteger, preferred)),
        staging_(Count(plan, asc::LapackWorkspaceKind::kScratch, preferred)),
        packing_(Count(plan, asc::LapackWorkspaceKind::kLayoutConversion,
                       preferred)) {}
  [[nodiscard]] asc::LapackWorkspace view() {
    asc::LapackWorkspace result;
    result.regions[Index(asc::LapackWorkspaceKind::kScalar)] = scalar_.view();
    result.regions[Index(asc::LapackWorkspaceKind::kReal)] = real_.view();
    result.regions[Index(asc::LapackWorkspaceKind::kInteger)] = integer_.view();
    result.regions[Index(asc::LapackWorkspaceKind::kScratch)] = staging_.view();
    result.regions[Index(asc::LapackWorkspaceKind::kLayoutConversion)] =
        packing_.view();
    return result;
  }
  void CheckGuards(TestContext& test) const {
    scalar_.CheckGuards(test);
    real_.CheckGuards(test);
    integer_.CheckGuards(test);
    staging_.CheckGuards(test);
    packing_.CheckGuards(test);
  }
  void CheckSame(TestContext& test, const Scratch& before) const {
    scalar_.CheckSame(test, before.scalar_.bytes());
    real_.CheckSame(test, before.real_.bytes());
    integer_.CheckSame(test, before.integer_.bytes());
    staging_.CheckSame(test, before.staging_.bytes());
    packing_.CheckSame(test, before.packing_.bytes());
  }

 private:
  static std::size_t Count(const asc::LapackWorkspacePlan& plan,
                           asc::LapackWorkspaceKind kind, bool preferred) {
    const auto region = plan.regions[Index(kind)];
    return static_cast<std::size_t>(preferred ? region.preferred_entries
                                              : region.minimum_entries);
  }
  Guarded<T> scalar_;
  Guarded<asc::DenseBlasRealType<T>> real_;
  Guarded<Integer> integer_;
  Guarded<asc::DenseBlasRealType<T>> staging_;
  Guarded<T> packing_;
};

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, Routine routine,
           asc::DenseBlasMatrixView<T> a, asc::DenseBlasMatrixView<T> b,
           asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> s,
           asc::DenseBlasRealType<T> rcond, asc::LapackReport& report) {
  return routine == Routine::kGelss
             ? asc::QueryGelssWorkspace(provider, a, b, s, rcond, report)
             : asc::QueryGelsdWorkspace(provider, a, b, s, rcond, report);
}

template <typename T>
auto Execute(const asc::ReferenceLapackProvider& provider, Routine routine,
             asc::DenseBlasMatrixView<T> a, asc::DenseBlasMatrixView<T> b,
             asc::DenseBlasVectorView<asc::DenseBlasRealType<T>> s,
             asc::DenseBlasRealType<T> rcond, asc::index_t& rank,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  return routine == Routine::kGelss ? asc::Gelss(provider, a, b, s, rcond, rank,
                                                 plan, workspace, report)
                                    : asc::Gelsd(provider, a, b, s, rcond, rank,
                                                 plan, workspace, report);
}
}  // namespace asc_svd_least_squares_test

#endif  // ASC_TESTS_DENSE_LAPACK_SVD_LEAST_SQUARES_TEST_SUPPORT_H_
