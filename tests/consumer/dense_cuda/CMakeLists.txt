cmake_minimum_required(VERSION 3.25)

project(ASCCppDenseCudaConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_cuda)

if(NOT ASCCpp_FOUND
   OR NOT ASCCpp_core_FOUND
   OR NOT ASCCpp_expression_FOUND
   OR NOT ASCCpp_dense_FOUND
   OR NOT ASCCpp_core_cuda_FOUND
   OR NOT ASCCpp_dense_cuda_FOUND)
  message(FATAL_ERROR
    "ASCCpp dense_cuda and its exact closure must be available."
  )
endif()
if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()

foreach(_required_target IN ITEMS
    ASC::core
    ASC::expression
    ASC::dense
    ASC::core_cuda
    ASC::dense_cuda
    CUDA::cudart
    CUDA::cublas
)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "The dense_cuda package omitted '${_required_target}'."
    )
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::utilities
    ASC::random
    ASC::sparse
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR
      "The dense_cuda closure imported '${_forbidden_target}'."
    )
  endif()
endforeach()

get_target_property(_links ASC::dense_cuda INTERFACE_LINK_LIBRARIES)
if(NOT _links
   OR NOT "ASC::dense" IN_LIST _links
   OR NOT "ASC::core_cuda" IN_LIST _links)
  message(FATAL_ERROR
    "ASC::dense_cuda must expose dense and core_cuda, found '${_links}'."
  )
endif()
get_target_property(_features ASC::dense_cuda INTERFACE_COMPILE_FEATURES)
if(NOT "cxx_std_20" IN_LIST _features)
  message(FATAL_ERROR
    "ASC::dense_cuda must publish cxx_std_20, found '${_features}'."
  )
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::dense_cuda TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::dense_cuda type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_dense_cuda_consumer main.cc)
set_target_properties(
  asc_cpp_dense_cuda_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(
  asc_cpp_dense_cuda_consumer
  PRIVATE ASC::dense_cuda
)
