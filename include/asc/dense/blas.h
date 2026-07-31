#ifndef ASC_DENSE_BLAS_H_
#define ASC_DENSE_BLAS_H_

#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/export.h"
#include "asc/dense/view.h"

namespace asc {

enum class DenseBlasTranspose : std::uint8_t {
  kNone,
  kTranspose,
};

ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const float, 1> source,
                             DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const float, 2> source,
                             DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const double, 1> source,
                             DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status Copy(const ExecutionContext& context,
                             DenseView<const double, 2> source,
                             DenseView<double, 2> destination);

ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, float alpha,
                             DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, float alpha,
                             DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, double alpha,
                             DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status Scal(const ExecutionContext& context, double alpha,
                             DenseView<double, 2> destination);

ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, float alpha,
                             DenseView<const float, 1> source,
                             DenseView<float, 1> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, float alpha,
                             DenseView<const float, 2> source,
                             DenseView<float, 2> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, double alpha,
                             DenseView<const double, 1> source,
                             DenseView<double, 1> destination);
ASC_DENSE_EXPORT Status Axpy(const ExecutionContext& context, double alpha,
                             DenseView<const double, 2> source,
                             DenseView<double, 2> destination);

ASC_DENSE_EXPORT Result<float> Dot(const ExecutionContext& context,
                                   DenseView<const float, 1> left,
                                   DenseView<const float, 1> right);
ASC_DENSE_EXPORT Result<double> Dot(const ExecutionContext& context,
                                    DenseView<const double, 1> left,
                                    DenseView<const double, 1> right);

ASC_DENSE_EXPORT Result<float> Nrm2(const ExecutionContext& context,
                                    DenseView<const float, 1> operand);
ASC_DENSE_EXPORT Result<double> Nrm2(const ExecutionContext& context,
                                     DenseView<const double, 1> operand);

ASC_DENSE_EXPORT Status Gemv(const ExecutionContext& context,
                             DenseBlasTranspose transpose, float alpha,
                             DenseView<const float, 2> matrix,
                             DenseView<const float, 1> input, float beta,
                             DenseView<float, 1> output);
ASC_DENSE_EXPORT Status Gemv(const ExecutionContext& context,
                             DenseBlasTranspose transpose, double alpha,
                             DenseView<const double, 2> matrix,
                             DenseView<const double, 1> input, double beta,
                             DenseView<double, 1> output);

ASC_DENSE_EXPORT Status Gemm(const ExecutionContext& context,
                             DenseBlasTranspose left_transpose,
                             DenseBlasTranspose right_transpose, float alpha,
                             DenseView<const float, 2> left,
                             DenseView<const float, 2> right, float beta,
                             DenseView<float, 2> output);
ASC_DENSE_EXPORT Status Gemm(const ExecutionContext& context,
                             DenseBlasTranspose left_transpose,
                             DenseBlasTranspose right_transpose, double alpha,
                             DenseView<const double, 2> left,
                             DenseView<const double, 2> right, double beta,
                             DenseView<double, 2> output);

}  // namespace asc

#endif  // ASC_DENSE_BLAS_H_
