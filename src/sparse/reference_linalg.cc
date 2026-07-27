// Keep the owning public header first even though the compiled source has a
// more specific filename.
// clang-format off
#include "asc/sparse/linalg.h"
// clang-format on

#include <cstddef>

#include "asc/core/types.h"
#include "asc/sparse/compressed.h"

namespace asc {
namespace internal_sparse_linalg {
namespace {

template <SparseLinearAlgebraScalar Scalar, typename Reader, typename Writer>
void SpmvImplementation(Scalar alpha, CsrView<const Scalar> matrix,
                        const void* input, Reader read_input, Scalar beta,
                        void* output, Reader read_output, Writer write_output) {
  const nnz_t* const offsets = matrix.outer_offset_data();
  const index_t* const indices = matrix.inner_index_data();
  const Scalar* const values = matrix.value_data();
  for (index_t row = 0; row < matrix.shape()[0]; ++row) {
    Scalar product = 0;
    const nnz_t begin = offsets[static_cast<std::size_t>(row)];
    const nnz_t end = offsets[static_cast<std::size_t>(row + 1)];
    for (nnz_t position = begin; position < end; ++position) {
      const std::size_t stored = static_cast<std::size_t>(position);
      product += values[stored] * read_input(input, indices[stored]);
    }
    const Scalar result =
        beta == static_cast<Scalar>(0)
            ? alpha * product
            : alpha * product + beta * read_output(output, row);
    write_output(output, row, result);
  }
}

}  // namespace

void ReferenceSpmv(float alpha, CsrView<const float> matrix, const void* input,
                   ReadFloat read_input, float beta, void* output,
                   ReadFloat read_output, WriteFloat write_output) {
  SpmvImplementation(alpha, matrix, input, read_input, beta, output,
                     read_output, write_output);
}

void ReferenceSpmv(double alpha, CsrView<const double> matrix,
                   const void* input, ReadDouble read_input, double beta,
                   void* output, ReadDouble read_output,
                   WriteDouble write_output) {
  SpmvImplementation(alpha, matrix, input, read_input, beta, output,
                     read_output, write_output);
}

}  // namespace internal_sparse_linalg
}  // namespace asc
