#include <array>

#include "asc/core/memory.h"
#include "asc/sparse/blas.h"
#include "m4_multi_tu.h"

double M4SpmvFromSecondTranslationUnit() {
  constexpr std::array<asc::extent_t, 2> kShape{1, 2};
  constexpr std::array<asc::nnz_t, 2> kOffsets{0, 2};
  constexpr std::array<asc::index_t, 2> kIndices{0, 1};
  std::array<double, 2> matrix_values{1.0, 2.0};
  auto matrix = asc::CsrView<const double>::Create(
      kOffsets.data(), kIndices.data(), matrix_values.data(), kShape, 2,
      asc::MemorySpace::kHost);
  if (!matrix.ok()) {
    return -1.0;
  }
  std::array<double, 2> input_values{3.0, 4.0};
  std::array<double, 1> output_values{};
  const M4CompileVector<const double> input{input_values.data(), 2};
  M4CompileVector<double> output{output_values.data(), 1};
  const asc::Status status = asc::Spmv(asc::ExecutionContext::Serial(), 1.0,
                                       *matrix, input, 0.0, output);
  return status.ok() ? output_values[0] : -2.0;
}
