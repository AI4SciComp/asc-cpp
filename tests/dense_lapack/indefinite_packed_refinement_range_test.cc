#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_refinement_fidelity.h"
#include "indefinite_packed_refinement_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace base = asc_indefinite_test;
namespace refinement = asc_packed_refinement_test;
template <typename T>
void Reset(refinement::Fixture<T>& sample, asc::DenseBlasRealType<T> scale) {
  auto& a = sample.system.a;
  for (int j = 0; j < a.n; ++j) {
    for (int i = 0; i < a.n; ++i) {
      T value{};
      if (a.n == 1 || i != j) {
        value = base::Value<T>(scale, a.n == 1 ? 0 : scale);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (a.hermitian && i < j) {
            value = std::conj(value);
          }
        }
      }
      a.full[i * a.n + j] = base::ToWide(value);
      if (a.Selected(i, j)) {
        a.a[a.Offset(i, j)] = value;
        if constexpr (asc::DenseBlasComplex<T>) {
          if (a.hermitian && i == j) {
            a.a[a.Offset(i, j)].imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
      }
    }
  }
  a.original = a.a;
  std::size_t slot = 1;
  for (int major = 0; major < a.n; ++major) {
    for (int minor = 0; minor < a.n; ++minor) {
      const int i = sample.original_layout == base::kColumn ? minor : major;
      const int j = sample.original_layout == base::kColumn ? major : minor;
      if (a.Selected(i, j)) {
        sample.original[slot++] = a.a[a.Offset(i, j)];
      }
    }
  }
}

template <typename T>
void CheckReport(base::TestContext& test, const refinement::Fixture<T>& sample,
                 const asc::Status& status, const asc::LapackReport& report) {
  bool finite = true;
  bool negative = false;
  for (int j = 0; j < sample.system.nrhs; ++j) {
    const auto forward = sample.ferr[j + 1];
    const auto backward = sample.berr[j + 1];
    finite = finite && std::isfinite(forward) && std::isfinite(backward);
    negative = negative || forward < 0 || backward < 0;
  }
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, 0);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  if (negative) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
  } else if (!finite) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  }
}

template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          refinement::Fixture<T>& sample, bool fidelity) {
  sample.Prepare(test, provider);
  const auto before = sample.original;
  for (int j = 0; j < sample.system.nrhs; ++j) {
    for (int i = 0; i < sample.system.a.n; ++i) {
      const auto rhs =
          base::ToWide(sample.system.original_rhs[sample.system.Offset(i, j)]);
      ASC_DENSE_TEST_CHECK(
          test, std::isfinite(rhs.real()) && std::isfinite(rhs.imag()));
    }
  }
  const auto plan = base::Take(base::WithoutAllocation(
      test, [&] { return refinement::Query(provider, sample); }));
  refinement::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return refinement::Refine(provider, sample, plan, workspace, report);
  });
  CheckReport(test, sample, status, report);
  if (fidelity) {
    refinement::Fidelity(test, sample);
  } else {
    sample.Mathematics(test);
  }
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(sample.original.data(),
                                              before.data(), sizeof(before)));
  sample.Padding(test);
  scratch.Guards(test, workspace);
  std::printf("FERR0=%La BERR0=%La status=%d\n",
              static_cast<long double>(sample.ferr[1]),
              static_cast<long double>(sample.berr[1]),
              static_cast<int>(status.code()));
}

template <typename T>
int Run(bool hermitian, bool fidelity) {
  using Real = asc::DenseBlasRealType<T>;
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const Real minimum = std::numeric_limits<Real>::min();
  const Real maximum = std::numeric_limits<Real>::max();
  const std::array<Real, 8> scales{
      minimum / 8, minimum / 2, minimum,     Real{1},
      maximum / 8, maximum / 4, maximum / 2, Real{0.75} * maximum};
  int cases = 0;
  for (int n : {1, 2}) {
    for (auto triangle : {base::kUpper, base::kLower}) {
      for (unsigned layouts = 0; layouts < 16; ++layouts) {
        const auto layout = [&](unsigned bit) {
          return (layouts & (1U << bit)) != 0 ? base::kRow : base::kColumn;
        };
        for (Real scale : scales) {
          const int nrhs = layouts % 2 == 0 ? 1 : 3;
          refinement::Fixture<T> sample(n, nrhs, hermitian, triangle, layout(0),
                                        layout(1), layout(2), layout(3), 0, 3);
          Reset(sample, scale);
          std::printf(
              "Packed refinement range n=%d nrhs=%d he=%d tri=%d layouts=%u "
              "scale=%La\n",
              n, nrhs, static_cast<int>(hermitian), static_cast<int>(triangle),
              layouts, static_cast<long double>(scale));
          Case(test, provider, sample, fidelity);
          ++cases;
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 512);
  std::printf("Packed refinement range cases=%d fidelity=%d\n", cases,
              static_cast<int>(fidelity));
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity") {
    return 2;
  }
  const bool fidelity = mode == "fidelity";
  if (scalar == "s") {
    return Run<float>(false, fidelity);
  }
  if (scalar == "d") {
    return Run<double>(false, fidelity);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false, fidelity);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false, fidelity);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true, fidelity);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true, fidelity);
  }
  return 2;
}
