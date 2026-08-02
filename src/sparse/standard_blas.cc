#include <complex>  // NOLINT(misc-include-cleaner)

#include "asc/sparse/blas.h"  // NOLINT(misc-include-cleaner)

namespace asc {

#define ASC_INSTANTIATE_LEVEL_ONE(Element)                                  \
  template Result<Element> SparseDot(                                       \
      const ExecutionContext&, SparseBlasConjugation,                       \
      SparseBlasIndexedVectorView<const Element>,                           \
      SparseBlasVectorView<const Element>);                                 \
  template Status SparseAxpy(const ExecutionContext&, Element,              \
                             SparseBlasIndexedVectorView<const Element>,    \
                             SparseBlasVectorView<Element>);                \
  template Status SparseGather(const ExecutionContext&,                     \
                               SparseBlasVectorView<const Element>,         \
                               SparseBlasIndexedVectorView<Element>);       \
  template Status SparseGatherZero(const ExecutionContext&,                 \
                                   SparseBlasVectorView<Element>,           \
                                   SparseBlasIndexedVectorView<Element>);   \
  template Status SparseScatter(const ExecutionContext&,                    \
                                SparseBlasIndexedVectorView<const Element>, \
                                SparseBlasVectorView<Element>)

#define ASC_INSTANTIATE_MATRIX(Element, Format)                               \
  template Status Spmv(const ExecutionContext&, SparseBlasTranspose, Element, \
                       CompressedSparseView<const Element, Format>,           \
                       SparseBlasVectorView<const Element>,                   \
                       SparseBlasVectorView<Element>);                        \
  template Status Spmm(const ExecutionContext&, SparseBlasTranspose, Element, \
                       CompressedSparseView<const Element, Format>,           \
                       SparseBlasMatrixView<const Element>,                   \
                       SparseBlasMatrixView<Element>);                        \
  template Status SparseTriangularSolve(                                      \
      const ExecutionContext&, SparseBlasTranspose, Element,                  \
      SparseBlasTriangularView<Element, Format>,                              \
      SparseBlasVectorView<Element>);                                         \
  template Status SparseTriangularSolveMultiple(                              \
      const ExecutionContext&, SparseBlasTranspose, Element,                  \
      SparseBlasTriangularView<Element, Format>,                              \
      SparseBlasMatrixView<Element>)

ASC_INSTANTIATE_LEVEL_ONE(float);
ASC_INSTANTIATE_LEVEL_ONE(double);
ASC_INSTANTIATE_LEVEL_ONE(std::complex<float>);
ASC_INSTANTIATE_LEVEL_ONE(std::complex<double>);

ASC_INSTANTIATE_MATRIX(float, SparseCompressedFormat::kCsr);
ASC_INSTANTIATE_MATRIX(float, SparseCompressedFormat::kCsc);
ASC_INSTANTIATE_MATRIX(double, SparseCompressedFormat::kCsr);
ASC_INSTANTIATE_MATRIX(double, SparseCompressedFormat::kCsc);
ASC_INSTANTIATE_MATRIX(std::complex<float>, SparseCompressedFormat::kCsr);
ASC_INSTANTIATE_MATRIX(std::complex<float>, SparseCompressedFormat::kCsc);
ASC_INSTANTIATE_MATRIX(std::complex<double>, SparseCompressedFormat::kCsr);
ASC_INSTANTIATE_MATRIX(std::complex<double>, SparseCompressedFormat::kCsc);

#undef ASC_INSTANTIATE_MATRIX
#undef ASC_INSTANTIATE_LEVEL_ONE

}  // namespace asc
