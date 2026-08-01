#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

#include "../random_cuda/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/sparse/blas.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/providers/cuda.h"

namespace {

using asc_random_cuda_test::TestContext;

struct Fixture {
  asc::ExecutionContext execution;
  asc::SparseCudaContext provider;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<Fixture> MakeFixture() {
  auto execution = asc::CreateCudaExecutionContext(0);
  if (!execution.ok()) {
    return execution.status();
  }
  auto provider = asc::SparseCudaContext::Create(*execution);
  if (!provider.ok()) {
    return provider.status();
  }
  auto resource = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return Fixture{*execution, std::move(*provider), std::move(*resource)};
}

template <typename Element>
Element Make(double real, double imaginary = 0.0) {
  if constexpr (asc::SparseBlasComplex<Element>) {
    return Element{static_cast<typename Element::value_type>(real),
                   static_cast<typename Element::value_type>(imaginary)};
  } else {
    static_cast<void>(imaginary);
    return static_cast<Element>(real);
  }
}

template <typename Element>
Element Conjugate(Element value) {
  if constexpr (asc::SparseBlasComplex<Element>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename Element>
void CheckNear(TestContext& test, Element actual, Element expected) {
  constexpr double kTolerance =
      std::same_as<Element, float> || std::same_as<Element, std::complex<float>>
          ? 3.0e-5
          : 3.0e-12;
  ASC_M7_CUDA_CHECK(
      test, static_cast<double>(std::abs(actual - expected)) <= kTolerance);
}

template <typename Element>
asc::Result<asc::SparseBlasVectorView<Element>> DeviceVector(
    asc::Buffer& buffer, asc::extent_t size, asc::stride_t increment = 1) {
  return asc::SparseBlasVectorView<Element>::Create(
      static_cast<Element*>(buffer.data()), size, increment,
      {buffer.data(), buffer.size(), asc::MemorySpace::kDevice});
}

template <typename Element>
asc::Result<asc::SparseBlasVectorView<const Element>> ConstDeviceVector(
    const asc::Buffer& buffer, asc::extent_t size,
    asc::stride_t increment = 1) {
  return asc::SparseBlasVectorView<const Element>::Create(
      static_cast<const Element*>(buffer.data()), size, increment,
      {buffer.data(), buffer.size(), asc::MemorySpace::kDevice});
}

template <typename Element>
void TestLevelOne(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::index_t, 2> kIndices{0, 2};
  std::array<Element, 2> sparse_values{Make<Element>(1.0, 2.0),
                                       Make<Element>(-2.0, 1.0)};
  std::array<Element, 3> dense_values{Make<Element>(3.0, -1.0),
                                      Make<Element>(9.0, 4.0),
                                      Make<Element>(2.0, 3.0)};
  auto host_sparse = asc::SparseBlasIndexedVectorView<const Element>::Create(
      kIndices.data(), sparse_values.data(), 2, 3,
      {kIndices.data(), sizeof(kIndices), asc::MemorySpace::kHost},
      {sparse_values.data(), sizeof(sparse_values), asc::MemorySpace::kHost});
  ASC_M7_CUDA_CHECK(test, host_sparse.ok());
  if (!host_sparse.ok()) {
    return;
  }
  auto sparse_clone = asc::CudaCloneIndexedVector(
      fixture.provider, *host_sparse, *fixture.resource);
  ASC_M7_CUDA_CHECK(test, sparse_clone.ok());
  if (!sparse_clone.ok() || !sparse_clone->completion.Wait().ok()) {
    return;
  }
  auto sparse = sparse_clone->array.view();
  auto dense_buffer = asc_random_cuda_test::Upload(
      dense_values, *fixture.resource, fixture.execution);
  std::array<Element, 1> zero{Element{}};
  auto result_buffer =
      asc_random_cuda_test::Upload(zero, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test,
                    sparse.ok() && dense_buffer.ok() && result_buffer.ok());
  if (!sparse.ok() || !dense_buffer.ok() || !result_buffer.ok()) {
    return;
  }
  const asc::SparseBlasIndexedVectorView<const Element> const_sparse(*sparse);
  auto dense = ConstDeviceVector<Element>(*dense_buffer, 3);
  auto result = DeviceVector<Element>(*result_buffer, 1);
  ASC_M7_CUDA_CHECK(test, dense.ok() && result.ok());
  if (!dense.ok() || !result.ok()) {
    return;
  }
  auto dot = asc::CudaSparseDot(fixture.provider,
                                asc::SparseBlasConjugation::kConjugated,
                                const_sparse, *dense, *result);
  ASC_M7_CUDA_CHECK(test, dot.ok());
  if (dot.ok()) {
    ASC_M7_CUDA_CHECK(test, dot->Wait().ok());
    auto actual = asc_random_cuda_test::Download<Element>(*result_buffer,
                                                          fixture.execution);
    ASC_M7_CUDA_CHECK(test, actual.ok());
    if (actual.ok()) {
      const Element expected = Conjugate(sparse_values[0]) * dense_values[0] +
                               Conjugate(sparse_values[1]) * dense_values[2];
      CheckNear(test, (*actual)[0], expected);
    }
  }
  auto invalid_conjugation = asc::CudaSparseDot(
      fixture.provider, static_cast<asc::SparseBlasConjugation>(255),
      const_sparse, *dense, *result);
  ASC_M7_CUDA_CHECK(test, !invalid_conjugation.ok());
  if (!invalid_conjugation.ok()) {
    ASC_M7_CUDA_EQ(test, invalid_conjugation.status().code(),
                   asc::ErrorCode::kInvalidArgument);
  }

  std::array<Element, 3> reversed_values{dense_values[2], dense_values[1],
                                         dense_values[0]};
  auto reversed_buffer = asc_random_cuda_test::Upload(
      reversed_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, reversed_buffer.ok());
  if (reversed_buffer.ok()) {
    auto reversed = asc::SparseBlasVectorView<const Element>::Create(
        static_cast<const Element*>(reversed_buffer->data()) + 2, 3, -1,
        {reversed_buffer->data(), reversed_buffer->size(),
         asc::MemorySpace::kDevice});
    ASC_M7_CUDA_CHECK(test, reversed.ok());
    if (reversed.ok()) {
      auto reversed_dot = asc::CudaSparseDot(
          fixture.provider, asc::SparseBlasConjugation::kConjugated,
          const_sparse, *reversed, *result);
      ASC_M7_CUDA_CHECK(test, reversed_dot.ok());
      if (reversed_dot.ok()) {
        ASC_M7_CUDA_CHECK(test, reversed_dot->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *result_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          const Element expected =
              Conjugate(sparse_values[0]) * dense_values[0] +
              Conjugate(sparse_values[1]) * dense_values[2];
          CheckNear(test, (*actual)[0], expected);
        }
      }
    }
  }

  auto writable_dense = DeviceVector<Element>(*dense_buffer, 3);
  ASC_M7_CUDA_CHECK(test, writable_dense.ok());
  if (writable_dense.ok()) {
    auto axpy = asc::CudaSparseAxpy(fixture.provider, Make<Element>(2.0),
                                    const_sparse, *writable_dense);
    ASC_M7_CUDA_CHECK(test, axpy.ok());
    if (axpy.ok()) {
      ASC_M7_CUDA_CHECK(test, axpy->Wait().ok());
      auto actual = asc_random_cuda_test::Download<Element>(*dense_buffer,
                                                            fixture.execution);
      ASC_M7_CUDA_CHECK(test, actual.ok());
      if (actual.ok()) {
        CheckNear(test, (*actual)[0],
                  dense_values[0] + Make<Element>(2.0) * sparse_values[0]);
        CheckNear(test, (*actual)[2],
                  dense_values[2] + Make<Element>(2.0) * sparse_values[1]);
      }
    }
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::array<Element, 2> nan_sparse_values{Make<Element>(nan, nan),
                                           Make<Element>(nan, nan)};
  auto host_nan_sparse =
      asc::SparseBlasIndexedVectorView<const Element>::Create(
          kIndices.data(), nan_sparse_values.data(), 2, 3,
          {kIndices.data(), sizeof(kIndices), asc::MemorySpace::kHost},
          {nan_sparse_values.data(), sizeof(nan_sparse_values),
           asc::MemorySpace::kHost});
  ASC_M7_CUDA_CHECK(test, host_nan_sparse.ok());
  if (host_nan_sparse.ok()) {
    auto nan_sparse_clone = asc::CudaCloneIndexedVector(
        fixture.provider, *host_nan_sparse, *fixture.resource);
    auto zero_axpy_buffer = asc_random_cuda_test::Upload(
        dense_values, *fixture.resource, fixture.execution);
    ASC_M7_CUDA_CHECK(test, nan_sparse_clone.ok() && zero_axpy_buffer.ok());
    if (nan_sparse_clone.ok() && zero_axpy_buffer.ok() &&
        nan_sparse_clone->completion.Wait().ok()) {
      auto nan_sparse = nan_sparse_clone->array.view();
      auto zero_axpy = DeviceVector<Element>(*zero_axpy_buffer, 3);
      ASC_M7_CUDA_CHECK(test, nan_sparse.ok() && zero_axpy.ok());
      if (nan_sparse.ok() && zero_axpy.ok()) {
        auto completion = asc::CudaSparseAxpy(
            fixture.provider, Element{},
            asc::SparseBlasIndexedVectorView<const Element>(*nan_sparse),
            *zero_axpy);
        ASC_M7_CUDA_CHECK(test, completion.ok());
        if (completion.ok() && completion->Wait().ok()) {
          auto actual = asc_random_cuda_test::Download<Element>(
              *zero_axpy_buffer, fixture.execution);
          ASC_M7_CUDA_CHECK(test, actual.ok());
          if (actual.ok()) {
            for (std::size_t position = 0; position < dense_values.size();
                 ++position) {
              CheckNear(test, (*actual)[position], dense_values[position]);
            }
          }
        }
      }
    }
  }

  auto gather_dense_buffer = asc_random_cuda_test::Upload(
      dense_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, gather_dense_buffer.ok());
  if (gather_dense_buffer.ok()) {
    auto gather_dense = ConstDeviceVector<Element>(*gather_dense_buffer, 3);
    ASC_M7_CUDA_CHECK(test, gather_dense.ok());
    if (gather_dense.ok()) {
      auto gather =
          asc::CudaSparseGather(fixture.provider, *gather_dense, *sparse);
      ASC_M7_CUDA_CHECK(test, gather.ok());
      if (gather.ok()) {
        ASC_M7_CUDA_CHECK(test, gather->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            sparse->values(), 2, asc::MemorySpace::kDevice, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], dense_values[0]);
          CheckNear(test, (*actual)[1], dense_values[2]);
        }
      }
    }
  }

  auto gather_zero_dense_buffer = asc_random_cuda_test::Upload(
      dense_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, gather_zero_dense_buffer.ok());
  if (gather_zero_dense_buffer.ok()) {
    auto gather_zero_dense =
        DeviceVector<Element>(*gather_zero_dense_buffer, 3);
    ASC_M7_CUDA_CHECK(test, gather_zero_dense.ok());
    if (gather_zero_dense.ok()) {
      auto gather_zero = asc::CudaSparseGatherZero(fixture.provider,
                                                   *gather_zero_dense, *sparse);
      ASC_M7_CUDA_CHECK(test, gather_zero.ok());
      if (gather_zero.ok()) {
        ASC_M7_CUDA_CHECK(test, gather_zero->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *gather_zero_dense_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], Element{});
          CheckNear(test, (*actual)[2], Element{});
        }
      }
    }
  }

  std::array<Element, 3> scatter_initial{};
  auto scatter_buffer = asc_random_cuda_test::Upload(
      scatter_initial, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, scatter_buffer.ok());
  if (scatter_buffer.ok()) {
    auto scatter_dense = DeviceVector<Element>(*scatter_buffer, 3);
    const asc::SparseBlasIndexedVectorView<const Element> gathered_sparse(
        *sparse);
    ASC_M7_CUDA_CHECK(test, scatter_dense.ok());
    if (scatter_dense.ok()) {
      auto scatter = asc::CudaSparseScatter(fixture.provider, gathered_sparse,
                                            *scatter_dense);
      ASC_M7_CUDA_CHECK(test, scatter.ok());
      if (scatter.ok()) {
        ASC_M7_CUDA_CHECK(test, scatter->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *scatter_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], dense_values[0]);
          CheckNear(test, (*actual)[2], dense_values[2]);
        }
      }
    }
  }
}

template <typename Element>
void TestLevelTwoAndThree(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  constexpr std::array<asc::nnz_t, 3> kOffsets{0, 1, 3};
  constexpr std::array<asc::index_t, 3> kIndices{0, 0, 1};
  std::array<Element, 3> values{Make<Element>(2.0, 1.0),
                                Make<Element>(1.0, -1.0),
                                Make<Element>(3.0, 0.5)};
  auto host_matrix = asc::CsrView<const Element>::Create(
      kOffsets.data(), kIndices.data(), values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, host_matrix.ok());
  if (!host_matrix.ok()) {
    return;
  }
  auto clone =
      asc::CudaCloneCsr(fixture.provider, *host_matrix, *fixture.resource);
  ASC_M7_CUDA_CHECK(test, clone.ok());
  if (!clone.ok() || !clone->completion.Wait().ok()) {
    return;
  }
  auto device_matrix = clone->array.view();
  ASC_M7_CUDA_CHECK(test, device_matrix.ok());
  if (!device_matrix.ok()) {
    return;
  }
  const asc::CsrView<const Element> matrix(*device_matrix);

  std::array<Element, 2> input_values{Make<Element>(1.0, 2.0),
                                      Make<Element>(-1.0, 1.0)};
  std::array<Element, 2> output_values{};
  auto input_buffer = asc_random_cuda_test::Upload(
      input_values, *fixture.resource, fixture.execution);
  auto output_buffer = asc_random_cuda_test::Upload(
      output_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, input_buffer.ok() && output_buffer.ok());
  if (!input_buffer.ok() || !output_buffer.ok()) {
    return;
  }
  auto input = ConstDeviceVector<Element>(*input_buffer, 2);
  auto output = DeviceVector<Element>(*output_buffer, 2);
  ASC_M7_CUDA_CHECK(test, input.ok() && output.ok());
  if (input.ok() && output.ok()) {
    auto spmv = asc::CudaSpmv(fixture.provider, asc::SparseBlasTranspose::kNone,
                              Make<Element>(1.0), matrix, *input, *output);
    ASC_M7_CUDA_CHECK(test, spmv.ok());
    if (spmv.ok()) {
      ASC_M7_CUDA_CHECK(test, spmv->Wait().ok());
      auto actual = asc_random_cuda_test::Download<Element>(*output_buffer,
                                                            fixture.execution);
      ASC_M7_CUDA_CHECK(test, actual.ok());
      if (actual.ok()) {
        CheckNear(test, (*actual)[0], values[0] * input_values[0]);
        CheckNear(test, (*actual)[1],
                  values[1] * input_values[0] + values[2] * input_values[1]);
      }
    }
    auto invalid_transpose = asc::CudaSpmv(
        fixture.provider, static_cast<asc::SparseBlasTranspose>(255),
        Make<Element>(1.0), matrix, *input, *output);
    ASC_M7_CUDA_CHECK(test, !invalid_transpose.ok());
    if (!invalid_transpose.ok()) {
      ASC_M7_CUDA_EQ(test, invalid_transpose.status().code(),
                     asc::ErrorCode::kInvalidArgument);
    }
  }

  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::array<Element, 3> nan_values{Make<Element>(nan, nan),
                                    Make<Element>(nan, nan),
                                    Make<Element>(nan, nan)};
  auto host_nan_matrix = asc::CsrView<const Element>::Create(
      kOffsets.data(), kIndices.data(), nan_values.data(), kShape, 3,
      asc::MemorySpace::kHost);
  ASC_M7_CUDA_CHECK(test, host_nan_matrix.ok());
  if (host_nan_matrix.ok()) {
    auto nan_clone = asc::CudaCloneCsr(fixture.provider, *host_nan_matrix,
                                       *fixture.resource);
    std::array<Element, 2> zero_spmv_values{Make<Element>(7.0),
                                            Make<Element>(9.0)};
    auto zero_spmv_buffer = asc_random_cuda_test::Upload(
        zero_spmv_values, *fixture.resource, fixture.execution);
    ASC_M7_CUDA_CHECK(test, nan_clone.ok() && zero_spmv_buffer.ok());
    if (nan_clone.ok() && zero_spmv_buffer.ok() &&
        nan_clone->completion.Wait().ok()) {
      auto nan_matrix = nan_clone->array.view();
      auto zero_spmv = DeviceVector<Element>(*zero_spmv_buffer, 2);
      ASC_M7_CUDA_CHECK(test, nan_matrix.ok() && zero_spmv.ok());
      if (nan_matrix.ok() && input.ok() && zero_spmv.ok()) {
        auto completion = asc::CudaSpmv(
            fixture.provider, asc::SparseBlasTranspose::kNone, Element{},
            asc::CsrView<const Element>(*nan_matrix), *input, *zero_spmv);
        ASC_M7_CUDA_CHECK(test, completion.ok());
        if (completion.ok() && completion->Wait().ok()) {
          auto actual = asc_random_cuda_test::Download<Element>(
              *zero_spmv_buffer, fixture.execution);
          ASC_M7_CUDA_CHECK(test, actual.ok());
          if (actual.ok()) {
            CheckNear(test, (*actual)[0], zero_spmv_values[0]);
            CheckNear(test, (*actual)[1], zero_spmv_values[1]);
          }
        }
      }
    }
  }

  auto transposed_output_buffer = asc_random_cuda_test::Upload(
      output_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, transposed_output_buffer.ok());
  if (input.ok() && transposed_output_buffer.ok()) {
    auto transposed_output =
        DeviceVector<Element>(*transposed_output_buffer, 2);
    ASC_M7_CUDA_CHECK(test, transposed_output.ok());
    if (transposed_output.ok()) {
      auto spmv = asc::CudaSpmv(
          fixture.provider, asc::SparseBlasTranspose::kConjugateTranspose,
          Make<Element>(1.0), matrix, *input, *transposed_output);
      ASC_M7_CUDA_CHECK(test, spmv.ok());
      if (spmv.ok()) {
        ASC_M7_CUDA_CHECK(test, spmv->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *transposed_output_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          const Element first = Conjugate(values[0]) * input_values[0] +
                                Conjugate(values[1]) * input_values[1];
          const Element second = Conjugate(values[2]) * input_values[1];
          CheckNear(test, (*actual)[0], first);
          CheckNear(test, (*actual)[1], second);
        }
      }
    }
  }

  std::array<Element, 4> rhs_values{Make<Element>(1.0), Make<Element>(2.0),
                                    Make<Element>(3.0), Make<Element>(4.0)};
  std::array<Element, 4> product_values{};
  auto rhs_buffer = asc_random_cuda_test::Upload(rhs_values, *fixture.resource,
                                                 fixture.execution);
  auto product_buffer = asc_random_cuda_test::Upload(
      product_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, rhs_buffer.ok() && product_buffer.ok());
  if (rhs_buffer.ok() && product_buffer.ok()) {
    auto rhs = asc::SparseBlasMatrixView<const Element>::Create(
        static_cast<const Element*>(rhs_buffer->data()), 2, 2,
        asc::SparseBlasLayout::kRowMajor, 2,
        {rhs_buffer->data(), rhs_buffer->size(), asc::MemorySpace::kDevice});
    auto product = asc::SparseBlasMatrixView<Element>::Create(
        static_cast<Element*>(product_buffer->data()), 2, 2,
        asc::SparseBlasLayout::kRowMajor, 2,
        {product_buffer->data(), product_buffer->size(),
         asc::MemorySpace::kDevice});
    ASC_M7_CUDA_CHECK(test, rhs.ok() && product.ok());
    if (rhs.ok() && product.ok()) {
      auto spmm =
          asc::CudaSpmm(fixture.provider, asc::SparseBlasTranspose::kNone,
                        Make<Element>(1.0), matrix, *rhs, *product);
      ASC_M7_CUDA_CHECK(test, spmm.ok());
      if (spmm.ok()) {
        ASC_M7_CUDA_CHECK(test, spmm->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *product_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], values[0] * rhs_values[0]);
          CheckNear(test, (*actual)[3],
                    values[1] * rhs_values[1] + values[2] * rhs_values[3]);
        }
      }
    }
  }

  std::array<Element, 4> column_rhs_values{
      Make<Element>(1.0), Make<Element>(3.0), Make<Element>(2.0),
      Make<Element>(4.0)};
  std::array<Element, 4> column_product_values{};
  auto column_rhs_buffer = asc_random_cuda_test::Upload(
      column_rhs_values, *fixture.resource, fixture.execution);
  auto column_product_buffer = asc_random_cuda_test::Upload(
      column_product_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, column_rhs_buffer.ok() && column_product_buffer.ok());
  if (column_rhs_buffer.ok() && column_product_buffer.ok()) {
    auto column_rhs = asc::SparseBlasMatrixView<const Element>::Create(
        static_cast<const Element*>(column_rhs_buffer->data()), 2, 2,
        asc::SparseBlasLayout::kColumnMajor, 2,
        {column_rhs_buffer->data(), column_rhs_buffer->size(),
         asc::MemorySpace::kDevice});
    auto column_product = asc::SparseBlasMatrixView<Element>::Create(
        static_cast<Element*>(column_product_buffer->data()), 2, 2,
        asc::SparseBlasLayout::kColumnMajor, 2,
        {column_product_buffer->data(), column_product_buffer->size(),
         asc::MemorySpace::kDevice});
    ASC_M7_CUDA_CHECK(test, column_rhs.ok() && column_product.ok());
    if (column_rhs.ok() && column_product.ok()) {
      auto spmm = asc::CudaSpmm(
          fixture.provider, asc::SparseBlasTranspose::kConjugateTranspose,
          Make<Element>(1.0), matrix, *column_rhs, *column_product);
      ASC_M7_CUDA_CHECK(test, spmm.ok());
      if (spmm.ok()) {
        ASC_M7_CUDA_CHECK(test, spmm->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *column_product_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0],
                    Conjugate(values[0]) * column_rhs_values[0] +
                        Conjugate(values[1]) * column_rhs_values[1]);
          CheckNear(test, (*actual)[1],
                    Conjugate(values[2]) * column_rhs_values[1]);
          CheckNear(test, (*actual)[2],
                    Conjugate(values[0]) * column_rhs_values[2] +
                        Conjugate(values[1]) * column_rhs_values[3]);
          CheckNear(test, (*actual)[3],
                    Conjugate(values[2]) * column_rhs_values[3]);
        }
      }
    }
  }

  auto host_triangular =
      asc::SparseBlasTriangularView<Element,
                                    asc::SparseCompressedFormat::kCsr>::
          Create(*host_matrix, asc::SparseBlasTriangle::kLower,
                 asc::SparseBlasDiagonal::kNonUnit);
  ASC_M7_CUDA_CHECK(test, host_triangular.ok());
  if (!host_triangular.ok()) {
    return;
  }
  auto triangular = asc::CudaCloneTriangularCsr(
      fixture.provider, *host_triangular, *fixture.resource);
  ASC_M7_CUDA_CHECK(test, triangular.ok());
  if (!triangular.ok() || !triangular->completion.Wait().ok()) {
    return;
  }
  std::array<Element, 2> solution_values{
      values[0] * Make<Element>(2.0),
      values[1] * Make<Element>(2.0) + values[2] * Make<Element>(-1.0)};
  auto solution_buffer = asc_random_cuda_test::Upload(
      solution_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, solution_buffer.ok());
  if (solution_buffer.ok()) {
    auto solution = DeviceVector<Element>(*solution_buffer, 2);
    ASC_M7_CUDA_CHECK(test, solution.ok());
    if (solution.ok()) {
      auto solve = asc::CudaSparseTriangularSolve(
          fixture.provider, asc::SparseBlasTranspose::kNone, Make<Element>(1.0),
          triangular->array, *solution);
      ASC_M7_CUDA_CHECK(test, solve.ok());
      if (solve.ok()) {
        ASC_M7_CUDA_CHECK(test, solve->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *solution_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], Make<Element>(2.0));
          CheckNear(test, (*actual)[1], Make<Element>(-1.0));
        }
      }
    }
  }

  std::array<Element, 2> transposed_solution_values{
      Conjugate(values[0]) * Make<Element>(2.0) - Conjugate(values[1]),
      -Conjugate(values[2])};
  auto transposed_solution_buffer = asc_random_cuda_test::Upload(
      transposed_solution_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, transposed_solution_buffer.ok());
  if (transposed_solution_buffer.ok()) {
    auto transposed_solution =
        DeviceVector<Element>(*transposed_solution_buffer, 2);
    ASC_M7_CUDA_CHECK(test, transposed_solution.ok());
    if (transposed_solution.ok()) {
      auto solve = asc::CudaSparseTriangularSolve(
          fixture.provider, asc::SparseBlasTranspose::kConjugateTranspose,
          Make<Element>(1.0), triangular->array, *transposed_solution);
      ASC_M7_CUDA_CHECK(test, solve.ok());
      if (solve.ok()) {
        ASC_M7_CUDA_CHECK(test, solve->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *transposed_solution_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], Make<Element>(2.0));
          CheckNear(test, (*actual)[1], Make<Element>(-1.0));
        }
      }
    }
  }

  std::array<Element, 4> multiple_values{
      values[0] * Make<Element>(2.0), values[0] * Make<Element>(1.0),
      values[1] * Make<Element>(2.0) + values[2] * Make<Element>(-1.0),
      values[1] * Make<Element>(1.0) + values[2] * Make<Element>(3.0)};
  auto multiple_buffer = asc_random_cuda_test::Upload(
      multiple_values, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, multiple_buffer.ok());
  if (multiple_buffer.ok()) {
    auto multiple = asc::SparseBlasMatrixView<Element>::Create(
        static_cast<Element*>(multiple_buffer->data()), 2, 2,
        asc::SparseBlasLayout::kRowMajor, 2,
        {multiple_buffer->data(), multiple_buffer->size(),
         asc::MemorySpace::kDevice});
    ASC_M7_CUDA_CHECK(test, multiple.ok());
    if (multiple.ok()) {
      auto solve = asc::CudaSparseTriangularSolveMultiple(
          fixture.provider, asc::SparseBlasTranspose::kNone, Make<Element>(1.0),
          triangular->array, *multiple);
      ASC_M7_CUDA_CHECK(test, solve.ok());
      if (solve.ok()) {
        ASC_M7_CUDA_CHECK(test, solve->Wait().ok());
        auto actual = asc_random_cuda_test::Download<Element>(
            *multiple_buffer, fixture.execution);
        ASC_M7_CUDA_CHECK(test, actual.ok());
        if (actual.ok()) {
          CheckNear(test, (*actual)[0], Make<Element>(2.0));
          CheckNear(test, (*actual)[1], Make<Element>(1.0));
          CheckNear(test, (*actual)[2], Make<Element>(-1.0));
          CheckNear(test, (*actual)[3], Make<Element>(3.0));
        }
      }
    }
  }

  if constexpr (asc::SparseBlasComplex<Element>) {
    using Real = typename Element::value_type;
    const double large = std::same_as<Real, float> ? 1.0e30 : 1.0e200;
    constexpr std::array<asc::extent_t, 2> kLargeShape{1, 1};
    constexpr std::array<asc::nnz_t, 2> kLargeOffsets{0, 1};
    constexpr std::array<asc::index_t, 1> kLargeIndices{0};
    std::array<Element, 1> large_values{Make<Element>(large, large)};
    auto large_matrix = asc::CsrView<const Element>::Create(
        kLargeOffsets.data(), kLargeIndices.data(), large_values.data(),
        kLargeShape, 1, asc::MemorySpace::kHost);
    ASC_M7_CUDA_CHECK(test, large_matrix.ok());
    if (large_matrix.ok()) {
      auto large_triangular =
          asc::SparseBlasTriangularView<Element,
                                        asc::SparseCompressedFormat::kCsr>::
              Create(*large_matrix, asc::SparseBlasTriangle::kLower,
                     asc::SparseBlasDiagonal::kNonUnit);
      ASC_M7_CUDA_CHECK(test, large_triangular.ok());
      if (large_triangular.ok()) {
        auto large_clone = asc::CudaCloneTriangularCsr(
            fixture.provider, *large_triangular, *fixture.resource);
        auto large_rhs_buffer = asc_random_cuda_test::Upload(
            large_values, *fixture.resource, fixture.execution);
        ASC_M7_CUDA_CHECK(test, large_clone.ok() && large_rhs_buffer.ok());
        if (large_clone.ok() && large_rhs_buffer.ok() &&
            large_clone->completion.Wait().ok()) {
          auto large_rhs = DeviceVector<Element>(*large_rhs_buffer, 1);
          ASC_M7_CUDA_CHECK(test, large_rhs.ok());
          if (large_rhs.ok()) {
            auto completion = asc::CudaSparseTriangularSolve(
                fixture.provider, asc::SparseBlasTranspose::kNone,
                Make<Element>(1.0), large_clone->array, *large_rhs);
            ASC_M7_CUDA_CHECK(test, completion.ok());
            if (completion.ok() && completion->Wait().ok()) {
              auto actual = asc_random_cuda_test::Download<Element>(
                  *large_rhs_buffer, fixture.execution);
              ASC_M7_CUDA_CHECK(test, actual.ok());
              if (actual.ok()) {
                CheckNear(test, (*actual)[0], Make<Element>(1.0));
              }
            }
          }
        }
      }
    }
  }
}

void TestInvalidMemory(Fixture& fixture, TestContext& test) {
  constexpr std::array<asc::extent_t, 2> kShape{1, 1};
  constexpr std::array<asc::nnz_t, 2> kOffsets{0, 1};
  constexpr std::array<asc::index_t, 1> kIndices{0};
  std::array<float, 1> values{2.0F};
  auto host = asc::CsrView<const float>::Create(
      kOffsets.data(), kIndices.data(), values.data(), kShape, 1,
      asc::MemorySpace::kHost);
  auto clone = asc::CudaCloneCsr(fixture.provider, *host, *fixture.resource);
  ASC_M7_CUDA_CHECK(test, clone.ok());
  if (!clone.ok() || !clone->completion.Wait().ok()) {
    return;
  }
  auto matrix = clone->array.view();
  std::array<float, 1> input_values{1.0F};
  std::array<float, 1> output_values{7.0F};
  auto input = asc::SparseBlasVectorView<const float>::Create(
      input_values.data(), 1, 1,
      {input_values.data(), sizeof(input_values), asc::MemorySpace::kHost});
  auto output = asc::SparseBlasVectorView<float>::Create(
      output_values.data(), 1, 1,
      {output_values.data(), sizeof(output_values), asc::MemorySpace::kHost});
  ASC_M7_CUDA_CHECK(test, matrix.ok() && input.ok() && output.ok());
  if (matrix.ok() && input.ok() && output.ok()) {
    auto invalid =
        asc::CudaSpmv(fixture.provider, asc::SparseBlasTranspose::kNone, 1.0F,
                      asc::CsrView<const float>(*matrix), *input, *output);
    ASC_M7_CUDA_CHECK(test, !invalid.ok());
    ASC_M7_CUDA_EQ(test, output_values[0], 7.0F);
  }
}

}  // namespace

int main() {
  TestContext test;
  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  auto fixture = MakeFixture();
  ASC_M7_CUDA_CHECK(test, fixture.ok());
  if (!fixture.ok()) {
    return test.Finish();
  }
  TestLevelOne<float>(*fixture, test);
  TestLevelOne<double>(*fixture, test);
  TestLevelOne<std::complex<float>>(*fixture, test);
  TestLevelOne<std::complex<double>>(*fixture, test);
  TestLevelTwoAndThree<float>(*fixture, test);
  TestLevelTwoAndThree<double>(*fixture, test);
  TestLevelTwoAndThree<std::complex<float>>(*fixture, test);
  TestLevelTwoAndThree<std::complex<double>>(*fixture, test);
  TestInvalidMemory(*fixture, test);
  return test.Finish();
}
