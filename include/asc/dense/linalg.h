#ifndef ASC_DENSE_LINALG_H_
#define ASC_DENSE_LINALG_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/export.h"
#include "asc/dense/view.h"

namespace asc {

enum class MatrixOperation : std::uint8_t {
  kNone = 0,
  kTranspose = 1,
};

template <typename Scalar>
concept DenseLinearAlgebraScalar =
    std::same_as<Scalar, float> || std::same_as<Scalar, double>;

namespace internal_dense_linalg {

ASC_DENSE_EXPORT Status ReferenceCopy(const ExecutionContext& context,
                                      DenseView<const float, 1> source,
                                      DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status ReferenceCopy(const ExecutionContext& context,
                                      DenseView<const float, 2> source,
                                      DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status ReferenceCopy(const ExecutionContext& context,
                                      DenseView<const double, 1> source,
                                      DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status ReferenceCopy(const ExecutionContext& context,
                                      DenseView<const double, 2> source,
                                      DenseView<double, 2> destination);

ASC_DENSE_EXPORT Status ReferenceScal(const ExecutionContext& context,
                                      float alpha,
                                      DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status ReferenceScal(const ExecutionContext& context,
                                      float alpha,
                                      DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status ReferenceScal(const ExecutionContext& context,
                                      double alpha,
                                      DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status ReferenceScal(const ExecutionContext& context,
                                      double alpha,
                                      DenseView<double, 2> destination);

ASC_DENSE_EXPORT Status ReferenceAxpy(const ExecutionContext& context,
                                      float alpha,
                                      DenseView<const float, 1> source,
                                      DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status ReferenceAxpy(const ExecutionContext& context,
                                      float alpha,
                                      DenseView<const float, 2> source,
                                      DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status ReferenceAxpy(const ExecutionContext& context,
                                      double alpha,
                                      DenseView<const double, 1> source,
                                      DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status ReferenceAxpy(const ExecutionContext& context,
                                      double alpha,
                                      DenseView<const double, 2> source,
                                      DenseView<double, 2> destination);

ASC_DENSE_EXPORT Result<float> ReferenceDot(const ExecutionContext& context,
                                            DenseView<const float, 1> left,
                                            DenseView<const float, 1> right);
ASC_DENSE_EXPORT Result<double> ReferenceDot(const ExecutionContext& context,
                                             DenseView<const double, 1> left,
                                             DenseView<const double, 1> right);
ASC_DENSE_EXPORT Result<float> ReferenceNrm2(const ExecutionContext& context,
                                             DenseView<const float, 1> input);
ASC_DENSE_EXPORT Result<double> ReferenceNrm2(const ExecutionContext& context,
                                              DenseView<const double, 1> input);

ASC_DENSE_EXPORT Status ReferenceGemv(const ExecutionContext& context,
                                      MatrixOperation operation, float alpha,
                                      DenseView<const float, 2> matrix,
                                      DenseView<const float, 1> input,
                                      float beta, DenseView<float, 1> output);
ASC_DENSE_EXPORT Status ReferenceGemv(const ExecutionContext& context,
                                      MatrixOperation operation, double alpha,
                                      DenseView<const double, 2> matrix,
                                      DenseView<const double, 1> input,
                                      double beta, DenseView<double, 1> output);

ASC_DENSE_EXPORT Status ReferenceGemm(const ExecutionContext& context,
                                      MatrixOperation left_operation,
                                      MatrixOperation right_operation,
                                      float alpha,
                                      DenseView<const float, 2> left,
                                      DenseView<const float, 2> right,
                                      float beta, DenseView<float, 2> output);
ASC_DENSE_EXPORT Status ReferenceGemm(const ExecutionContext& context,
                                      MatrixOperation left_operation,
                                      MatrixOperation right_operation,
                                      double alpha,
                                      DenseView<const double, 2> left,
                                      DenseView<const double, 2> right,
                                      double beta, DenseView<double, 2> output);

}  // namespace internal_dense_linalg

// Copy, Scal, and Axpy traverse dimension zero fastest for rank-one or
// rank-two operands. Successful computational paths create no storage,
// workspace, temporary, packing, transfer, or synchronization. A failed
// validation may allocate storage for its Status diagnostic.
template <DenseElement SourceElement, DenseLinearAlgebraScalar Scalar,
          std::size_t Rank>
  requires(Rank == 1 || Rank == 2) &&
          std::same_as<std::remove_const_t<SourceElement>, Scalar>
Status Copy(const ExecutionContext& context,
            DenseView<SourceElement, Rank> source,
            DenseView<Scalar, Rank> destination) {
  return internal_dense_linalg::ReferenceCopy(
      context, DenseView<const Scalar, Rank>(source), destination);
}

template <DenseLinearAlgebraScalar Scalar, std::size_t Rank>
  requires(Rank == 1 || Rank == 2)
Status Scal(const ExecutionContext& context, Scalar alpha,
            DenseView<Scalar, Rank> destination) {
  return internal_dense_linalg::ReferenceScal(context, alpha, destination);
}

template <DenseElement SourceElement, DenseLinearAlgebraScalar Scalar,
          std::size_t Rank>
  requires(Rank == 1 || Rank == 2) &&
          std::same_as<std::remove_const_t<SourceElement>, Scalar>
Status Axpy(const ExecutionContext& context, Scalar alpha,
            DenseView<SourceElement, Rank> source,
            DenseView<Scalar, Rank> destination) {
  return internal_dense_linalg::ReferenceAxpy(
      context, alpha, DenseView<const Scalar, Rank>(source), destination);
}

template <DenseElement LeftElement, DenseElement RightElement>
  requires std::same_as<std::remove_const_t<LeftElement>,
                        std::remove_const_t<RightElement>> &&
           DenseLinearAlgebraScalar<std::remove_const_t<LeftElement>>
Result<std::remove_const_t<LeftElement>> Dot(const ExecutionContext& context,
                                             DenseView<LeftElement, 1> left,
                                             DenseView<RightElement, 1> right) {
  using Scalar = std::remove_const_t<LeftElement>;
  return internal_dense_linalg::ReferenceDot(context,
                                             DenseView<const Scalar, 1>(left),
                                             DenseView<const Scalar, 1>(right));
}

template <DenseElement Element>
  requires DenseLinearAlgebraScalar<std::remove_const_t<Element>>
Result<std::remove_const_t<Element>> Nrm2(const ExecutionContext& context,
                                          DenseView<Element, 1> input) {
  using Scalar = std::remove_const_t<Element>;
  return internal_dense_linalg::ReferenceNrm2(
      context, DenseView<const Scalar, 1>(input));
}

template <DenseElement MatrixElement, DenseElement InputElement,
          DenseLinearAlgebraScalar Scalar>
  requires std::same_as<std::remove_const_t<MatrixElement>, Scalar> &&
           std::same_as<std::remove_const_t<InputElement>, Scalar>
Status Gemv(const ExecutionContext& context, MatrixOperation operation,
            Scalar alpha, DenseView<MatrixElement, 2> matrix,
            DenseView<InputElement, 1> input, Scalar beta,
            DenseView<Scalar, 1> output) {
  return internal_dense_linalg::ReferenceGemv(
      context, operation, alpha, DenseView<const Scalar, 2>(matrix),
      DenseView<const Scalar, 1>(input), beta, output);
}

template <DenseElement LeftElement, DenseElement RightElement,
          DenseLinearAlgebraScalar Scalar>
  requires std::same_as<std::remove_const_t<LeftElement>, Scalar> &&
           std::same_as<std::remove_const_t<RightElement>, Scalar>
Status Gemm(const ExecutionContext& context, MatrixOperation left_operation,
            MatrixOperation right_operation, Scalar alpha,
            DenseView<LeftElement, 2> left, DenseView<RightElement, 2> right,
            Scalar beta, DenseView<Scalar, 2> output) {
  return internal_dense_linalg::ReferenceGemm(
      context, left_operation, right_operation, alpha,
      DenseView<const Scalar, 2>(left), DenseView<const Scalar, 2>(right), beta,
      output);
}

}  // namespace asc

#endif  // ASC_DENSE_LINALG_H_
