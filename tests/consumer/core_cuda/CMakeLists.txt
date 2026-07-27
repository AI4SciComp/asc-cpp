cmake_minimum_required(VERSION 3.25)

project(ASCCppCoreCudaConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core_cuda)

if(NOT ASCCpp_FOUND
   OR NOT ASCCpp_core_FOUND
   OR NOT ASCCpp_core_cuda_FOUND)
  message(FATAL_ERROR
    "ASCCpp core_cuda and its core dependency must be available."
  )
endif()
if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()

set(_expected_available_components
  core
  utilities
  expression
  random
  dense
  sparse
  random_dense
  random_sparse
  cpp
  core_cuda
  dense_cuda
  sparse_cuda
  random_cuda
  random_dense_cuda
  random_sparse_cuda
)
set(_actual_available_components "${ASCCpp_AVAILABLE_COMPONENTS}")
list(SORT _expected_available_components)
list(SORT _actual_available_components)
if(NOT _actual_available_components STREQUAL _expected_available_components)
  message(FATAL_ERROR
    "CUDA-enabled components differ from the frozen contract: "
    "'${ASCCpp_AVAILABLE_COMPONENTS}'."
  )
endif()

foreach(_required_target IN ITEMS ASC::core ASC::core_cuda CUDA::cudart)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "The core_cuda package omitted '${_required_target}'."
    )
  endif()
endforeach()
foreach(_forbidden_target IN ITEMS
    ASC::utilities
    ASC::expression
    ASC::random
    ASC::dense
    ASC::sparse
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
)
  if(TARGET "${_forbidden_target}")
    message(FATAL_ERROR
      "The core_cuda closure imported '${_forbidden_target}'."
    )
  endif()
endforeach()

get_target_property(_links ASC::core_cuda INTERFACE_LINK_LIBRARIES)
if(NOT _links OR NOT "ASC::core" IN_LIST _links)
  message(FATAL_ERROR
    "ASC::core_cuda must expose ASC::core, found '${_links}'."
  )
endif()
get_target_property(_features ASC::core_cuda INTERFACE_COMPILE_FEATURES)
if(NOT "cxx_std_20" IN_LIST _features)
  message(FATAL_ERROR
    "ASC::core_cuda must publish cxx_std_20, found '${_features}'."
  )
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::core_cuda TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::core_cuda type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_core_cuda_consumer main.cc)
set_target_properties(
  asc_cpp_core_cuda_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(
  asc_cpp_core_cuda_consumer
  PRIVATE ASC::core_cuda
)
