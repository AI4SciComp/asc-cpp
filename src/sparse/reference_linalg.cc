#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/linalg.h"

namespace asc::internal_sparse_linalg {
namespace {

template <typename Element>
Status SpmvReferenceImpl(Element alpha, CsrView<const Element> matrix,
                         ReadableVectorDescriptor<Element> input, Element beta,
                         WritableVectorDescriptor<Element> output) {
  for (index_t row = 0; row < matrix.rows(); ++row) {
    auto begin = matrix.OuterOffset(row);
    auto end = matrix.OuterOffset(row + 1);
    if (!begin.ok()) {
      return begin.status();
    }
    if (!end.ok()) {
      return end.status();
    }
    Element product{};
    for (nnz_t position = *begin; position < *end; ++position) {
      auto column = matrix.InnerIndex(position);
      auto value = matrix.AtStored(position);
      if (!column.ok()) {
        return column.status();
      }
      if (!value.ok()) {
        return value.status();
      }
      product += **value * input.read(input.object, *column);
    }
    Element result = alpha * product;
    if (beta != Element{}) {
      result += beta * output.read(output.object, row);
    }
    output.write(output.object, row, result);
  }
  return Status::Ok();
}

}  // namespace

Status SpmvReference(float alpha, CsrView<const float> matrix,
                     ReadableVectorDescriptor<float> input, float beta,
                     WritableVectorDescriptor<float> output) {
  return SpmvReferenceImpl(alpha, matrix, input, beta, output);
}

Status SpmvReference(double alpha, CsrView<const double> matrix,
                     ReadableVectorDescriptor<double> input, double beta,
                     WritableVectorDescriptor<double> output) {
  return SpmvReferenceImpl(alpha, matrix, input, beta, output);
}

}  // namespace asc::internal_sparse_linalg
