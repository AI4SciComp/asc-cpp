include_guard(GLOBAL)

set(ASC_CPP_MODULE_COMPONENTS
  core
  utilities
  expression
  dense
  sparse
  random
)
set(ASC_CPP_FACET_COMPONENTS
  random_dense
  random_sparse
)
set(ASC_CPP_AGGREGATE_COMPONENTS
  cpp
)
set(ASC_CPP_PROVIDER_COMPONENTS
  dense_lapack
  core_cuda
  dense_cuda
  sparse_cuda
  random_cuda
  random_dense_cuda
  random_sparse_cuda
)
set(ASC_CPP_KNOWN_COMPONENTS
  ${ASC_CPP_MODULE_COMPONENTS}
  ${ASC_CPP_FACET_COMPONENTS}
  ${ASC_CPP_AGGREGATE_COMPONENTS}
  ${ASC_CPP_PROVIDER_COMPONENTS}
)
set(ASC_CPP_AVAILABLE_COMPONENTS core)

set(ASC_CPP_COMPONENT_core_DEPENDENCIES)
set(ASC_CPP_COMPONENT_utilities_DEPENDENCIES core)
set(ASC_CPP_COMPONENT_expression_DEPENDENCIES core)
set(ASC_CPP_COMPONENT_dense_DEPENDENCIES core expression)
set(ASC_CPP_COMPONENT_sparse_DEPENDENCIES core expression)
set(ASC_CPP_COMPONENT_random_DEPENDENCIES core)
set(ASC_CPP_COMPONENT_random_dense_DEPENDENCIES random dense)
set(ASC_CPP_COMPONENT_random_sparse_DEPENDENCIES random sparse)
set(ASC_CPP_COMPONENT_cpp_DEPENDENCIES
  core
  utilities
  expression
  dense
  sparse
  random
  random_dense
  random_sparse
)
set(ASC_CPP_COMPONENT_core_cuda_DEPENDENCIES core)
set(ASC_CPP_COMPONENT_dense_lapack_DEPENDENCIES dense)
set(ASC_CPP_COMPONENT_dense_cuda_DEPENDENCIES dense core_cuda)
set(ASC_CPP_COMPONENT_sparse_cuda_DEPENDENCIES sparse core_cuda)
set(ASC_CPP_COMPONENT_random_cuda_DEPENDENCIES random core_cuda)
set(ASC_CPP_COMPONENT_random_dense_cuda_DEPENDENCIES
  random_dense
  random_cuda
  core_cuda
)
set(ASC_CPP_COMPONENT_random_sparse_cuda_DEPENDENCIES
  random_sparse
  random_cuda
  core_cuda
)

set(ASC_CPP_COMPONENT_core_EXPORT_NAME Core)
set(ASC_CPP_COMPONENT_utilities_EXPORT_NAME Utilities)
set(ASC_CPP_COMPONENT_expression_EXPORT_NAME Expression)
set(ASC_CPP_COMPONENT_dense_EXPORT_NAME Dense)
set(ASC_CPP_COMPONENT_sparse_EXPORT_NAME Sparse)
set(ASC_CPP_COMPONENT_random_EXPORT_NAME Random)
set(ASC_CPP_COMPONENT_random_dense_EXPORT_NAME RandomDense)
set(ASC_CPP_COMPONENT_random_sparse_EXPORT_NAME RandomSparse)
set(ASC_CPP_COMPONENT_cpp_EXPORT_NAME Cpp)
set(ASC_CPP_COMPONENT_core_cuda_EXPORT_NAME CoreCuda)
set(ASC_CPP_COMPONENT_dense_lapack_EXPORT_NAME DenseLapack)
set(ASC_CPP_COMPONENT_dense_cuda_EXPORT_NAME DenseCuda)
set(ASC_CPP_COMPONENT_sparse_cuda_EXPORT_NAME SparseCuda)
set(ASC_CPP_COMPONENT_random_cuda_EXPORT_NAME RandomCuda)
set(ASC_CPP_COMPONENT_random_dense_cuda_EXPORT_NAME RandomDenseCuda)
set(ASC_CPP_COMPONENT_random_sparse_cuda_EXPORT_NAME RandomSparseCuda)
