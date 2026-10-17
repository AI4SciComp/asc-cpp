#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
#include "asc/dense/providers/lapack_matrix_set.h"
namespace {
template <typename T>
T Value(int real, int imaginary) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (std::is_same_v<T, Real>) {
    return static_cast<T>(real);
  } else {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  }
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  for (auto layout :
       {asc::DenseBlasLayout::kColumnMajor, asc::DenseBlasLayout::kRowMajor}) {
    for (auto part :
         {asc::LapackMatrixPart::kAll, asc::LapackMatrixPart::kUpper,
          asc::LapackMatrixPart::kLower}) {
      std::array<T, 24> data;
      data.fill(T{-7});
      const auto output = asc::DenseBlasMatrixView<T>::Create(
          data.data() + 1, 4, 3, layout, 5,
          {data.data(), sizeof(data), asc::MemorySpace::kHost});
      if (!output.ok()) {
        return false;
      }
      const auto plan = asc::QueryLasetWorkspace(provider, part, *output);
      if (!plan.ok()) {
        return false;
      }
      std::array<T, 12> staged{};
      asc::LapackWorkspace workspace;
      if (layout == asc::DenseBlasLayout::kRowMajor) {
        workspace.regions[static_cast<std::size_t>(
            asc::LapackWorkspaceKind::kLayoutConversion)] = {
            staged.data(), sizeof(staged), asc::MemorySpace::kHost};
      }
      // The queried shape/selection plan is independent of alpha and beta.
      for (int reuse = 0; reuse < 2; ++reuse) {
        const T alpha = Value<T>(2 + reuse, -1 - reuse);
        const T beta = Value<T>(-3 - reuse, 4 + reuse);
        asc::LapackReport report;
        if (!asc::Laset(provider, part, alpha, beta, *output, *plan, workspace,
                        report)
                 .ok() ||
            !report.called_provider || report.native_info ||
            report.output_validity != asc::LapackOutputValidity::kComplete) {
          return false;
        }
        std::array<T, 24> expected;
        expected.fill(T{-7});
        for (asc::extent_t i = 0; i < 4; ++i) {
          for (asc::extent_t j = 0; j < 3; ++j) {
            const bool selected =
                part == asc::LapackMatrixPart::kAll ||
                (part == asc::LapackMatrixPart::kUpper ? i <= j : i >= j);
            if (selected) {
              const auto index =
                  1 + (layout == asc::DenseBlasLayout::kRowMajor ? i * 5 + j
                                                                 : j * 5 + i);
              expected[static_cast<std::size_t>(index)] = i == j ? beta : alpha;
            }
          }
        }
        if (data != expected) {
          return false;
        }
      }
    }
  }
  return true;
}
}  // namespace
int main() {
  const auto provider =
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial());
  if (!provider.ok()) {
    return 1;
  }
  if (!Run<float>(*provider) || !Run<double>(*provider) ||
      !Run<std::complex<float>>(*provider) ||
      !Run<std::complex<double>>(*provider)) {
    return 1;
  }
  std::puts(
      "LASET: four scalars, all/upper/lower, padded independent layouts, "
      "changed-value plan reuse and absent native INFO passed.");
  return 0;
}
