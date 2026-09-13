#include <array>
#include <complex>
#include <cstdio>
#include <string_view>

#include "asc/core/execution.h"
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
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          refinement::Fixture<T>& sample, bool fidelity) {
  sample.Prepare(test, provider);
  const auto before_original = sample.original;
  const auto before_x = sample.x;
  const auto before_ferr = sample.ferr;
  const auto before_berr = sample.berr;
  const auto plan = base::Take(base::WithoutAllocation(
      test, [&] { return refinement::Query(provider, sample); }));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.original.data(), before_original.data(),
                             sizeof(before_original)));
  ASC_DENSE_TEST_EQ(test, sample.x, before_x);
  ASC_DENSE_TEST_EQ(test, sample.ferr, before_ferr);
  ASC_DENSE_TEST_EQ(test, sample.berr, before_berr);
  const auto n = sample.system.a.n;
  const auto nrhs = sample.system.nrhs;
  const bool active = n != 0 && nrhs != 0;
  const auto active_n = active ? n : 0;
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kScalar].minimum_entries,
                    (asc::DenseBlasComplex<T> ? 2 : 3) * active_n);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kPivot].minimum_entries,
                    (asc::DenseBlasComplex<T> ? 1 : 2) * active_n);
  const auto native_real = asc::DenseBlasComplex<T> ? active_n : 0;
  ASC_DENSE_TEST_EQ(test, plan.regions[refinement::kReal].minimum_entries,
                    native_real + (active ? 2 * nrhs : 0));
  const auto packed_entries = n * (n + 1) / 2;
  const auto conversion =
      (sample.original_layout == base::kRow ? packed_entries : 0) +
      (sample.system.a.layout == base::kRow ? packed_entries : 0) +
      (sample.system.rhs_layout == base::kRow ? n * nrhs : 0) +
      (sample.solution_layout == base::kRow ? n * nrhs : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[base::kLayout].minimum_entries,
                    active ? conversion : 0);
  refinement::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return refinement::Refine(provider, sample, plan, workspace, report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 0);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                 !report.diagnostic_index.has_value());
  if (fidelity) {
    refinement::Fidelity(test, sample);
  } else {
    sample.Mathematics(test);
  }
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(sample.original.data(), before_original.data(),
                             sizeof(before_original)));
  sample.Padding(test);
  scratch.Guards(test, workspace);
}
template <typename T>
int Run(bool hermitian, bool fidelity) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5, 17, 65}) {
    for (int nrhs : {0, 1, 3}) {
      for (auto triangle : {base::kUpper, base::kLower}) {
        for (unsigned layouts = 0; layouts < 16; ++layouts) {
          const auto layout = [&](unsigned bit) {
            return (layouts & (1U << bit)) != 0 ? base::kRow : base::kColumn;
          };
          const int exponent =
              std::array{-20, 0, 20}[(layouts + static_cast<unsigned>(n)) % 3];
          const int kind =
              (layouts + static_cast<unsigned>(nrhs)) % 2 == 0 ? 0 : 3;
          refinement::Fixture<T> sample(n, nrhs, hermitian, triangle, layout(0),
                                        layout(1), layout(2), layout(3),
                                        exponent, kind);
          std::printf(
              "Packed refinement ordinary n=%d nrhs=%d tri=%d layouts=%u "
              "exponent=%d kind=%d\n",
              n, nrhs, static_cast<int>(triangle), layouts, exponent, kind);
          Case(test, provider, sample, fidelity);
          ++cases;
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 576);
  std::printf("Packed refinement ordinary cases=%d fidelity=%d\n", cases,
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
