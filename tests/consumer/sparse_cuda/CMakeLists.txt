cmake_minimum_required(VERSION 3.25)

project(ASCCppSparseCudaConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse_cuda)

foreach(_required_target IN ITEMS
    ASC::core
    ASC::expression
    ASC::sparse
    ASC::core_cuda
    ASC::sparse_cuda
    CUDA::cudart
    CUDA::cusparse
)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "The sparse_cuda package omitted '${_required_target}'."
    )
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::utilities
    ASC::random
    ASC::dense
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
    ASC::dense_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR
      "The sparse_cuda closure imported '${_forbidden_target}'."
    )
  endif()
endforeach()

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()
get_target_property(_links ASC::sparse_cuda INTERFACE_LINK_LIBRARIES)
if(NOT "ASC::sparse" IN_LIST _links
   OR NOT "ASC::core_cuda" IN_LIST _links)
  message(FATAL_ERROR
    "ASC::sparse_cuda has an invalid direct ASC closure: '${_links}'."
  )
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::sparse_cuda TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::sparse_cuda type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_sparse_cuda_consumer main.cc)
target_compile_features(asc_cpp_sparse_cuda_consumer PRIVATE cxx_std_20)
set_target_properties(
  asc_cpp_sparse_cuda_consumer
  PROPERTIES CXX_EXTENSIONS OFF
)
target_link_libraries(
  asc_cpp_sparse_cuda_consumer
  PRIVATE ASC::sparse_cuda
)
