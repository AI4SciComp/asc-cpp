#include <algorithm>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_aasen_two_stage_faults.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
namespace {
namespace aa = asc_aasen_two_stage_test;
namespace faults = asc_aasen_two_stage_fault_test;
namespace base = asc_indefinite_rook_test;
using faults::Fault;
template <typename T>
bool Accepted(Fault fault) {
  return fault == Fault::kPass || fault == Fault::kRealSingular ||
         (ASC_LAPACK_INTEGER_BITS == 32 && (fault == Fault::kWrite32BitInfo ||
                                            fault == Fault::kWrite32BitOuter ||
                                            fault == Fault::kWrite32BitBand)) ||
         (!asc::DenseBlasComplex<T> &&
          (fault == Fault::kRealOnlyNb || fault == Fault::kRealOnlyWitness));
}
template <typename T>
void Outcome(base::TestContext& test, Fault fault, int n,
             const asc::Status& status, const asc::LapackReport& report) {
  const bool accepted = Accepted<T>(fault);
  const bool singular = faults::SingularInput(fault);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), singular ? n : 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.factor_family,
                    asc::LapackFactorFamily::kAasen);
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      accepted ? (singular ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk)
               : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(
      test, report.output_validity,
      accepted ? (singular ? asc::LapackOutputValidity::kDocumentedPartial
                           : asc::LapackOutputValidity::kComplete)
               : asc::LapackOutputValidity::kUnusable);
  std::int64_t info = singular ? n : 0;
  if (fault == Fault::kOmitInfo ||
      (fault == Fault::kWrite32BitInfo && ASC_LAPACK_INTEGER_BITS == 64)) {
    info = ASC_LAPACK_INTEGER_BITS == 32
               ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
               : std::numeric_limits<std::int64_t>::min();
  } else if (fault == Fault::kNegativeInfo) {
    info = -7;
  } else if (fault == Fault::kOutOfRangeInfo) {
    info = n + 1;
  } else if (fault == Fault::kFalseSingular) {
    info = n;
  }
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(17), info);
  if (accepted && singular) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), n - 1);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
  }
}
template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          bool he, bool upper, bool row, int n, int band_size, int work_size,
          Fault fault) {
  aa::Sample<T> sample(n, he, upper, faults::SingularInput(fault));
  auto a = sample.before;
  const auto at = [&](int i, int j) {
    return 1 + (row ? i * sample.lda + j : j * sample.lda + i);
  };
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      a[at(i, j)] = sample.before[1 + j * sample.lda + i];
    }
  }
  const auto before = a;
  const auto ltb = n * band_size;
  std::vector<T> tb(static_cast<std::size_t>(ltb + 2), T{-73});
  std::vector<asc::index_t> p(static_cast<std::size_t>(n + 2), -71);
  std::vector<asc::index_t> q(p);
  auto matrix = base::Take(asc::DenseBlasMatrixView<T>::Create(
      a.data() + 1, n, n, row ? base::kRow : base::kColumn, sample.lda,
      {a.data(), a.size() * sizeof(T), base::kHost}));
  auto band = aa::Vector(tb, ltb);
  auto pivots = aa::Vector(p, n);
  auto band_pivots = aa::Vector(q, n);
  faults::SetFault(fault);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return aa::Query(provider, upper ? base::kUpper : base::kLower, he, matrix,
                     band, pivots, band_pivots);
  }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  aa::Scratch<T> scratch(plan, n * work_size);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return aa::Factor(provider, upper ? base::kUpper : base::kLower, he, matrix,
                      band, pivots, band_pivots, plan, scratch.workspace,
                      report);
  });
  Outcome<T>(test, fault, n, status, report);
  if (Accepted<T>(fault)) {
    const auto nb = std::min({192, (band_size - 1) / 3, work_size});
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (sample.Selected(i, j)) {
          sample.a[1 + j * sample.lda + i] = a[at(i, j)];
        }
      }
    }
    aa::Reconstruction(test, n, he, upper, sample.a.data() + 1, sample.lda,
                       tb.data() + 1, ltb / n, nb, p.data() + 1, q.data() + 1,
                       sample.full);
  } else {
    for (auto value : p) {
      ASC_DENSE_TEST_EQ(test, value, -71);
    }
    for (auto value : q) {
      ASC_DENSE_TEST_EQ(test, value, -71);
    }
    if (he || row) {
      ASC_DENSE_TEST_CHECK(test, base::EqualBytes(a.data(), before.data(),
                                                  a.size() * sizeof(T)));
    }
  }
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < sample.lda; ++i) {
      if (i >= n || !(row ? sample.Selected(j, i) : sample.Selected(i, j))) {
        const auto index = 1 + j * sample.lda + i;
        ASC_DENSE_TEST_EQ(test, a[index], before[index]);
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, a.front(), before.front());
  ASC_DENSE_TEST_EQ(test, a.back(), before.back());
  ASC_DENSE_TEST_EQ(test, tb.front(), T{-73});
  ASC_DENSE_TEST_EQ(test, tb.back(), T{-73});
  ASC_DENSE_TEST_EQ(test, p.front(), -71);
  ASC_DENSE_TEST_EQ(test, p.back(), -71);
  ASC_DENSE_TEST_EQ(test, q.front(), -71);
  ASC_DENSE_TEST_EQ(test, q.back(), -71);
  scratch.Guards(test);
}
template <typename T>
int Run(bool he) {
  base::TestContext test;
  int cases = 0;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (int f = 0; f <= static_cast<int>(Fault::kRealOnlyWitness); ++f) {
    for (bool upper : {false, true}) {
      for (bool row : {false, true}) {
        for (int n : {1, 3}) {
          for (int band : {4, 577}) {
            for (int work : {1, 192}) {
              Case<T>(test, provider, he, upper, row, n, band, work,
                      static_cast<Fault>(f));
              ++cases;
            }
          }
        }
      }
    }
  }
  std::printf("Two-stage fault cases=%d\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
